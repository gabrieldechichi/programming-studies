#include "renderer.h"
#include "gpu.h"
#include "lib/handle.h"
#include "lib/hash.h"
#include "lib/thread_context.h"

#define MAX_RENDER_CMDS 1024
#define MAX_MESHES 64
#define MAX_MATERIALS 64
#define MAX_INSTANCE_BUFFERS 16

typedef struct {
  HandleArray_GpuMesh meshes;
  HandleArray_Material materials;
  HandleArray_InstanceBuffer instance_buffers;

  // Per-frame state
  // todo: move to camera uniforms, send to shader
  mat4 view;
  mat4 proj;
  mat4 view_proj;

  // Per-thread command queues (no atomics needed)
  u8 thread_count;
  // todo: maybe pass thread_count on renderer_init?
  DynArray(RenderCmd) thread_cmds[32]; // MAX_THREADS

  GpuTexture placeholder_texture;
} RendererState;

global RendererState g_renderer;

void renderer_init(ArenaAllocator *arena, u8 thread_count) {
  Allocator alloc = make_arena_allocator(arena);

  gpu_init(arena, GPU_UNIFORM_BUFFER_SIZE);
  g_renderer.meshes = ha_init(GpuMesh, &alloc, MAX_MESHES);
  g_renderer.materials = ha_init(Material, &alloc, MAX_MATERIALS);
  g_renderer.instance_buffers = ha_init(InstanceBuffer, &alloc, MAX_INSTANCE_BUFFERS);

  g_renderer.thread_count = thread_count;
  assert_msg(g_renderer.thread_count > 0, "Thread count can't be zero");

  u32 cmds_per_thread = MAX_RENDER_CMDS / g_renderer.thread_count;
  for (u8 i = 0; i < g_renderer.thread_count; i++) {
    g_renderer.thread_cmds[i] = (RenderCmd_DynArray){
        .len = 0,
        .cap = cmds_per_thread,
        .items = ARENA_ALLOC_ARRAY(arena, RenderCmd, cmds_per_thread),
    };
  }

#ifdef DEBUG
  u8 placeholder_pixels[4] = {255, 0, 255, 255};
#else
  u8 placeholder_pixels[4] = {255, 255, 255, 255};
#endif
  g_renderer.placeholder_texture = gpu_make_texture_data(1, 1, placeholder_pixels);
}

GpuMesh_Handle renderer_upload_mesh(MeshDesc *desc) {
  // Create GPU buffers
  GpuBuffer vbuf = gpu_make_buffer(&(GpuBufferDesc){
      .type = GPU_BUFFER_VERTEX,
      .size = desc->vertex_size,
      .data = desc->vertices,
  });

  GpuBuffer ibuf = gpu_make_buffer(&(GpuBufferDesc){
      .type = GPU_BUFFER_INDEX,
      .size = desc->index_size,
      .data = desc->indices,
  });

  // Create mesh and add to storage
  GpuMesh mesh = {
      .vbuf = vbuf,
      .ibuf = ibuf,
      .index_count = desc->index_count,
      .index_format = desc->index_format,
  };

  return ha_add(Mesh, &g_renderer.meshes, mesh);
}

Material_Handle renderer_create_material(MaterialDesc *desc) {
  Material material = {0};
  // todo: check if shader exists first
  material.shader = gpu_make_shader(&desc->shader_desc);
  material.pipeline = gpu_make_pipeline(&(GpuPipelineDesc){
      .shader = material.shader,
      .vertex_layout = desc->vertex_layout,
      .primitive = desc->primitive,
      .depth_test = desc->depth_test,
      .depth_write = desc->depth_write,
  });

  // Cache property slots
  material.property_count = desc->property_count;
  for (u8 i = 0; i < desc->property_count; i++) {
    MaterialPropertyDesc *p = &desc->properties[i];
    material.properties[i] = (MaterialProperty){
        .name_hash = fnv1a_hash(p->name),
        .binding = p->binding,
        .type = p->type,
    };
  }

  return ha_add(Material, &g_renderer.materials, material);
}

static MaterialProperty *find_property(Material *mat, const char *name) {
  u32 hash = fnv1a_hash(name);
  for (u8 i = 0; i < mat->property_count; i++) {
    if (mat->properties[i].name_hash == hash) {
      return &mat->properties[i];
    }
  }
  return NULL;
}

void material_set_float(Material_Handle handle, const char *name, f32 value) {
  Material *mat = ha_get(Material, &g_renderer.materials, handle);
  if (!mat) return;
  MaterialProperty *prop = find_property(mat, name);
  if (!prop || prop->type != MAT_PROP_FLOAT) return;
  prop->f = value;
}

