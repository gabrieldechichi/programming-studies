#include "temp_generated.h"
#include "lib/serialization.h"

Model3DData *read_Model3DData(const uint8 *binary_data, u32 data_len,
                              Allocator *allocator) {
  if (binary_data == NULL || data_len == 0 || allocator == NULL) {
    return NULL; // Invalid input
  }

  BinaryReader reader = {
      .bytes = binary_data, .len = data_len, .cur_offset = 0};
  Model3DData *model = ALLOC(allocator, Model3DData);
  if (model == NULL) {
    return NULL;
  }

  read_u32(&reader, &model->version);
  read_u32(&reader, &model->num_meshes);

  model->meshes = ALLOC_ARRAY(allocator, MeshData, model->num_meshes);
  if (model->meshes == NULL) {
    return NULL;
  }

  // Read mesh data
  for (u32 i = 0; i < model->num_meshes; i++) {
    MeshData *mesh = &model->meshes[i];

    // read mesh_name
    u32 mesh_name_len;
    read_u32(&reader, &mesh_name_len);
    mesh->mesh_name.len = mesh_name_len;
    if (mesh_name_len > 0) {
      mesh->mesh_name.value = ALLOC_ARRAY(allocator, char, mesh_name_len + 1);
      read_u8_array(&reader, (u8 *)mesh->mesh_name.value, mesh_name_len);
      mesh->mesh_name.value[mesh_name_len] = '\0';
    } else {
      mesh->mesh_name.value = NULL;
    }

    // read blendshape_names array
    u32 blendshape_names_len;
    read_u32(&reader, &blendshape_names_len);
    mesh->blendshape_names =
        arr_new_ALLOC(allocator, String32Bytes, blendshape_names_len);
    if (blendshape_names_len > 0) {
      read_u8_array(&reader, (u8 *)mesh->blendshape_names.items,
                    sizeof(String32Bytes) * blendshape_names_len);
    }

    // read submeshes array
    u32 submeshes_len;
    read_u32(&reader, &submeshes_len);
    mesh->submeshes = arr_new_ALLOC(allocator, SubMeshData, submeshes_len);

    for (u32 j = 0; j < submeshes_len; j++) {
      SubMeshData *submesh = &mesh->submeshes.items[j];

      read_u32(&reader, &submesh->len_vertices);
      read_u32(&reader, &submesh->vertex_stride);
      read_u32(&reader, &submesh->len_vertex_buffer);

      submesh->vertex_buffer =
          ALLOC_ARRAY(allocator, u8, submesh->len_vertex_buffer);
      if (submesh->vertex_buffer == NULL) {
        return NULL;
      }

      read_u8_array(&reader, submesh->vertex_buffer,
                    submesh->len_vertex_buffer);

      read_u32(&reader, &submesh->len_indices);

      submesh->indices = ALLOC_ARRAY(allocator, u32, submesh->len_indices);
      if (submesh->indices == NULL) {
        return NULL;
      }

      read_u32_array(&reader, submesh->indices, submesh->len_indices);

      // Read blendshape data
      read_u32(&reader, &submesh->len_blendshapes);
      if (submesh->len_blendshapes > 0) {
        u32 total_deltas = submesh->len_blendshapes * submesh->len_vertices * 6;
        submesh->blendshape_deltas = ALLOC_ARRAY(allocator, f32, total_deltas);
        if (submesh->blendshape_deltas == NULL) {
          return NULL;
        }
        read_f32_array(&reader, submesh->blendshape_deltas, total_deltas);
      } else {
        submesh->blendshape_deltas = NULL;
      }

      // read material_path
      u32 material_path_len;
      read_u32(&reader, &material_path_len);
      submesh->material_path.len = material_path_len;
      if (material_path_len > 0) {
        submesh->material_path.value =
            ALLOC_ARRAY(allocator, char, material_path_len + 1);
        read_u8_array(&reader, (u8 *)submesh->material_path.value,
                      material_path_len);
        submesh->material_path.value[material_path_len] = '\0';
      } else {
        submesh->material_path.value = NULL;
      }
    }
  }

  read_u32(&reader, &model->len_joints);
  if (model->len_joints > 0) {
    model->joints = ALLOC_ARRAY(allocator, Joint, model->len_joints);
    if (model->joints == NULL) {
      return NULL;
    }

    foreach_ptr(model->joints, model->len_joints, Joint, joint) {
      read_i32(&reader, &joint->parent_index);
      read_f32_array(&reader, (f32 *)joint->inverse_bind_matrix, 16);
      read_u32(&reader, &joint->children.len);

      if (joint->children.len > 0) {
        joint->children.items =
            ALLOC_ARRAY(allocator, u32, joint->children.len);
        read_u32_array(&reader, joint->children.items, joint->children.len);
      }
    }

    // read joint_names
    model->joint_names = ALLOC_ARRAY(allocator, String, model->len_joints);
    for (u32 i = 0; i < model->len_joints; i++) {
      u32 str_len = 0;
      read_u32(&reader, &str_len);
      model->joint_names[i].len = str_len;
      model->joint_names[i].value = ALLOC_ARRAY(allocator, char, str_len + 1);
      read_u8_array(&reader, (u8 *)model->joint_names[i].value, str_len);
      model->joint_names[i].value[str_len] = '\0'; // null terminate
    }
  }

  assert(reader.cur_offset == reader.len);

  return model;
}

