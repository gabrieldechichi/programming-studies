#include "../animation.h"
#include "../animation_system.h"
#include "../assets.h"
#include "../camera.h"
#include "../conversation_system.h"
#include "../game.h"
#include "../gameplay_lib.c"
#include "../lib/array.h"
#include "../lib/audio.h"
#include "../lib/lipsync.h"
#include "../lib/math.h"
#include "../lib/memory.h"
#include "../lib/queue.h"
#include "../lib/random.h"
#include "../lib/typedefs.h"
#include "../platform.h"
#include "../renderer.h"
#include "../stats.h"
#include "../tts_system.h"
#include "../vendor/cglm/vec3.h"
#include "../vendor/stb/stb_image.h"

// Define slice types
slice_define(AnimationAsset_Handle);
typedef Animation *AnimationPtr;
slice_define(AnimationPtr);

typedef struct {
  AnimationAsset_Handle handle;
  Animation *animation;
  bool32 is_loaded;
} AnimationAssetRef;

slice_define(AnimationAssetRef);

internal PhonemeBlendshapeDefinition phoneme_blendshape_definitions[] = {
    {"A", "vrc.v_aa"}, {"I", "vrc.v_ih"}, {"U", "vrc.v_ou"},
    {"E", "vrc.v_e"},  {"O", "vrc.v_oh"},
};

#define ANIMATIONS_CAP 64

global char *idle_animations[] = {
    // "anya/Anya - Idle 1.hasset",
    "anya/Anya - Idle 2.hasset",
};

global char *listening_animations[] = {
    "anya/Anya - Idle 1.hasset",
    "anya/Anya - Idle 2.hasset",
    "anya/Anya - Look Around.hasset",
};

global char *thinking_animations[] = {
    "anya/Anya - Thinking arms behind back 2.hasset",
    "anya/Anya - Thinking hands on hips lean right.hasset",
};

global char *speaking_animations[] = {
    "anya/Anya - Speaking 01.hasset",
    "anya/Anya - Speaking 02.hasset",
    "anya/Anya - Speaking 03.hasset",
};

global char *hands_animations[] = {
    "anya/Anya - Hands.hasset",
};

typedef struct {
  EMOTION_TAGS emotion;
  f32 predicted_playback_time;
  char phrase[1024];
} EmotionQueueItem;

queue_define(EmotionQueueItem);

typedef struct {
  AnimationAssetRef_Slice neutral_animations;
  AnimationAssetRef_Slice happy_animations;
  AnimationAssetRef_Slice sad_animations;
  AnimationAssetRef_Slice angry_animations;
  AnimationAssetRef_Slice surprised_animations;
  AnimationAssetRef_Slice scared_animations;
  AnimationAssetRef_Slice serious_animations;
  AnimationAssetRef_Slice smug_animations;

  EMOTION_TAGS current_emotion;
  u32 current_anim_idx;
  f32 next_face_switch_time;
  PCG32_State rng;
  EmotionQueueItem_Queue emotion_queue;
  TTSQueueItem *last_added_item;

  AnimationAssetRef blink_animation;
  f32 next_blink_time;
  b32 is_double_blink;
  f32 double_blink_delay;
  PCG32_State blink_rng;

  f32 next_idle_conversation_time;
  PCG32_State idle_conversation_rng;
  // b32 has_triggered_idle_conversation;

  f32 reset_to_neutral_time;
  b32 pending_neutral_reset;
} CharacterEmotions;

global char *face_animations[] = {
    "anya/Anya - Face Neutral 1 - BS.hasset",
    "anya/Anya - Face Neutral 2 - BS.hasset",
    "anya/Anya - Face Angry - BS.hasset",
    "anya/Anya - Face Happy - BS.hasset",
    "anya/Anya - Face Sad 1 - BS.hasset",
    "anya/Anya - Face Sad 2 - BS.hasset",
    "anya/Anya - Face Sad 3 - BS.hasset",
    "anya/Anya - Face Scared - BS.hasset",
    "anya/Anya - Face Serious - BS.hasset",
    "anya/Anya - Face Smile - BS.hasset",
    "anya/Anya - Face Smug 1 - BS.hasset",
    "anya/Anya - Face Smug 2 - BS.hasset",
    "anya/Anya - Face Surprised - BS.hasset",
    "anya/Anya - Face Surprised Scared - BS.hasset",
};

global char *greeting_animations[] = {
    "anya/Anya - TPose.hasset",
};

EMOTION_TAGS animation_path_to_emotion_tag(const char *path) {
  if (str_contains(path, "Neutral")) {
    return EMOTION_TAGS_NEUTRAL;
  } else if (str_contains(path, "Happy")) {
    return EMOTION_TAGS_HAPPY;
  } else if (str_contains(path, "Sad")) {
    return EMOTION_TAGS_SAD;
  } else if (str_contains(path, "Angry")) {
    return EMOTION_TAGS_ANGRY;
  } else if (str_contains(path, "Surprised")) {
    return EMOTION_TAGS_SURPRISED;
  } else if (str_contains(path, "Scared")) {
    return EMOTION_TAGS_SCARED;
  } else if (str_contains(path, "Serious")) {
    return EMOTION_TAGS_SERIOUS;
  } else if (str_contains(path, "Smug")) {
    return EMOTION_TAGS_SMUG;
  } else if (str_contains(path, "Smile")) {
    return EMOTION_TAGS_HAPPY;
  }
  return EMOTION_TAGS_NEUTRAL;
}

AnimationAssetRef_Slice *
character_emotions_get_animation_slice(CharacterEmotions *emotions,
                                       EMOTION_TAGS emotion_tag) {
  switch (emotion_tag) {
  case EMOTION_TAGS_NEUTRAL:
    return &emotions->neutral_animations;
  case EMOTION_TAGS_HAPPY:
    return &emotions->happy_animations;
  case EMOTION_TAGS_SAD:
    return &emotions->sad_animations;
  case EMOTION_TAGS_ANGRY:
    return &emotions->angry_animations;
  case EMOTION_TAGS_SURPRISED:
    return &emotions->surprised_animations;
  case EMOTION_TAGS_SCARED:
    return &emotions->scared_animations;
  case EMOTION_TAGS_SERIOUS:
    return &emotions->serious_animations;
  case EMOTION_TAGS_SMUG:
    return &emotions->smug_animations;
  default:
    return &emotions->neutral_animations;
  }
}

void character_emotions_request_animation(CharacterEmotions *emotions,
                                          EMOTION_TAGS emotion_tag,
                                          const char *animation_path,
                                          AssetSystem *asset_system,
                                          GameContext *ctx) {
  AnimationAssetRef_Slice *slice =
      character_emotions_get_animation_slice(emotions, emotion_tag);

  AnimationAssetRef ref = {0};
  ref.handle = asset_request(AnimationAsset, asset_system, ctx, animation_path);
  ref.animation = NULL;
  ref.is_loaded = false;

  slice_append(*slice, ref);
}