void material_set_vec4(Material_Handle handle, const char *name, vec4 value) {
  Material *mat = ha_get(Material, &g_renderer.materials, handle);
  if (!mat) return;
  MaterialProperty *prop = find_property(mat, name);
  if (!prop || prop->type != MAT_PROP_VEC4) return;
  glm_vec4_copy(value, prop->v4);
}

void material_set_texture(Material_Handle handle, const char *name, GpuTexture tex) {
  Material *mat = ha_get(Material, &g_renderer.materials, handle);
  if (!mat) return;
  MaterialProperty *prop = find_property(mat, name);
  if (!prop || prop->type != MAT_PROP_TEXTURE) return;
  prop->tex = tex;
}

InstanceBuffer_Handle renderer_create_instance_buffer(InstanceBufferDesc *desc) {
  u32 size = desc->stride * desc->max_instances;
  GpuBuffer buf = gpu_make_buffer(&(GpuBufferDesc){
      .type = GPU_BUFFER_STORAGE,
      .size = size,
      .data = NULL,
  });

  InstanceBuffer ib = {
      .buffer = buf,
      .stride = desc->stride,
      .max_instances = desc->max_instances,
      .instance_count = 0,
  };

  return ha_add(InstanceBuffer, &g_renderer.instance_buffers, ib);
}

void renderer_update_instance_buffer(InstanceBuffer_Handle handle, void *data,
                                     u32 instance_count) {
  InstanceBuffer *ib =
      ha_get(InstanceBuffer, &g_renderer.instance_buffers, handle);
  if (!ib) return;

  assert(instance_count <= ib->max_instances);
  ib->instance_count = instance_count;

  u32 size = ib->stride * instance_count;
  gpu_update_buffer(ib->buffer, data, size);
}

// todo: separate call for camera uniforms
void renderer_begin_frame(mat4 view, mat4 proj, GpuColor clear_color) {
  debug_assert_msg(
      is_main_thread(),
      "renderer_begin_frame can only be called from the main thread");

  // Store view/proj for MVP computation
  memcpy(g_renderer.view, view, sizeof(mat4));
  memcpy(g_renderer.proj, proj, sizeof(mat4));
  mat4_mul(proj, view, g_renderer.view_proj);

  // Reset all thread command arrays
  for (u8 i = 0; i < g_renderer.thread_count; i++) {
    g_renderer.thread_cmds[i].len = 0;
  }

  // Begin GPU pass (also resets internal uniform buffer)
  gpu_begin_pass(&(GpuPassDesc){
      .clear_color = clear_color,
      .clear_depth = 1.0f,
  });
}

void renderer_draw_mesh(GpuMesh_Handle mesh, Material_Handle material,
                        mat4 model_matrix) {
  RenderCmd cmd = {
      .type = RENDER_CMD_DRAW_MESH,
      .draw_mesh.mesh = mesh,
      .draw_mesh.material = material,
  };
  memcpy(cmd.draw_mesh.model_matrix, model_matrix, sizeof(mat4));

  // Append to current thread's command array (no atomic!)
  u8 tid = tctx_current()->thread_idx;
  arr_append(g_renderer.thread_cmds[tid], cmd);
}

void renderer_draw_mesh_instanced(GpuMesh_Handle mesh, Material_Handle material,
                                  InstanceBuffer_Handle instances) {
  RenderCmd cmd = {
      .type = RENDER_CMD_DRAW_MESH_INSTANCED,
      .draw_mesh_instanced = {
          .mesh = mesh,
          .material = material,
          .instances = instances,
      },
  };

  // Append to current thread's command array (no atomic!)
  u8 tid = tctx_current()->thread_idx;
  arr_append(g_renderer.thread_cmds[tid], cmd);
}