b32 write_Model3DData(const Model3DData *model, Allocator *allocator,
                      _out_ u8 **buffer, _out_ u32 *buffer_size) {
  if (!model || !allocator) {
    return false; // Invalid input
  }
  // calculate total size
  size_t total_size = 0;
  total_size += sizeof(u32); // version
  total_size += sizeof(u32); // num_meshes
                             //
  for (u32 i = 0; i < model->num_meshes; i++) {
    MeshData *mesh = &model->meshes[i];

    // mesh_name
    total_size += sizeof(u32); // mesh_name length
    total_size += mesh->mesh_name.len;

    // blendshape_names array
    total_size += sizeof(u32); // blendshape_names.len
    if (mesh->blendshape_names.len > 0) {
      total_size += sizeof(String32Bytes) * mesh->blendshape_names.len;
    }

    // submeshes array
    total_size += sizeof(u32); // submeshes.len
    for (u32 j = 0; j < mesh->submeshes.len; j++) {
      SubMeshData *submesh = &mesh->submeshes.items[j];
      total_size += sizeof(u32); // len_vertices
      total_size += sizeof(u32); // vertex_stride
      total_size += sizeof(u32); // len_vertex_buffer;
      total_size += sizeof(u8) * submesh->len_vertex_buffer; // vertex_buffer
      total_size += sizeof(u32);                             // len_indices
      total_size += sizeof(u32) * submesh->len_indices;      // indices

      // blendshape data
      total_size += sizeof(u32); // len_blendshapes
      if (submesh->len_blendshapes > 0) {
        u32 total_deltas = submesh->len_blendshapes * submesh->len_vertices * 6;
        total_size += sizeof(f32) * total_deltas; // blendshape_deltas
      }

      // material_path
      total_size += sizeof(u32); // material_path length
      total_size += submesh->material_path.len;
    }
  }

  // len_joints
  total_size += sizeof(u32);
  for (u32 i = 0; i < model->len_joints; i++) {
    total_size += sizeof(i32);  // parent_index
    total_size += sizeof(mat4); // inverse_bind_matrix
    total_size += sizeof(u32);  // children.len
    total_size += sizeof(u32) * model->joints[i].children.len; // children.items
  }
  // joint_names - length + string data for each joint
  for (u32 i = 0; i < model->len_joints; i++) {
    total_size += sizeof(u32);               // string length
    total_size += model->joint_names[i].len; // string data
  }

  BinaryWriter writer = {.cur_offset = 0,
                         .len = total_size,
                         .bytes = ALLOC_ARRAY(allocator, u8, total_size)};
  assert(writer.bytes);

  write_u32(&writer, model->version);
  write_u32(&writer, model->num_meshes);

  for (u32 i = 0; i < model->num_meshes; i++) {
    MeshData *mesh = &model->meshes[i];

    // write mesh_name
    write_u32(&writer, mesh->mesh_name.len);
    write_u8(&writer, (u8 *)mesh->mesh_name.value, mesh->mesh_name.len);

    // write blendshape_names array
    write_u32(&writer, mesh->blendshape_names.len);
    if (mesh->blendshape_names.len > 0) {
      for (u32 j = 0; j < mesh->blendshape_names.len; j++) {
        write_u8(&writer, (u8 *)&mesh->blendshape_names.items[j],
                 sizeof(String32Bytes));
      }
    }

    // write submeshes array
    write_u32(&writer, mesh->submeshes.len);
    for (u32 j = 0; j < mesh->submeshes.len; j++) {
      SubMeshData *submesh = &mesh->submeshes.items[j];

      write_u32(&writer, submesh->len_vertices);
      write_u32(&writer, submesh->vertex_stride);
      write_u32(&writer, submesh->len_vertex_buffer);
      write_u8(&writer, submesh->vertex_buffer, submesh->len_vertex_buffer);

      // indices
      write_u32(&writer, submesh->len_indices);
      write_u32_array(&writer, submesh->indices, submesh->len_indices);

      // blendshape data
      write_u32(&writer, submesh->len_blendshapes);
      if (submesh->len_blendshapes > 0) {
        u32 total_deltas = submesh->len_blendshapes * submesh->len_vertices * 6;
        write_f32_array(&writer, submesh->blendshape_deltas, total_deltas);
      }

      // write material_path
      write_u32(&writer, submesh->material_path.len);
      if (submesh->material_path.value) {
        write_u8(&writer, (u8 *)submesh->material_path.value,
                 submesh->material_path.len);
      }
    }
  }

  write_u32(&writer, model->len_joints);
  for (u32 i = 0; i < model->len_joints; i++) {
    Joint *joint = &model->joints[i];
    write_i32(&writer, joint->parent_index);
    write_f32_array(&writer, (f32 *)joint->inverse_bind_matrix, 16);
    write_u32(&writer, joint->children.len);
    write_u32_array(&writer, joint->children.items, joint->children.len);
  }

  for (u32 i = 0; i < model->len_joints; i++) {
    String *joint_name = &model->joint_names[i];
    write_u32(&writer, joint_name->len);
    write_u8(&writer, (u8 *)joint_name->value, joint_name->len);
  }

  assert(writer.cur_offset == writer.len);
  *buffer = writer.bytes;
  *buffer_size = writer.len;
  return true;
}