void character_emotions_update_loading(CharacterEmotions *emotions,
                                       AssetSystem *asset_system,
                                       Model3DData *model_data,
                                       GameContext *ctx) {
  AnimationAssetRef_Slice *all_slices[] = {
      &emotions->neutral_animations,   &emotions->happy_animations,
      &emotions->sad_animations,       &emotions->angry_animations,
      &emotions->surprised_animations, &emotions->scared_animations,
      &emotions->serious_animations,   &emotions->smug_animations,
  };

  for (u32 i = 0; i < ARRAY_SIZE(all_slices); i++) {
    AnimationAssetRef_Slice *slice = all_slices[i];
    for (u32 j = 0; j < slice->len; j++) {
      AnimationAssetRef *ref = &slice->items[j];
      if (!ref->is_loaded && asset_is_ready(asset_system, ref->handle)) {
        AnimationAsset *anim_asset =
            asset_get_data(AnimationAsset, asset_system, ref->handle);
        ref->animation =
            animation_from_asset(anim_asset, model_data, &ctx->allocator);
        ref->is_loaded = true;
      }
    }
  }

  // Load blink animation
  if (!emotions->blink_animation.is_loaded &&
      asset_is_ready(asset_system, emotions->blink_animation.handle)) {
    AnimationAsset *anim_asset = asset_get_data(
        AnimationAsset, asset_system, emotions->blink_animation.handle);
    emotions->blink_animation.animation =
        animation_from_asset(anim_asset, model_data, &ctx->allocator);
    emotions->blink_animation.is_loaded = true;
  }
}

AnimationAssetRef_Slice *
character_emotions_get_current_animations(CharacterEmotions *emotions) {
  return character_emotions_get_animation_slice(emotions,
                                                emotions->current_emotion);
}

void character_emotions_update_detection(CharacterEmotions *emotions,
                                         TextToSpeechSystem *tts_system,
                                         AudioState *audio_system,
                                         GameContext *ctx, const GameTime *time,
                                         AnimatedEntity *animated,
                                         u32 face_layer_index) {
  UNUSED(ctx);

  // Queue new emotions when TTS has a new emotion ready
  if (tts_current_emotion_ready(tts_system)) {
    TTSQueueItem *head_item =
        &tts_system->tts_queue.items[tts_system->tts_queue.head];

    // Only queue if this is a new TTS item (different predicted time)
    if (head_item != emotions->last_added_item) {
      f32 predicted_time = time->now + head_item->predicted_playback_start_time;
      EMOTION_TAGS tts_emotion = tts_get_current_emotion(tts_system);

      // Queue the emotion with its predicted playback time
      if (!queue_is_full(emotions->emotion_queue)) {
        EmotionQueueItem emotion_item = {
            .emotion = tts_emotion,
            .predicted_playback_time = predicted_time,
        };
        memcpy_safe(emotion_item.phrase, head_item->text.value,
                    head_item->text.len + 1);
        queue_enqueue(emotions->emotion_queue, emotion_item);

        extern const char *emotion_tags[];
        LOG_INFO("Queued emotion % with predicted time %",
                 FMT_STR(emotion_tags[tts_emotion]), FMT_FLOAT(predicted_time));
      }

      emotions->last_added_item = head_item;
    }
  }

  // Process queued emotions when their predicted playback time is reached
  if (!queue_is_empty(emotions->emotion_queue)) {
    EmotionQueueItem *emotion_item =
        queue_peek_head_ptr(emotions->emotion_queue);

    // Check if this emotion's predicted playback time has been reached
    if (time->now >= emotion_item->predicted_playback_time) {
      // Cancel any pending neutral reset since we're playing a new emotion
      emotions->pending_neutral_reset = false;

      // Play the animation for this emotion
      emotions->current_emotion = emotion_item->emotion;

      // Change face animation to match new emotion
      AnimationAssetRef_Slice *current_animations =
          character_emotions_get_current_animations(emotions);
      if (current_animations->len > 0 &&
          animated->layers.len > face_layer_index) {
        // Find first loaded animation for this emotion
        for (u32 j = 0; j < current_animations->len; j++) {
          if (current_animations->items[j].is_loaded) {
            emotions->current_anim_idx = j;
            Animation *anim = current_animations->items[j].animation;

            animated_entity_play_animation_on_layer(animated, face_layer_index,
                                                    anim, 0.1f, 1.0f, false);

            LOG_INFO("Playing emotion at predicted time % % - switched to "
                     "animation: %",
                     FMT_FLOAT(emotion_item->predicted_playback_time),
                     FMT_STR(emotion_tags[emotion_item->emotion]),
                     FMT_STR(anim->name.value));
            break;
          }
        }
      }

      emotions->next_face_switch_time = 0.0f;

      // Remove this emotion from the queue
      EmotionQueueItem dummy;
      queue_dequeue(emotions->emotion_queue, &dummy);
    }
  }
}

void character_emotions_update_blinking(CharacterEmotions *emotions,
                                        const GameTime *time,
                                        AnimatedEntity *animated,
                                        u32 blink_layer_index) {
  if (!emotions->blink_animation.is_loaded) {
    return;
  }

  if (time->now >= emotions->next_blink_time) {
    if (!emotions->is_double_blink) {
      // Start first blink or single blink
      animated_entity_play_animation_on_layer(
          animated, blink_layer_index, emotions->blink_animation.animation,
          0.0f, 1.0f, false);

      // 30% chance for double blink
      if (pcg32_next_f32(&emotions->blink_rng) < 0.3f) {
        emotions->is_double_blink = true;
        emotions->double_blink_delay = time->now + 0.3f; // 300ms between blinks
      } else {
        // Schedule next single blink (1-4 seconds)
        f32 next_interval =
            pcg32_next_f32_range(&emotions->blink_rng, 1.0, 3.0f);
        emotions->next_blink_time = time->now + next_interval;
      }
    } else if (time->now >= emotions->double_blink_delay) {
      // Second blink of double blink
      animated_entity_play_animation_on_layer(
          animated, blink_layer_index, emotions->blink_animation.animation,
          0.0f, 1.0f, false);

      emotions->is_double_blink = false;

      // Schedule next blink (1-4 seconds)
      f32 next_interval = pcg32_next_f32_range(&emotions->blink_rng, 1.0, 3.0f);
      emotions->next_blink_time = time->now + next_interval;
    }
  }
}