void renderer_end_frame(void) {
  debug_assert_msg(
      is_main_thread(),
      "renderer_end_frame can only be called from the main thread");

  Material *last_applied_material = NULL;

  // Process commands from all threads
  for (u8 t = 0; t < g_renderer.thread_count; t++) {
    DynArray(RenderCmd) *cmds = &g_renderer.thread_cmds[t];

    for (u32 i = 0; i < cmds->len; i++) {
      RenderCmd *cmd = &cmds->items[i];

      switch (cmd->type) {
      case RENDER_CMD_DRAW_MESH: {
        GpuMesh *mesh =
            ha_get(GpuMesh, &g_renderer.meshes, cmd->draw_mesh.mesh);
        if (!mesh)
          continue;

        Material *material =
            ha_get(Material, &g_renderer.materials, cmd->draw_mesh.material);
        assert(material);
        if (material != last_applied_material) {
          gpu_apply_pipeline(material->pipeline);
          last_applied_material = material;
        }

        // Build GlobalUniforms for this draw
        GlobalUniforms globals;
        memcpy(globals.model, cmd->draw_mesh.model_matrix, sizeof(mat4));
        memcpy(globals.view, g_renderer.view, sizeof(mat4));
        memcpy(globals.proj, g_renderer.proj, sizeof(mat4));
        memcpy(globals.view_proj, g_renderer.view_proj, sizeof(mat4));

        // Set uniform data for slot 0 (GlobalUniforms)
        gpu_apply_uniforms(0, &globals, sizeof(GlobalUniforms));

        GpuTexture mat_textures[GPU_MAX_TEXTURE_SLOTS];
        u32 mat_texture_count = 0;

        for (u8 p = 0; p < material->property_count; p++) {
          MaterialProperty *prop = &material->properties[p];
          if (prop->type == MAT_PROP_TEXTURE) {
            GpuTexture tex = prop->tex;
            if (!gpu_texture_is_ready(tex)) {
              tex = g_renderer.placeholder_texture;
            }
            mat_textures[mat_texture_count++] = tex;
          } else {
            void *data;
            u32 size;
            switch (prop->type) {
              case MAT_PROP_FLOAT: data = &prop->f;  size = sizeof(f32);  break;
              case MAT_PROP_VEC2:  data = prop->v2;  size = sizeof(vec2); break;
              case MAT_PROP_VEC3:  data = prop->v3;  size = sizeof(vec3); break;
              case MAT_PROP_VEC4:  data = prop->v4;  size = sizeof(vec4); break;
              case MAT_PROP_MAT4:  data = prop->m4;  size = sizeof(mat4); break;
              default: continue;
            }
            gpu_apply_uniforms(prop->binding, data, size);
          }
        }

        GpuBindings bindings = {
            .vertex_buffers = {mesh->vbuf},
            .vertex_buffer_count = 1,
            .index_buffer = mesh->ibuf,
            .index_format = mesh->index_format,
            .texture_count = mat_texture_count,
        };
        for (u32 t = 0; t < mat_texture_count; t++) {
          bindings.textures[t] = mat_textures[t];
        }
        gpu_apply_bindings(&bindings);

        gpu_draw_indexed(mesh->index_count, 1);
        break;
      }

      case RENDER_CMD_DRAW_MESH_INSTANCED: {
        GpuMesh *mesh =
            ha_get(GpuMesh, &g_renderer.meshes, cmd->draw_mesh_instanced.mesh);
        if (!mesh)
          continue;

        Material *material = ha_get(Material, &g_renderer.materials,
                                    cmd->draw_mesh_instanced.material);
        assert(material);

        InstanceBuffer *ib = ha_get(InstanceBuffer, &g_renderer.instance_buffers,
                                    cmd->draw_mesh_instanced.instances);
        if (!ib || ib->instance_count == 0)
          continue;

        if (material != last_applied_material) {
          gpu_apply_pipeline(material->pipeline);
          last_applied_material = material;
        }

        // Build GlobalUniforms (model matrix unused for instanced - shader reads from storage)
        GlobalUniforms globals;
        mat4_identity(globals.model);
        memcpy(globals.view, g_renderer.view, sizeof(mat4));
        memcpy(globals.proj, g_renderer.proj, sizeof(mat4));
        memcpy(globals.view_proj, g_renderer.view_proj, sizeof(mat4));

        gpu_apply_uniforms(0, &globals, sizeof(GlobalUniforms));

        // Apply material properties
        for (u8 p = 0; p < material->property_count; p++) {
          MaterialProperty *prop = &material->properties[p];
          void *data;
          u32 size;
          switch (prop->type) {
            case MAT_PROP_FLOAT: data = &prop->f;  size = sizeof(f32);  break;
            case MAT_PROP_VEC2:  data = prop->v2;  size = sizeof(vec2); break;
            case MAT_PROP_VEC3:  data = prop->v3;  size = sizeof(vec3); break;
            case MAT_PROP_VEC4:  data = prop->v4;  size = sizeof(vec4); break;
            case MAT_PROP_MAT4:  data = prop->m4;  size = sizeof(mat4); break;
            default: continue;
          }
          gpu_apply_uniforms(prop->binding, data, size);
        }

        // Apply vertex/index buffer bindings with storage buffer for instance data
        gpu_apply_bindings(&(GpuBindings){
            .vertex_buffers = {mesh->vbuf},
            .vertex_buffer_count = 1,
            .index_buffer = mesh->ibuf,
            .index_format = mesh->index_format,
            .storage_buffers = {ib->buffer},
            .storage_buffer_count = 1,
        });

        gpu_draw_indexed(mesh->index_count, ib->instance_count);
        break;
      }
      }
    }
  }

  gpu_end_pass();
  gpu_commit(); // Flushes uniforms internally before submit
}
