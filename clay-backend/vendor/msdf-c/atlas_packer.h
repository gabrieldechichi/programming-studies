#ifndef ATLAS_PACKER_H
#define ATLAS_PACKER_H

#include <stdint.h>

// Skyline bin packer node
typedef struct AtlasNode {
    short x, y, width;
} AtlasNode;

// Atlas packer using skyline algorithm
typedef struct Atlas {
    int width, height;
    AtlasNode* nodes;
    int nnodes;   // current number of nodes
    int cnodes;   // capacity
} Atlas;

// Create a new atlas
Atlas* atlas_create(int width, int height, int initial_nodes);

// Delete atlas
void atlas_delete(Atlas* atlas);

// Add a rectangle to the atlas, returns 1 on success, 0 on failure
// rx, ry will be filled with the position where the rect was placed
int atlas_add_rect(Atlas* atlas, int rw, int rh, int* rx, int* ry);

// Reset atlas (clear all packed rectangles)
void atlas_reset(Atlas* atlas, int width, int height);

// Expand atlas size
void atlas_expand(Atlas* atlas, int width, int height);

#endif // ATLAS_PACKER_H