b32 write_MaterialAsset(const MaterialAsset *material, Allocator *allocator,
                        _out_ u8 **buffer, _out_ u32 *buffer_size) {
  if (!material || !allocator) {
    return false;
  }

  // Calculate total size
  size_t total_size = 0;

  // Material name
  total_size += sizeof(u32); // name length
  total_size += material->name.len;

  // Shader path
  total_size += sizeof(u32); // shader_path length
  total_size += material->shader_path.len;

  // Transparency flag
  total_size += sizeof(b32);

  // Shader defines
  total_size += sizeof(u32); // shader_defines.len
  for (u32 i = 0; i < material->shader_defines.len; i++) {
    ShaderDefine *define = &material->shader_defines.items[i];
    total_size += sizeof(u32); // name length
    total_size += define->name.len;
    total_size += sizeof(u32); // type
    total_size += sizeof(b32); // value (for boolean defines)
  }

  // Properties
  total_size += sizeof(u32); // properties.len
  for (u32 i = 0; i < material->properties.len; i++) {
    MaterialAssetProperty *prop = &material->properties.items[i];
    total_size += sizeof(u32); // name length
    total_size += prop->name.len;
    total_size += sizeof(u32); // type
    if (prop->type == MAT_PROP_TEXTURE) {
      total_size += sizeof(u32); // texture_path length
      total_size += prop->texture_path.len;
    } else if (prop->type == MAT_PROP_VEC3) {
      total_size += sizeof(Color); // color
    }
  }

  BinaryWriter writer = {.cur_offset = 0,
                         .len = total_size,
                         .bytes = ALLOC_ARRAY(allocator, u8, total_size)};
  assert(writer.bytes);

  // Write material name
  write_u32(&writer, material->name.len);
  write_u8(&writer, (u8 *)material->name.value, material->name.len);

  // Write shader path
  write_u32(&writer, material->shader_path.len);
  write_u8(&writer, (u8 *)material->shader_path.value,
           material->shader_path.len);

  // Write transparency
  write_u32(&writer, material->transparent);

  // Write shader defines
  write_u32(&writer, material->shader_defines.len);
  for (u32 i = 0; i < material->shader_defines.len; i++) {
    ShaderDefine *define = &material->shader_defines.items[i];
    write_u32(&writer, define->name.len);
    write_u8(&writer, (u8 *)define->name.value, define->name.len);
    write_u32(&writer, define->type);
    write_u32(&writer, define->value.flag);
  }

  // Write properties
  write_u32(&writer, material->properties.len);
  for (u32 i = 0; i < material->properties.len; i++) {
    MaterialAssetProperty *prop = &material->properties.items[i];
    write_u32(&writer, prop->name.len);
    write_u8(&writer, (u8 *)prop->name.value, prop->name.len);
    write_u32(&writer, prop->type);

    if (prop->type == MAT_PROP_TEXTURE) {
      write_u32(&writer, prop->texture_path.len);
      write_u8(&writer, (u8 *)prop->texture_path.value, prop->texture_path.len);
    } else if (prop->type == MAT_PROP_VEC3) {
      write_f32_array(&writer, (f32 *)&prop->color, 4); // Color has 4 floats
    }
  }

  assert(writer.cur_offset == writer.len);
  *buffer = writer.bytes;
  *buffer_size = writer.len;
  return true;
}

