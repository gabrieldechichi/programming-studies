meta:
  id: model_blob_asset
  file-extension: hasset
  endian: le

seq:
  - id: header
    type: blob_asset_header
  - id: mesh_count
    type: u4
  - id: meshes_ptr
    type: blob_array

instances:
  meshes:
    pos: meshes_ptr.offset
    type: mesh_blob_asset(_index)
    repeat: expr
    repeat-expr: mesh_count

types:
  blob_asset_header:
    seq:
      - id: version
        type: u4
      - id: padding1
        type: u4
      - id: asset_size
        type: u8
      - id: asset_type_hash
        type: u8
      - id: dependency_count
        type: u4
      - id: padding2
        type: u4

  blob_array:
    seq:
      - id: offset
        type: u4
      - id: size
        type: u4
      - id: type_size
        type: u4
      - id: typehash
        type: u4

  string_blob:
    params:
      - id: base_offset
        type: u4
    seq:
      - id: len
        type: u4
      - id: offset
        type: u4
    instances:
      value:
        pos: base_offset + offset
        size: len
        type: str
        encoding: ASCII

  mesh_blob_asset:
    params:
      - id: mesh_index
        type: u4
    seq:
      - id: name
        type: string_blob(_root.meshes_ptr.offset + mesh_index * 100)
      - id: index_format
        type: u4
        enum: index_format_enum
      - id: index_count
        type: u4
      - id: vertex_count
        type: u4
      - id: indices_ptr
        type: blob_array
      - id: positions_ptr
        type: blob_array
      - id: normals_ptr
        type: blob_array
      - id: tangents_ptr
        type: blob_array
      - id: uvs_ptr
        type: blob_array
    instances:
      mesh_base:
        value: _root.meshes_ptr.offset + mesh_index * 100
      indices:
        pos: mesh_base + indices_ptr.offset
        size: indices_ptr.size
      positions:
        pos: mesh_base + positions_ptr.offset
        type: f4
        repeat: expr
        repeat-expr: vertex_count * 3
      normals:
        pos: mesh_base + normals_ptr.offset
        type: f4
        repeat: expr
        repeat-expr: vertex_count * 3
      tangents:
        pos: mesh_base + tangents_ptr.offset
        type: f4
        repeat: expr
        repeat-expr: vertex_count * 4
      uvs:
        pos: mesh_base + uvs_ptr.offset
        type: f4
        repeat: expr
        repeat-expr: vertex_count * 2

enums:
  index_format_enum:
    0: u16
    1: u32
