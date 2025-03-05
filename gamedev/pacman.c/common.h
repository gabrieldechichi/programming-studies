#define DISPLAY_TILES_X (28)
#define DISPLAY_TILES_Y (36)

#define DISPLAY_TILES_X (28)
#define DISPLAY_TILES_Y (36)
#define DISPLAY_RES_X (224)
#define DISPLAY_RES_Y (288)
#define PIXEL_SCALE 2
#define SCREEN_WIDTH DISPLAY_RES_X *PIXEL_SCALE
#define SCREEN_HEIGHT DISPLAY_RES_Y *PIXEL_SCALE

#define SIM_SPEED 1
#define TARGET_FPS 45 * SIM_SPEED
#define TARGET_DT_NS 1000000000 / TARGET_FPS
#define TARGET_DT NS_TO_SECS(TARGET_DT_NS)
#define SLEEP_BUFFER_NS MS_TO_NS(2)

//Namco sound generator runs at 96kHz
#define AUDIO_SAMPLE_RATE 96000
// #define AUDIO_SAMPLE_RATE 44100
#define AUDIO_BUFFER_SIZE (int)(AUDIO_SAMPLE_RATE * TARGET_DT * 1.5)

