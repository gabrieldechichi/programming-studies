// Atlas packer using Skyline Bin Packing algorithm
// Based on fontstash.h by Mikko Mononen
// Algorithm by Jukka Jylänki

#include "atlas_packer.h"
#include <stdlib.h>
#include <string.h>

static int mini(int a, int b) { return a < b ? a : b; }
static int maxi(int a, int b) { return a > b ? a : b; }

static void* atlas_realloc(void* ptr, size_t size) {
    return realloc(ptr, size);
}

Atlas* atlas_create(int w, int h, int nnodes) {
    Atlas* atlas = (Atlas*)malloc(sizeof(Atlas));
    if (atlas == NULL) return NULL;
    memset(atlas, 0, sizeof(Atlas));

    atlas->width = w;
    atlas->height = h;

    // Allocate space for skyline nodes
    atlas->nodes = (AtlasNode*)malloc(sizeof(AtlasNode) * nnodes);
    if (atlas->nodes == NULL) {
        free(atlas);
        return NULL;
    }
    memset(atlas->nodes, 0, sizeof(AtlasNode) * nnodes);
    atlas->nnodes = 0;
    atlas->cnodes = nnodes;

    // Init root node
    atlas->nodes[0].x = 0;
    atlas->nodes[0].y = 0;
    atlas->nodes[0].width = (short)w;
    atlas->nnodes++;

    return atlas;
}

void atlas_delete(Atlas* atlas) {
    if (atlas == NULL) return;
    if (atlas->nodes != NULL) free(atlas->nodes);
    free(atlas);
}

static int atlas_insert_node(Atlas* atlas, int idx, int x, int y, int w) {
    int i;
    // Insert node
    if (atlas->nnodes + 1 > atlas->cnodes) {
        atlas->cnodes = atlas->cnodes == 0 ? 8 : atlas->cnodes * 2;
        atlas->nodes = (AtlasNode*)atlas_realloc(atlas->nodes, sizeof(AtlasNode) * atlas->cnodes);
        if (atlas->nodes == NULL)
            return 0;
    }
    for (i = atlas->nnodes; i > idx; i--)
        atlas->nodes[i] = atlas->nodes[i-1];
    atlas->nodes[idx].x = (short)x;
    atlas->nodes[idx].y = (short)y;
    atlas->nodes[idx].width = (short)w;
    atlas->nnodes++;

    return 1;
}

static void atlas_remove_node(Atlas* atlas, int idx) {
    int i;
    if (atlas->nnodes == 0) return;
    for (i = idx; i < atlas->nnodes - 1; i++)
        atlas->nodes[i] = atlas->nodes[i + 1];
    atlas->nnodes--;
}

void atlas_expand(Atlas* atlas, int w, int h) {
    // Insert node for empty space
    if (w > atlas->width)
        atlas_insert_node(atlas, atlas->nnodes, atlas->width, 0, w - atlas->width);
    atlas->width = w;
    atlas->height = h;
}

void atlas_reset(Atlas* atlas, int w, int h) {
    atlas->width = w;
    atlas->height = h;
    atlas->nnodes = 0;

    // Init root node
    atlas->nodes[0].x = 0;
    atlas->nodes[0].y = 0;
    atlas->nodes[0].width = (short)w;
    atlas->nnodes++;
}

static int atlas_add_skyline_level(Atlas* atlas, int idx, int x, int y, int w, int h) {
    int i;

    // Insert new node
    if (atlas_insert_node(atlas, idx, x, y + h, w) == 0)
        return 0;

    // Delete skyline segments that fall under the shadow of the new segment
    for (i = idx + 1; i < atlas->nnodes; i++) {
        if (atlas->nodes[i].x < atlas->nodes[i-1].x + atlas->nodes[i-1].width) {
            int shrink = atlas->nodes[i-1].x + atlas->nodes[i-1].width - atlas->nodes[i].x;
            atlas->nodes[i].x += (short)shrink;
            atlas->nodes[i].width -= (short)shrink;
            if (atlas->nodes[i].width <= 0) {
                atlas_remove_node(atlas, i);
                i--;
            } else {
                break;
            }
        } else {
            break;
        }
    }

    // Merge same height skyline segments that are next to each other
    for (i = 0; i < atlas->nnodes - 1; i++) {
        if (atlas->nodes[i].y == atlas->nodes[i+1].y) {
            atlas->nodes[i].width += atlas->nodes[i+1].width;
            atlas_remove_node(atlas, i + 1);
            i--;
        }
    }

    return 1;
}

static int atlas_rect_fits(Atlas* atlas, int i, int w, int h) {
    // Checks if there is enough space at the location of skyline span 'i'
    // Returns the max height of all skyline spans under that location
    // Or -1 if no space found
    int x = atlas->nodes[i].x;
    int y = atlas->nodes[i].y;
    int space_left;
    if (x + w > atlas->width)
        return -1;
    space_left = w;
    while (space_left > 0) {
        if (i == atlas->nnodes) return -1;
        y = maxi(y, atlas->nodes[i].y);
        if (y + h > atlas->height) return -1;
        space_left -= atlas->nodes[i].width;
        ++i;
    }
    return y;
}

int atlas_add_rect(Atlas* atlas, int rw, int rh, int* rx, int* ry) {
    int besth = atlas->height, bestw = atlas->width, besti = -1;
    int bestx = -1, besty = -1, i;

    // Bottom left fit heuristic
    for (i = 0; i < atlas->nnodes; i++) {
        int y = atlas_rect_fits(atlas, i, rw, rh);
        if (y != -1) {
            if (y + rh < besth || (y + rh == besth && atlas->nodes[i].width < bestw)) {
                besti = i;
                bestw = atlas->nodes[i].width;
                besth = y + rh;
                bestx = atlas->nodes[i].x;
                besty = y;
            }
        }
    }

    if (besti == -1)
        return 0;

    // Perform the actual packing
    if (atlas_add_skyline_level(atlas, besti, bestx, besty, rw, rh) == 0)
        return 0;

    *rx = bestx;
    *ry = besty;

    return 1;
}
