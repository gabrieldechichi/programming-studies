#pragma once

#include <GL/gl.h>
#include <cstdint>
class GLTexture {
  public:
    GLuint id;
    uint32_t width, height;

    static GLTexture create(int width, int height);

    void destroy();

    void setPixels(unsigned char *pixels);

    void draw();
};

typedef GLTexture Texture;
