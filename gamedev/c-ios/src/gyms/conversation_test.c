#include "../conversation_system.h"
#include "../game.h"
#include "../lib/audio.h"
#include "../lib/memory.h"
#include "../lib/typedefs.h"
#include "../platform.h"
#include "../vendor/stb/stb_image.h"

typedef struct {
  AudioState audio_system;
  ConversationSystem conversation_system;

  bool32 initial_greeting_sent;

} GymState;

global GymState *gym_state;

void gym_init(GameMemory *memory) {
  GameContext *ctx = &memory->ctx;

  gym_state = ALLOC(&ctx->allocator, sizeof(GymState));
  gym_state->audio_system = audio_init(ctx);

  gym_state->conversation_system =
      conversation_system_init(ctx, &gym_state->audio_system);

  gym_state->initial_greeting_sent = false;
}

void gym_update_and_render(GameMemory *memory) {
  GameContext *ctx = &memory->ctx;
  GameTime *time = &memory->time;

  f32 dt = time->dt;

  AudioState *audio_system = &gym_state->audio_system;
  ConversationSystem *conversation_system = &gym_state->conversation_system;

  // send initial greeting if not done yet
  if (!gym_state->initial_greeting_sent) {
    gym_state->initial_greeting_sent = true;
    send_conversation_request(conversation_system, ctx);
    LOG_INFO("Sent initial AI greeting request");
  }

  conversation_system_update(conversation_system, ctx, dt, audio_system);
  audio_update(audio_system);
}