MaterialAsset *read_MaterialAsset(const u8 *binary_data, u32 data_len,
                                  Allocator *allocator) {
  if (!binary_data || data_len == 0 || !allocator) {
    return NULL;
  }

  BinaryReader reader = {
      .bytes = binary_data, .len = data_len, .cur_offset = 0};

  MaterialAsset *material = ALLOC(allocator, MaterialAsset);
  if (!material) {
    return NULL;
  }

  // Read material name
  u32 name_len;
  read_u32(&reader, &name_len);
  material->name.len = name_len;
  material->name.value = ALLOC_ARRAY(allocator, char, name_len + 1);
  read_u8_array(&reader, (u8 *)material->name.value, name_len);
  material->name.value[name_len] = '\0';

  // Read shader path
  u32 shader_path_len;
  read_u32(&reader, &shader_path_len);
  material->shader_path.len = shader_path_len;
  material->shader_path.value =
      ALLOC_ARRAY(allocator, char, shader_path_len + 1);
  read_u8_array(&reader, (u8 *)material->shader_path.value, shader_path_len);
  material->shader_path.value[shader_path_len] = '\0';

  // Read transparency
  u32 transparent;
  read_u32(&reader, &transparent);
  material->transparent = transparent;

  // Read shader defines
  u32 shader_defines_len;
  read_u32(&reader, &shader_defines_len);
  material->shader_defines =
      arr_new_ALLOC(allocator, ShaderDefine, shader_defines_len);

  for (u32 i = 0; i < shader_defines_len; i++) {
    ShaderDefine *define = &material->shader_defines.items[i];

    u32 define_name_len;
    read_u32(&reader, &define_name_len);
    define->name.len = define_name_len;
    define->name.value = ALLOC_ARRAY(allocator, char, define_name_len + 1);
    read_u8_array(&reader, (u8 *)define->name.value, define_name_len);
    define->name.value[define_name_len] = '\0';

    read_u32(&reader, (u32 *)&define->type);

    u32 flag_value;
    read_u32(&reader, &flag_value);
    define->value.flag = flag_value;
  }

  // Read properties
  u32 properties_len;
  read_u32(&reader, &properties_len);
  material->properties =
      arr_new_ALLOC(allocator, MaterialAssetProperty, properties_len);

  for (u32 i = 0; i < properties_len; i++) {
    MaterialAssetProperty *prop = &material->properties.items[i];

    u32 prop_name_len;
    read_u32(&reader, &prop_name_len);
    prop->name.len = prop_name_len;
    prop->name.value = ALLOC_ARRAY(allocator, char, prop_name_len + 1);
    read_u8_array(&reader, (u8 *)prop->name.value, prop_name_len);
    prop->name.value[prop_name_len] = '\0';

    read_u32(&reader, (u32 *)&prop->type);

    if (prop->type == MAT_PROP_TEXTURE) {
      u32 texture_path_len;
      read_u32(&reader, &texture_path_len);
      prop->texture_path.len = texture_path_len;
      prop->texture_path.value =
          ALLOC_ARRAY(allocator, char, texture_path_len + 1);
      read_u8_array(&reader, (u8 *)prop->texture_path.value, texture_path_len);
      prop->texture_path.value[texture_path_len] = '\0';
    } else if (prop->type == MAT_PROP_VEC3) {
      read_f32_array(&reader, (f32 *)&prop->color, 4);
    }
  }

  assert(reader.cur_offset == reader.len);
  return material;
}
