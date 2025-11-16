#ifndef H_TEMPGEN
#define H_TEMPGEN
#include "lib/typedefs.h"
#include "renderer/renderer.h"

Model3DData *read_Model3DData(const uint8 *binary_data, u32 data_len,
                              Allocator *allocator);

b32 write_Model3DData(const Model3DData *model, Allocator *allocator,
                      _out_ u8 **buffer, _out_ u32 *buffer_size);

MaterialAsset *read_MaterialAsset(const u8 *binary_data, u32 data_len,
                                  Allocator *allocator);

b32 write_MaterialAsset(const MaterialAsset *material, Allocator *allocator,
                        _out_ u8 **buffer, _out_ u32 *buffer_size);
#endif