void character_emotions_update_neutral_reset(CharacterEmotions *emotions,
                                             ConversationSystem *conversation,
                                             const GameTime *time,
                                             AnimatedEntity *animated,
                                             u32 face_layer_index) {
  // If Anya is currently speaking, cancel any pending reset and don't schedule
  // new ones
  if (conversation_is_ai_speaking(conversation)) {
    emotions->pending_neutral_reset = false;
    return;
  }

  // If Anya just finished speaking and has a non-neutral emotion, schedule
  // reset
  b32 should_reset_to_neutral =
      !emotions->pending_neutral_reset &&
      emotions->current_emotion != EMOTION_TAGS_NEUTRAL;
  should_reset_to_neutral &= emotions->current_emotion != EMOTION_TAGS_SAD;
  should_reset_to_neutral &= emotions->current_emotion != EMOTION_TAGS_SERIOUS;

  if (should_reset_to_neutral) {
    f32 reset_delay =
        emotions->current_emotion == EMOTION_TAGS_HAPPY ? 1.5f : 3.0f;
    emotions->reset_to_neutral_time = time->now + reset_delay;
    emotions->pending_neutral_reset = true;
    extern const char *emotion_tags[];
    LOG_INFO("Scheduled neutral reset for time % (current emotion: %)",
             FMT_FLOAT(emotions->reset_to_neutral_time),
             FMT_STR(emotion_tags[emotions->current_emotion]));
  }

  // Execute the reset if it's time
  if (emotions->pending_neutral_reset &&
      time->now >= emotions->reset_to_neutral_time) {
    emotions->current_emotion = EMOTION_TAGS_NEUTRAL;
    emotions->pending_neutral_reset = false;

    AnimationAssetRef_Slice *neutral_animations =
        character_emotions_get_animation_slice(emotions, EMOTION_TAGS_NEUTRAL);

    if (neutral_animations->len > 0 &&
        animated->layers.len > face_layer_index) {
      for (u32 j = 0; j < neutral_animations->len; j++) {
        if (neutral_animations->items[j].is_loaded) {
          emotions->current_anim_idx = j;
          Animation *anim = neutral_animations->items[j].animation;

          animated_entity_play_animation_on_layer(animated, face_layer_index,
                                                  anim, 0.3f, 1.0f, false);

          extern const char *emotion_tags[];
          LOG_INFO("Reset to neutral expression: %", FMT_STR(anim->name.value));
          break;
        }
      }
    }
  }
}

void character_emotions_update_idle_conversation(
    CharacterEmotions *emotions, ConversationSystem *conversation,
    const GameTime *time, GameContext *ctx) {
  SpeechToTextSystem *stt_system = &conversation->stt_system;

  // Reset trigger flag when user starts speaking
  if (stt_system->is_actively_recording) {
    // emotions->has_triggered_idle_conversation = false;
  }

  // Don't trigger idle conversations if:
  // - User is speaking
  // - AI is speaking
  // - System is processing
  // - We already triggered one for this idle period
  if (stt_system->is_actively_recording ||
      conversation_is_ai_speaking(conversation) ||
      conversation_is_processing(conversation)) {
    emotions->next_idle_conversation_time = 0.0;
    return;
  }

  // Generate random idle threshold if not set
  if (emotions->next_idle_conversation_time == 0.0f) {
    emotions->next_idle_conversation_time =
        time->now +
        pcg32_next_f32_range(&emotions->idle_conversation_rng, 10.0, 14.0);
  }

  // Check if silence duration has exceeded our threshold
  if (time->now >= emotions->next_idle_conversation_time) {
    // Add internal thought message to conversation history
    String idle_message =
        STR_FROM_CSTR("The person in the shiny box hasn't spoken in a while... "
                      "I should continue the conversation!");
    conversation_history_add_assistant_message(&conversation->history,
                                               idle_message, &ctx->allocator);

    // Trigger conversation request
    send_conversation_request(conversation, ctx);

    // Mark as triggered and reset timer
    emotions->next_idle_conversation_time = 0.0f;

    LOG_INFO("Triggered idle conversation after % seconds of silence",
             FMT_FLOAT(stt_system->silence_duration));
  }
}

typedef enum {
  CHARACTER_STATE_IDLE,
  CHARACTER_STATE_LISTENING,
  CHARACTER_STATE_THINKING,
  CHARACTER_STATE_SPEAKING,
  CHARACTER_STATE_GREETING
} CharacterStateType;

typedef struct {
  AnimationPtr_Slice animations;
  u32 current_anim_idx;
  b32 did_enter;
  f32 next_switch_time;
  PCG32_State rng;
} StateLogic_Loop;

typedef struct {
  AnimationPtr_Slice animations;
  u32 current_anim_idx;
  b32 did_enter;
  b32 did_finish_playing;
  PCG32_State rng;
} StateLogic_OneShot;

typedef struct {
  CharacterStateType type;
  union {
    StateLogic_Loop idle, listening, thinking, speaking;
    StateLogic_OneShot greeting;
  };
} CharacterStateLogic;

typedef struct {
  mat4 model_matrix;
  SkinnedModel skinned_model;
  AnimatedEntity animated;
  LipsyncBlendshapeController face_blendshapes;
  LipSyncContext face_lipsync;
} Character;

typedef struct {
  AssetSystem asset_system;
  AudioState audio_system;
  GameInput input;

  ConversationSystem conversation_system;

  // assets
  Model3DData_Handle model_asset_handle;
  MaterialAsset_Handle *material_asset_handles;
  u32 material_count;
  LipSyncProfile_Handle lipsync_profile_handle;
  Model3DData *model_data;
  LipSyncProfile *lipsync_profile;
  Material_Slice materials;

  // 3D scene data
  DirectionalLightBlock directional_lights;
  PointLightsBlock point_lights;
  Camera camera;

  Texture_Handle skybox_texture_handle;
  b32 skybox_material_ready;

  AnimationAsset_Handle_Slice idle_anim_asset_handles;
  AnimationAsset_Handle_Slice listening_anim_asset_handles;
  AnimationAsset_Handle_Slice thinking_anim_asset_handles;
  AnimationAsset_Handle_Slice speaking_anim_asset_handles;
  AnimationAsset_Handle_Slice greeting_anim_asset_handles;
  AnimationAsset_Handle_Slice hands_anim_asset_handles;

  Character character;
  CharacterStateLogic *character_state;

  CharacterStateLogic idle_state;
  CharacterStateLogic listening_state;
  CharacterStateLogic thinking_state;
  CharacterStateLogic speaking_state;
  CharacterStateLogic greeting_state;
  CharacterStateLogic hands_state;

  u32 hands_layer_index;
  u32 face_layer_index;
  u32 blink_layer_index;

  CharacterEmotions character_emotions;

  WavFile_Handle background_music_handle;
  WavFile *background_music_file;
  b32 background_music_loaded;
  b32 background_music_playing;

  bool32 initial_greeting_sent;
  bool32 did_wave;

  GameStats stats;
} GymState;

typedef struct {
  u32 type_id;
} GymState_ReflectionData;

global const GymState_ReflectionData GymState_TYPE = {.type_id = 1};
global b32 can_start = false;

