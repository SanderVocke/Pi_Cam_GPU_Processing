#ifndef COMMON_H
#define COMMON_H

#define DBG(...) {printf(__VA_ARGS__); printf("\n");}

#define CAPTURE_WIDTH 640
#define CAPTURE_HEIGHT 640
#define CAPTURE_FPS 15

#define LOWRES_WIDTH 256

#define MAINSHADER "./shaders/out.glsl"

#endif