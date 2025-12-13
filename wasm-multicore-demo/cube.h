#ifndef H_CUBE
#define H_CUBE

#include "lib/typedefs.h"

// Vertex structure: position (vec3) + normal (vec3) + color (vec4)
typedef struct {
  f32 x, y, z;
  f32 nx, ny, nz;
  f32 r, g, b, a;
} CubeVertex;

// Cube mesh data (24 vertices, 6 faces with unique colors and normals)
static CubeVertex cube_vertices[] = {
    // Front face (red) - normal: (0, 0, 1)
    {-1, -1, 1, 0, 0, 1, 1, 0, 0, 1},
    {1, -1, 1, 0, 0, 1, 1, 0, 0, 1},
    {1, 1, 1, 0, 0, 1, 1, 0, 0, 1},
    {-1, 1, 1, 0, 0, 1, 1, 0, 0, 1},
    // Back face (green) - normal: (0, 0, -1)
    {-1, -1, -1, 0, 0, -1, 0, 1, 0, 1},
    {-1, 1, -1, 0, 0, -1, 0, 1, 0, 1},
    {1, 1, -1, 0, 0, -1, 0, 1, 0, 1},
    {1, -1, -1, 0, 0, -1, 0, 1, 0, 1},
    // Top face (blue) - normal: (0, 1, 0)
    {-1, 1, -1, 0, 1, 0, 0, 0, 1, 1},
    {-1, 1, 1, 0, 1, 0, 0, 0, 1, 1},
    {1, 1, 1, 0, 1, 0, 0, 0, 1, 1},
    {1, 1, -1, 0, 1, 0, 0, 0, 1, 1},
    // Bottom face (yellow) - normal: (0, -1, 0)
    {-1, -1, -1, 0, -1, 0, 1, 1, 0, 1},
    {1, -1, -1, 0, -1, 0, 1, 1, 0, 1},
    {1, -1, 1, 0, -1, 0, 1, 1, 0, 1},
    {-1, -1, 1, 0, -1, 0, 1, 1, 0, 1},
    // Right face (magenta) - normal: (1, 0, 0)
    {1, -1, -1, 1, 0, 0, 1, 0, 1, 1},
    {1, 1, -1, 1, 0, 0, 1, 0, 1, 1},
    {1, 1, 1, 1, 0, 0, 1, 0, 1, 1},
    {1, -1, 1, 1, 0, 0, 1, 0, 1, 1},
    // Left face (cyan) - normal: (-1, 0, 0)
    {-1, -1, -1, -1, 0, 0, 0, 1, 1, 1},
    {-1, -1, 1, -1, 0, 0, 0, 1, 1, 1},
    {-1, 1, 1, -1, 0, 0, 0, 1, 1, 1},
    {-1, 1, -1, -1, 0, 0, 0, 1, 1, 1},
};

static u16 cube_indices[] = {
    0,  1,  2,  0,  2,  3,  // front
    4,  5,  6,  4,  6,  7,  // back
    8,  9,  10, 8,  10, 11, // top
    12, 13, 14, 12, 14, 15, // bottom
    16, 17, 18, 16, 18, 19, // right
    20, 21, 22, 20, 22, 23, // left
};

#define CUBE_INDEX_COUNT 36
#define CUBE_VERTEX_STRIDE sizeof(CubeVertex)

#endif