void gym_init(GameMemory *memory) {
  can_start = true;

  GameContext *ctx = &memory->ctx;

  GymState *gym_state = NULL;
  gym_state = ALLOC(&ctx->allocator, GymState);
  ctx_set_user_data(ctx, GymState, gym_state);

  gym_state->input = input_init();
  gym_state->audio_system = audio_init(ctx);
  gym_state->asset_system = asset_system_init(&ctx->allocator, 512);
  gym_state->conversation_system =
      conversation_system_init(ctx, &gym_state->audio_system);

  gym_state->initial_greeting_sent = false;

  gym_state->idle_state = (CharacterStateLogic){
      .type = CHARACTER_STATE_IDLE,
      .idle =
          {
              .animations = slice_new_ALLOC(&ctx->allocator, AnimationPtr,
                                            ANIMATIONS_CAP),
              .rng = pcg32_new(45678, 4),
          },
  };

  gym_state->listening_state = (CharacterStateLogic){
      .type = CHARACTER_STATE_LISTENING,
      .listening =
          {
              .animations = slice_new_ALLOC(&ctx->allocator, AnimationPtr,
                                            ANIMATIONS_CAP),
              .rng = pcg32_new(45678, 4),
          },
  };

  gym_state->thinking_state = (CharacterStateLogic){
      .type = CHARACTER_STATE_THINKING,
      .thinking =
          {
              .animations = slice_new_ALLOC(&ctx->allocator, AnimationPtr,
                                            ANIMATIONS_CAP),
              .rng = pcg32_new(45678, 4),
          },
  };

  gym_state->speaking_state = (CharacterStateLogic){
      .type = CHARACTER_STATE_SPEAKING,
      .speaking =
          {
              .animations = slice_new_ALLOC(&ctx->allocator, AnimationPtr,
                                            ANIMATIONS_CAP),
              .rng = pcg32_new(45678, 4),
          },
  };

  gym_state->greeting_state = (CharacterStateLogic){
      .type = CHARACTER_STATE_GREETING,
      .greeting =
          {
              .animations = slice_new_ALLOC(&ctx->allocator, AnimationPtr,
                                            ANIMATIONS_CAP),
              .rng = pcg32_new(45678, 4),
          },
  };

  gym_state->hands_state = (CharacterStateLogic){
      .type = CHARACTER_STATE_IDLE,
      .idle =
          {
              .animations = slice_new_ALLOC(&ctx->allocator, AnimationPtr,
                                            ANIMATIONS_CAP),
              .rng = pcg32_new(45678, 4),
          },
  };

  // we want this to start as null
  gym_state->character_state = NULL;

  // Initialize character emotions
  CharacterEmotions *emotions = &gym_state->character_emotions;
  emotions->neutral_animations =
      slice_new_ALLOC(&ctx->allocator, AnimationAssetRef, ANIMATIONS_CAP);
  emotions->happy_animations =
      slice_new_ALLOC(&ctx->allocator, AnimationAssetRef, ANIMATIONS_CAP);
  emotions->sad_animations =
      slice_new_ALLOC(&ctx->allocator, AnimationAssetRef, ANIMATIONS_CAP);
  emotions->angry_animations =
      slice_new_ALLOC(&ctx->allocator, AnimationAssetRef, ANIMATIONS_CAP);
  emotions->surprised_animations =
      slice_new_ALLOC(&ctx->allocator, AnimationAssetRef, ANIMATIONS_CAP);
  emotions->scared_animations =
      slice_new_ALLOC(&ctx->allocator, AnimationAssetRef, ANIMATIONS_CAP);
  emotions->serious_animations =
      slice_new_ALLOC(&ctx->allocator, AnimationAssetRef, ANIMATIONS_CAP);
  emotions->smug_animations =
      slice_new_ALLOC(&ctx->allocator, AnimationAssetRef, ANIMATIONS_CAP);

  emotions->current_emotion = EMOTION_TAGS_NEUTRAL;
  emotions->current_anim_idx = 0;
  emotions->next_face_switch_time = 0.0f;
  emotions->rng = pcg32_new(98765, 5);
  emotions->emotion_queue =
      queue_new_ALLOC(&ctx->allocator, EmotionQueueItem, 16);
  emotions->last_added_item = NULL;

  emotions->next_blink_time =
      2.0f + pcg32_next_f32_range(&emotions->blink_rng, 0.0f, 3.0f);
  emotions->is_double_blink = false;
  emotions->double_blink_delay = 0.0f;
  emotions->blink_rng = pcg32_new(12345, 6);

  emotions->next_idle_conversation_time = 0.0f;
  emotions->idle_conversation_rng = pcg32_new(54321, 7);

  emotions->reset_to_neutral_time = 0.0f;
  emotions->pending_neutral_reset = false;

  gym_state->model_asset_handle = asset_request(
      Model3DData, &gym_state->asset_system, ctx, "anya/anya.hasset");

  // Request multiple animations
  gym_state->idle_anim_asset_handles =
      slice_new_ALLOC(&ctx->allocator, AnimationAsset_Handle, ANIMATIONS_CAP);
  gym_state->listening_anim_asset_handles =
      slice_new_ALLOC(&ctx->allocator, AnimationAsset_Handle, ANIMATIONS_CAP);
  gym_state->thinking_anim_asset_handles =
      slice_new_ALLOC(&ctx->allocator, AnimationAsset_Handle, ANIMATIONS_CAP);
  gym_state->speaking_anim_asset_handles =
      slice_new_ALLOC(&ctx->allocator, AnimationAsset_Handle, ANIMATIONS_CAP);
  gym_state->greeting_anim_asset_handles =
      slice_new_ALLOC(&ctx->allocator, AnimationAsset_Handle, ANIMATIONS_CAP);
  gym_state->hands_anim_asset_handles =
      slice_new_ALLOC(&ctx->allocator, AnimationAsset_Handle, ANIMATIONS_CAP);

  {
    struct {
      char **animation_paths;
      u32 len;
      AnimationAsset_Handle_Slice *asset_handles;
    } all_animations[] = {
        {
            .animation_paths = idle_animations,
            .len = ARRAY_SIZE(idle_animations),
            .asset_handles = &gym_state->idle_anim_asset_handles,
        },
        {
            .animation_paths = listening_animations,
            .len = ARRAY_SIZE(listening_animations),
            .asset_handles = &gym_state->listening_anim_asset_handles,
        },
        {
            .animation_paths = thinking_animations,
            .len = ARRAY_SIZE(thinking_animations),
            .asset_handles = &gym_state->thinking_anim_asset_handles,
        },
        {
            .animation_paths = speaking_animations,
            .len = ARRAY_SIZE(speaking_animations),
            .asset_handles = &gym_state->speaking_anim_asset_handles,
        },
        {
            .animation_paths = greeting_animations,
            .len = ARRAY_SIZE(greeting_animations),
            .asset_handles = &gym_state->greeting_anim_asset_handles,
        },
        {
            .animation_paths = hands_animations,
            .len = ARRAY_SIZE(hands_animations),
            .asset_handles = &gym_state->hands_anim_asset_handles,
        },
    };

    for (u32 ii = 0; ii < ARRAY_SIZE(all_animations); ii++) {
      char **animation_paths = all_animations[ii].animation_paths;
      u32 num_animations = all_animations[ii].len;
      AnimationAsset_Handle_Slice *handles = all_animations[ii].asset_handles;
      for (u32 i = 0; i < num_animations; i++) {
        slice_append(*handles,
                     asset_request(AnimationAsset, &gym_state->asset_system,
                                   ctx, animation_paths[i]));
      }
    }
  }

  // Load face animations by emotion
  for (u32 i = 0; i < ARRAY_SIZE(face_animations); i++) {
    const char *animation_path = face_animations[i];
    EMOTION_TAGS emotion_tag = animation_path_to_emotion_tag(animation_path);
    character_emotions_request_animation(&gym_state->character_emotions,
                                         emotion_tag, animation_path,
                                         &gym_state->asset_system, ctx);
  }

  // Load blink animation
  emotions->blink_animation.handle =
      asset_request(AnimationAsset, &gym_state->asset_system, ctx,
                    "anya/Anya - Blink - BS.hasset");
  emotions->blink_animation.animation = NULL;
  emotions->blink_animation.is_loaded = false;

  gym_state->lipsync_profile_handle = asset_request(
      LipSyncProfile, &gym_state->asset_system, ctx, "lipsync_profile.passet");

  // Initialize camera
  glm_vec3(cast(vec3){0.0, 0.22, 3.25}, gym_state->camera.pos);
  gym_state->camera.fov = 14;
  gym_state->camera.pitch = 0;

  gym_state->skybox_texture_handle =
      asset_request(Texture, &gym_state->asset_system, ctx,
                    "backgrounds/background_anya_1.webp");

  gym_state->skybox_material_ready = false;

  // Load background music
  // gym_state->background_music_handle = asset_request(
  //     WavFile, &gym_state->asset_system, ctx, "music/spf - housework.wav");
  // gym_state->background_music_file = NULL;
  // gym_state->background_music_loaded = false;
  // gym_state->background_music_playing = false;
}

void handle_loading(GameContext *ctx, AssetSystem *asset_system,
                    AudioState *audio_system) {
  GymState *gym_state = ctx_user_data(ctx, GymState);
  // Load lipsync profile first since other assets depend on it
  if (!gym_state->lipsync_profile &&
      asset_is_ready(asset_system, gym_state->lipsync_profile_handle)) {
    gym_state->lipsync_profile =
        asset_get_data(LipSyncProfile, &gym_state->asset_system,
                       gym_state->lipsync_profile_handle);
    LOG_INFO("Lipsync profile loaded");
  }

  // Load model data first
  if (!gym_state->model_data &&
      asset_is_ready(asset_system, gym_state->model_asset_handle)) {
    gym_state->model_data = asset_get_data(
        Model3DData, &gym_state->asset_system, gym_state->model_asset_handle);

    // Count total submeshes across all meshes
    u32 total_submeshes = 0;
    for (u32 i = 0; i < gym_state->model_data->num_meshes; i++) {
      MeshData *mesh_data = &gym_state->model_data->meshes[i];
      total_submeshes += mesh_data->submeshes.len;
    }

    // Request materials based on submesh material_path
    gym_state->material_count = total_submeshes;
    gym_state->material_asset_handles =
        ALLOC_ARRAY(&ctx->allocator, MaterialAsset_Handle, total_submeshes);

    u32 material_idx = 0;
    for (u32 i = 0; i < gym_state->model_data->num_meshes; i++) {
      MeshData *mesh_data = &gym_state->model_data->meshes[i];

      for (u32 j = 0; j < mesh_data->submeshes.len; j++) {
        SubMeshData *submesh_data = &mesh_data->submeshes.items[j];

        if (submesh_data->material_path.len > 0 &&
            submesh_data->material_path.value != NULL) {
          // Request material asset - path is already absolute
          gym_state->material_asset_handles[material_idx] =
              asset_request(MaterialAsset, &gym_state->asset_system, ctx,
                            submesh_data->material_path.value);
          LOG_INFO("Requesting material % for mesh % submesh %",
                   FMT_STR(submesh_data->material_path.value), FMT_UINT(i),
                   FMT_UINT(j));
        } else {
          // No material path - will use white material
          gym_state->material_asset_handles[material_idx] =
              (MaterialAsset_Handle){0};
          LOG_INFO(
              "No material path for mesh % submesh %, will use white material",
              FMT_UINT(i), FMT_UINT(j));
        }
        material_idx++;
      }
    }

    LOG_INFO("Character Model loaded with % meshes, % total submeshes",
             FMT_UINT(gym_state->model_data->num_meshes),
             FMT_UINT(total_submeshes));
  }

  // Wait for all materials to load, then create SkinnedModel
  if (gym_state->model_data &&
      !gym_state->character.skinned_model.meshes.items) {
    bool all_materials_ready = true;

    for (u32 i = 0; i < gym_state->material_count; i++) {
      if (gym_state->material_asset_handles[i].idx != 0) {
        if (!asset_is_ready(asset_system,
                            gym_state->material_asset_handles[i])) {
          all_materials_ready = false;
          break;
        }
      }
    }

    if (all_materials_ready && gym_state->lipsync_profile) {
      // Create materials array
      gym_state->materials =
          slice_new_ALLOC(&ctx->allocator, Material, gym_state->material_count);

      for (u32 i = 0; i < gym_state->material_count; i++) {
        if (gym_state->material_asset_handles[i].idx != 0) {
          // Load material from asset
          MaterialAsset *material_asset =
              asset_get_data(MaterialAsset, &gym_state->asset_system,
                             gym_state->material_asset_handles[i]);
          assert(material_asset);
          Material *material = material_from_asset(
              material_asset, &gym_state->asset_system, ctx);
          slice_append(gym_state->materials, *material);
          LOG_INFO("Loaded material % for submesh %",
                   FMT_STR(material_asset->name.value), FMT_UINT(i));
        } else {
          // Use default white material for submeshes without materials
          LOG_WARN("No material for submesh %, skipping", FMT_UINT(i));
          Material default_material = {0};
          slice_append(gym_state->materials, default_material);
        }
      }

      // Create SkinnedModel with loaded materials
      Character *entity = &gym_state->character;

      quaternion temp_rot;
      quat_from_euler((vec3){0, 0, 0}, temp_rot);
      mat_trs((vec3){-0.00, 0, 0}, temp_rot, (vec3){0.01, 0.01, 0.01},
              entity->model_matrix);

      entity->skinned_model =
          skmodel_from_asset(ctx, gym_state->model_data, gym_state->materials);

      // init animated entity
      AnimatedEntity *animated_entity = &gym_state->character.animated;
      animated_entity_init(animated_entity, gym_state->model_data,
                           &ctx->allocator);

      // Create hands skeleton mask
      String hands_joints[] = {
          STR_FROM_CSTR("Left Hand"),       STR_FROM_CSTR("Right hand"),
          STR_FROM_CSTR("IndexFinger1_L"),  STR_FROM_CSTR("IndexFinger2_L"),
          STR_FROM_CSTR("IndexFinger3_L"),  STR_FROM_CSTR("MiddleFinger1_L"),
          STR_FROM_CSTR("MiddleFinger2_L"), STR_FROM_CSTR("MiddleFinger3_L"),
          STR_FROM_CSTR("RingFinger1_L"),   STR_FROM_CSTR("RingFinger2_L"),
          STR_FROM_CSTR("RingFinger3_L"),   STR_FROM_CSTR("Thumb0_L"),
          STR_FROM_CSTR("Thumb1_L"),        STR_FROM_CSTR("Thumb2_L"),
          STR_FROM_CSTR("LittleFinger1_L"), STR_FROM_CSTR("LittleFinger2_L"),
          STR_FROM_CSTR("LittleFinger3_L"), STR_FROM_CSTR("IndexFinger1_R"),
          STR_FROM_CSTR("IndexFinger2_R"),  STR_FROM_CSTR("IndexFinger3_R"),
          STR_FROM_CSTR("MiddleFinger1_R"), STR_FROM_CSTR("MiddleFinger2_R"),
          STR_FROM_CSTR("MiddleFinger3_R"), STR_FROM_CSTR("RingFinger1_R"),
          STR_FROM_CSTR("RingFinger2_R"),   STR_FROM_CSTR("RingFinger3_R"),
          STR_FROM_CSTR("Thumb0_R"),        STR_FROM_CSTR("Thumb1_R"),
          STR_FROM_CSTR("Thumb2_R"),        STR_FROM_CSTR("LittleFinger1_R"),
          STR_FROM_CSTR("LittleFinger2_R"), STR_FROM_CSTR("LittleFinger3_R"),
      };
      u32 num_hands_joints = sizeof(hands_joints) / sizeof(hands_joints[0]);

      SkeletonMask hands_mask = skeleton_mask_create_from_joint_names(
          &ctx->allocator, gym_state->model_data, hands_joints,
          num_hands_joints);

      // Add hands layer (after default layer at index 0)
      gym_state->hands_layer_index =
          animated_entity_add_layer(animated_entity, STR_FROM_CSTR("Hands"),
                                    hands_mask, 1.0f, &ctx->allocator);

      LOG_INFO("Created hands layer: %",
               FMT_UINT(gym_state->hands_layer_index));

      // Create face layer with empty skeleton mask (blendshapes only)
      SkeletonMask empty_mask =
          skeleton_mask_create_from_joints(&ctx->allocator, NULL, 0);
      gym_state->face_layer_index =
          animated_entity_add_layer(animated_entity, STR_FROM_CSTR("Face"),
                                    empty_mask, 1.0f, &ctx->allocator);

      LOG_INFO("Created face layer: %", FMT_UINT(gym_state->face_layer_index));

      // Create blink layer with empty skeleton mask (blendshapes only)
      SkeletonMask blink_empty_mask =
          skeleton_mask_create_from_joints(&ctx->allocator, NULL, 0);
      gym_state->blink_layer_index =
          animated_entity_add_layer(animated_entity, STR_FROM_CSTR("Blink"),
                                    blink_empty_mask, 1.0f, &ctx->allocator);

      LOG_INFO("Created blink layer: %",
               FMT_UINT(gym_state->blink_layer_index));

      entity->face_lipsync = lipsync_init(
          &ctx->allocator, gym_state->audio_system.output_sample_rate,
          gym_state->lipsync_profile);

      const char *face_name = "Body";
      i32 face_idx = arr_find_index_pred_raw(
          gym_state->model_data->meshes, gym_state->model_data->num_meshes,
          str_equal(_item.mesh_name.value, face_name));
      assert_msg(face_idx >= 0, "Couldn't find mesh %", FMT_STR(face_name));
      SkinnedMesh *face_mesh =
          arr_get_ptr(entity->skinned_model.meshes, face_idx);
      entity->face_blendshapes = blendshape_controller_init(
          &ctx->allocator, gym_state->lipsync_profile,
          phoneme_blendshape_definitions,
          ARRAY_SIZE(phoneme_blendshape_definitions), face_mesh);

      LOG_INFO("SkinnedModel created with % materials",
               FMT_UINT(gym_state->materials.len));
    }
  }

  // Load animations as they become ready
  if (gym_state->model_data && gym_state->materials.len > 0) {

    struct {
      AnimationAsset_Handle_Slice *handles;
      CharacterStateLogic *character_state;
    } load_character_states[] = {
        {
            .handles = &gym_state->idle_anim_asset_handles,
            .character_state = &gym_state->idle_state,
        },
        {
            .handles = &gym_state->listening_anim_asset_handles,
            .character_state = &gym_state->listening_state,
        },
        {
            .handles = &gym_state->speaking_anim_asset_handles,
            .character_state = &gym_state->speaking_state,
        },
        {
            .handles = &gym_state->thinking_anim_asset_handles,
            .character_state = &gym_state->thinking_state,
        },
        {
            .handles = &gym_state->greeting_anim_asset_handles,
            .character_state = &gym_state->greeting_state,
        },
        {
            .handles = &gym_state->hands_anim_asset_handles,
            .character_state = &gym_state->hands_state,
        },
    };

    foreach (load_character_states, ARRAY_SIZE(load_character_states),
             typeof(load_character_states[0]), load_op) {

      AnimationPtr_Slice *animations = NULL;
      switch (load_op.character_state->type) {
      case CHARACTER_STATE_IDLE:
        animations = &load_op.character_state->idle.animations;
        break;
      case CHARACTER_STATE_LISTENING:
        animations = &load_op.character_state->listening.animations;
        break;
      case CHARACTER_STATE_THINKING:
        animations = &load_op.character_state->thinking.animations;
        break;
      case CHARACTER_STATE_SPEAKING:
        animations = &load_op.character_state->speaking.animations;
        break;
      case CHARACTER_STATE_GREETING:
        animations = &load_op.character_state->greeting.animations;
        break;
      }

      for (u32 i = animations->len; i < load_op.handles->len; i++) {
        AnimationAsset_Handle handle = load_op.handles->items[i];
        if (asset_is_ready(asset_system, handle)) {
          AnimationAsset *anim_asset =
              asset_get_data(AnimationAsset, &gym_state->asset_system, handle);
          Animation *anim = animation_from_asset(
              anim_asset, gym_state->model_data, &ctx->allocator);
          slice_append(*animations, anim);
        } else {
          break; // Stop checking once we hit an unready asset
        }
      }
    }
  }

  // Update emotion animations loading
  if (gym_state->model_data) {
    character_emotions_update_loading(&gym_state->character_emotions,
                                      asset_system, gym_state->model_data, ctx);
  }

  // Handle hands layer animation - always play hands animation
  if (gym_state->hands_state.idle.animations.len > 0 &&
      gym_state->character.animated.layers.len > gym_state->hands_layer_index) {
    AnimatedEntity *animated = &gym_state->character.animated;
    AnimationLayer *hands_layer =
        &animated->layers.items[gym_state->hands_layer_index];

    if (hands_layer->animation_states.len == 0) {
      // Start hands animation
      animated_entity_play_animation_on_layer(
          animated, gym_state->hands_layer_index,
          gym_state->hands_state.idle.animations.items[0], 0.0f, 1.0, true);
      LOG_INFO(
          "Started hands animation: %",
          FMT_STR(gym_state->hands_state.idle.animations.items[0]->name.value));
    }
  }

  if (!gym_state->skybox_material_ready &&
      asset_is_ready(asset_system, gym_state->skybox_texture_handle)) {

    // Create MaterialAsset in memory
    MaterialAsset *skybox_mat_asset = ALLOC(&ctx->allocator, MaterialAsset);
    *skybox_mat_asset = (MaterialAsset){
        .name = STR_FROM_CSTR("SkyboxMaterial"),
        .shader_path = STR_FROM_CSTR("materials/background_img.frag"),
        .transparent = false,
        .shader_defines = arr_new_ALLOC(&ctx->allocator, ShaderDefine, 0),
        .properties = arr_from_c_array_alloc(
            MaterialAssetProperty, &ctx->allocator,
            ((MaterialAssetProperty[]){
                {
                    .name = STR_FROM_CSTR("uTexture"),
                    .type = MAT_PROP_TEXTURE,
                    .texture_path =
                        STR_FROM_CSTR("backgrounds/background_anya_1.webp"),
                },
                {
                    .name = STR_FROM_CSTR("uColor"),
                    .type = MAT_PROP_VEC3,
                    .color = {{{0.9, 0.9, 0.9, 1.0}}},
                },
            })),
    };

    // Create Material from asset
    Material *skybox_material =
        material_from_asset(skybox_mat_asset, asset_system, ctx);

    LOG_INFO("Created skybox material with shader handle idx=%, gen=%",
             FMT_UINT(skybox_material->gpu_material.idx),
             FMT_UINT(skybox_material->gpu_material.gen));

    // Set as skybox material - gpu_material should contain the renderer
    // material handle
    renderer_set_skybox_material(skybox_material->gpu_material);

    gym_state->skybox_material_ready = true;
    LOG_INFO("Skybox material set successfully");
  }

  // Load background music
  // if (!gym_state->background_music_loaded &&
  //     asset_is_ready(asset_system, gym_state->background_music_handle)) {
  //   gym_state->background_music_file = asset_get_data(
  //       WavFile, asset_system, gym_state->background_music_handle);
  //   gym_state->background_music_loaded = true;
  //
  //   if (gym_state->background_music_file) {
  //     LOG_INFO("Background music loaded: % Hz, % channels, % samples",
  //              FMT_INT(gym_state->background_music_file->format.sample_rate),
  //              FMT_INT(gym_state->background_music_file->format.channels),
  //              FMT_INT(gym_state->background_music_file->total_samples));
  //   }
  // }

  // Start background music if loaded and not playing
  // if (gym_state->background_music_loaded &&
  //     !gym_state->background_music_playing &&
  //     gym_state->background_music_file) {
  //   AudioClip music_clip = {
  //       .wav_file = gym_state->background_music_file,
  //       .loop = true,
  //       .volume = 0.01,
  //   };
  //   audio_play_clip(audio_system, music_clip);
  //   gym_state->background_music_playing = true;
  //   LOG_INFO("Started playing background music");
  // }
}

const char *character_state_to_string(CharacterStateType state) {
  switch (state) {
  case CHARACTER_STATE_IDLE:
    return "IDLE";
  case CHARACTER_STATE_LISTENING:
    return "LISTENING";
  case CHARACTER_STATE_THINKING:
    return "THINKING";
  case CHARACTER_STATE_SPEAKING:
    return "SPEAKING";
  default:
    return "UNKNOWN";
  }
}

CharacterStateType determine_character_state(ConversationSystem *conversation) {
  if (conversation_is_user_speaking(conversation)) {
    return CHARACTER_STATE_LISTENING;
  }
  if (conversation_is_ai_speaking(conversation)) {
    return CHARACTER_STATE_SPEAKING;
  }
  if (conversation_is_processing(conversation)) {
    return CHARACTER_STATE_THINKING;
  }
  return CHARACTER_STATE_IDLE;
}

void character_state_enter(GameContext *ctx, CharacterStateLogic *state) {
  GymState *gym_state = ctx_user_data(ctx, GymState);
  switch (state->type) {
  case CHARACTER_STATE_IDLE:
    state->idle.did_enter = false;
    break;
  case CHARACTER_STATE_LISTENING:
    state->listening.did_enter = false;
    break;
  case CHARACTER_STATE_THINKING:
    state->thinking.did_enter = false;
    break;
  case CHARACTER_STATE_SPEAKING:
    state->speaking.did_enter = false;
    break;
  case CHARACTER_STATE_GREETING: {
    state->greeting.did_enter = false;
    state->greeting.did_finish_playing = false;
    ConversationSystem *conversation = &gym_state->conversation_system;
    conversation->tts_system.audio_play_enabled = false;
  } break;
  }
}

void character_state_update(GameContext *ctx, CharacterStateLogic *in_state,
                            const GameTime *time) {
  GymState *gym_state = ctx_user_data(ctx, GymState);

  switch (in_state->type) {
  case CHARACTER_STATE_IDLE:
  case CHARACTER_STATE_LISTENING:
  case CHARACTER_STATE_THINKING:
  case CHARACTER_STATE_SPEAKING: {
    StateLogic_Loop *state = &in_state->idle;
    if (state->animations.len == 0) {
      return;
    }

    AnimatedEntity *animated = &gym_state->character.animated;
    if (!state->did_enter) {
      u32 random_idx =
          pcg32_next_u32_range(&state->rng, 0, state->animations.len);
      state->current_anim_idx = random_idx;
      AnimationPtr anim = state->animations.items[random_idx];
      animated_entity_play_animation(animated, anim, 0.35, 1.0, true);

      f32 switch_interval = 3.0f + pcg32_next_f32(&state->rng) * 2.0f;
      state->next_switch_time = time->now + switch_interval;
      state->did_enter = true;
    } else if (state->animations.len > 1 && animated->layers.len > 0) {
      AnimationLayer *layer = &animated->layers.items[0];
      if (!layer->current_transition.active &&
          layer->animation_states.len > 0) {
        if (time->now >= state->next_switch_time) {
          u32 next_idx;
          next_idx =
              pcg32_next_u32_range(&state->rng, 0, state->animations.len);

          state->current_anim_idx = next_idx;
          AnimationPtr next_anim = state->animations.items[next_idx];
          animated_entity_play_animation(animated, next_anim, 0.35, 1.0, true);

          f32 switch_interval = 3.0f + pcg32_next_f32(&state->rng) * 2.0f;
          state->next_switch_time = time->now + switch_interval;
        }
      }
    }

    break;
  }
  case CHARACTER_STATE_GREETING: {
    StateLogic_OneShot *state = &in_state->greeting;
    if (state->animations.len == 0) {
      return;
    }

    AnimatedEntity *animated = &gym_state->character.animated;
    if (!state->did_enter) {
      u32 random_idx =
          state->animations.len > 1
              ? pcg32_next_u32_range(&state->rng, 0, state->animations.len)
              : 0;
      state->current_anim_idx = random_idx;
      AnimationPtr anim = state->animations.items[random_idx];
      animated_entity_play_animation(animated, anim, 0.35, 1.0, true);
      state->did_enter = true;
    } else if (!state->did_finish_playing && animated->layers.len > 0) {
      AnimationLayer *layer = &animated->layers.items[0];
      if (layer->animation_states.len > 0) {
        AnimationState *current_state =
            &layer->animation_states.items[layer->current_animation_index];
        Animation *current_animation = current_state->animation;
        Animation *state_current_animation =
            state->animations.items[state->current_anim_idx];
        if (state_current_animation != current_animation) {
          return;
        }

        f32 transition_trigger_time = current_animation->length * 0.9f;
        if (current_state->time >= transition_trigger_time) {
          state->did_finish_playing = true;
        }
      }
    }

    break;
  } break;
  }
}

void character_state_exit(GameContext *ctx, CharacterStateLogic *state) {
  GymState *gym_state = ctx_user_data(ctx, GymState);
  switch (state->type) {
  case CHARACTER_STATE_IDLE:
  case CHARACTER_STATE_LISTENING:
  case CHARACTER_STATE_THINKING:
  case CHARACTER_STATE_SPEAKING:
    break;
  case CHARACTER_STATE_GREETING:
    gym_state->conversation_system.tts_system.audio_play_enabled = true;
    break;
  }
}

void character_state_machine_update(ConversationSystem *conversation,
                                    GameContext *ctx, const GameTime *time) {
  UNUSED(time);
  GymState *gym_state = ctx_user_data(ctx, GymState);
  if (gym_state->character_state == NULL) {
    gym_state->character_state = &gym_state->idle_state;
    character_state_enter(ctx, gym_state->character_state);
  }

  // send initial greeting if not done yet
  if (can_start && !gym_state->initial_greeting_sent) {
    microphone_start_recording(
        &gym_state->conversation_system.stt_system.mic_system);
    gym_state->initial_greeting_sent = true;
    send_conversation_request(conversation, ctx);
    gym_state->conversation_system.tts_system.audio_play_enabled = true;
    gym_state->did_wave = true;
    LOG_INFO("Sent initial AI greeting request");
  }

  // if (!gym_state->did_wave && time->now > 1.5) {
  //   gym_state->did_wave = true;
  //   if (gym_state->character_state) {
  //     character_state_exit(gym_state->character_state);
  //   }
  //   gym_state->character_state = &gym_state->greeting_state;
  //   character_state_enter(gym_state->character_state);
  //   LOG_INFO("Sent initial AI greeting request");
  // }

  if (!gym_state->initial_greeting_sent || !gym_state->did_wave) {
    return;
  }

  // prevent changing state until we finished greeting
  CharacterStateLogic *current_state = gym_state->character_state;
  if (current_state && current_state->type == CHARACTER_STATE_GREETING &&
      !current_state->greeting.did_finish_playing) {
    return;
  }

  CharacterStateType new_state = determine_character_state(conversation);

  if (gym_state->character_state->type != new_state) {
    CharacterStateLogic *prev_state = gym_state->character_state;
    switch (new_state) {
    case CHARACTER_STATE_IDLE:
      gym_state->character_state = &gym_state->idle_state;
      break;
    case CHARACTER_STATE_LISTENING:
      gym_state->character_state = &gym_state->listening_state;
      break;
    case CHARACTER_STATE_THINKING:
      gym_state->character_state = &gym_state->thinking_state;
      break;
    case CHARACTER_STATE_SPEAKING:
      gym_state->character_state = &gym_state->speaking_state;
      break;
    case CHARACTER_STATE_GREETING:
      gym_state->character_state = &gym_state->greeting_state;
      break;
    }

    character_state_exit(ctx, prev_state);
    character_state_enter(ctx, gym_state->character_state);
  }
  // LOG_INFO("Character state: %", FMT_STR(character_state_to_string(
  //                                    gym_state->character_state->type)));
}

void gym_update_and_render(GameMemory *memory) {
  GameContext *ctx = &memory->ctx;
  GameTime *time = &memory->time;

  GymState *gym_state = ctx_user_data(ctx, GymState);

  f32 dt = time->dt;

  AudioState *audio_system = &gym_state->audio_system;
  AssetSystem *asset_system = &gym_state->asset_system;
  GameInput *input = &gym_state->input;
  ConversationSystem *conversation = &gym_state->conversation_system;

  Character *entity = &gym_state->character;
  AnimatedEntity *animated = &entity->animated;

  asset_system_update(asset_system, ctx);
  handle_loading(ctx, asset_system, audio_system);

  if (!can_start) {
    return;
  }

  conversation_system_update(conversation, ctx, dt, audio_system);

  input_update(input, &memory->input_events, memory->time.now);
  audio_update(audio_system, ctx, dt);

  character_state_machine_update(conversation, ctx, time);
  character_state_update(ctx, gym_state->character_state, time);

  character_emotions_update_detection(
      &gym_state->character_emotions, &conversation->tts_system, audio_system,
      ctx, time, animated, gym_state->face_layer_index);

  character_emotions_update_neutral_reset(&gym_state->character_emotions,
                                          conversation, time, animated,
                                          gym_state->face_layer_index);

  character_emotions_update_blinking(&gym_state->character_emotions, time,
                                     animated, gym_state->blink_layer_index);

  character_emotions_update_idle_conversation(&gym_state->character_emotions,
                                              conversation, time, ctx);

  animated_entity_update(animated, dt);

  animated_entity_evaluate_pose(animated, gym_state->model_data);

  animated_entity_apply_pose(animated, gym_state->model_data,
                             &entity->skinned_model);

  // lipsync
  // Feed audio to lipsync system
  LipSyncContext *lipsync = &entity->face_lipsync;
  lipsync_feed_audio(lipsync, ctx, audio_system->sample_buffer,
                     audio_system->sample_buffer_len,
                     audio_system->output_channels);
  // Process and get results
  if (lipsync_process(lipsync, ctx)) {
    LipSyncResult result = lipsync_get_result(lipsync);

    LipsyncBlendshapeController *blendshape_controller =
        &entity->face_blendshapes;

    blendshape_controller_update(blendshape_controller, result, time->dt);

    blendshape_controller_apply(blendshape_controller);
  }

  camera_update_uniforms(&gym_state->camera, memory->canvas.width,
                         memory->canvas.height);

  local_persist vec3 light_dir = {0.5349, 0.2722, 0.79914};
  glm_normalize(light_dir);
  gym_state->directional_lights.count = 1;
  gym_state->directional_lights.lights[0] = (DirectionalLight){
      .direction = {light_dir[0], light_dir[1], light_dir[2]},
      .color = {1, 1, 1},
      .intensity = 0.8};

  gym_state->point_lights.count = 0;

  renderer_set_lights(&gym_state->directional_lights, &gym_state->point_lights);

  // Render the model
  renderer_skm_draw(&ctx->temp_allocator, &entity->skinned_model,
                    entity->model_matrix);

  input_end_frame(input);
  game_stats_update(ctx, &gym_state->stats, dt);
  ui_set_stats(&gym_state->stats);
}

export void game_set_can_start() { can_start = true; }
