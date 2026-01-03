#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define COBJMACROS
#include <windows.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>

#include "gpu_backend.h"
#include "gpu.h"
#include "os/os.h"
#include "lib/thread_context.h"
#include "stb/stb_image.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#include "shaders/blit_vs_d3d11.h"
#include "shaders/blit_fs_d3d11.h"

static const IID D3D11_IID_IDXGIDevice = { 0x54ec77fa, 0x1377, 0x44e6, {0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c} };
static const IID D3D11_IID_IDXGIFactory2 = { 0x50c83a1c, 0xe072, 0x4c48, {0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0} };
static const IID D3D11_IID_ID3D11Texture2D = { 0x6f15aaf2, 0xd208, 0x4e89, {0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c} };
static const IID D3D11_IID_ID3D11DeviceContext1 = { 0xbb2c6faa, 0xb5fb, 0x4082, {0x8e, 0x6b, 0x38, 0x8b, 0x8c, 0xfa, 0x90, 0xe1} };

#define D3D11_MAX_BUFFERS 256
#define D3D11_MAX_TEXTURES 128
#define D3D11_MAX_SHADERS 64
#define D3D11_MAX_PIPELINES 64
#define D3D11_MAX_RENDER_TARGETS 32

typedef struct {
    ID3D11Buffer *buffer;
    ID3D11ShaderResourceView *srv;
    GpuBufferType type;
} D3D11Buffer;

typedef struct {
    ID3D11Texture2D *texture;
    ID3D11ShaderResourceView *srv;
    ID3D11SamplerState *sampler;
    b32 ready;
} D3D11Texture;

typedef struct {
    ID3D11VertexShader *vs;
    ID3D11PixelShader *ps;
    void *vs_blob;
    size_t vs_blob_len;
} D3D11Shader;

typedef struct {
    u32 shader_idx;
    ID3D11InputLayout *input_layout;
    ID3D11RasterizerState *rasterizer;
    ID3D11DepthStencilState *depth_stencil;
    ID3D11BlendState *blend;
    D3D11_PRIMITIVE_TOPOLOGY topology;
    UINT vb_strides[GPU_MAX_VERTEX_BUFFERS];
    UINT ub_sizes[GPU_MAX_UNIFORMBLOCK_SLOTS];
    GpuShaderStage tex_stages[GPU_MAX_TEXTURE_SLOTS];
    u32 tex_bindings[GPU_MAX_TEXTURE_SLOTS];
    u32 sampler_bindings[GPU_MAX_TEXTURE_SLOTS];
    u32 tex_count;
} D3D11Pipeline;

typedef struct {
    ID3D11Texture2D *texture;
    ID3D11RenderTargetView *rtv;
    ID3D11ShaderResourceView *srv;
    ID3D11Texture2D *depth_texture;
    ID3D11DepthStencilView *dsv;
    u32 width;
    u32 height;
    GpuTextureFormat format;
} D3D11RenderTarget;

typedef struct {
    ID3D11Device *device;
    ID3D11DeviceContext *context;
    ID3D11DeviceContext1 *context1;
    IDXGISwapChain1 *swapchain;

    ID3D11RenderTargetView *backbuffer_rtv;
    ID3D11Texture2D *backbuffer_depth;
    ID3D11DepthStencilView *backbuffer_dsv;

    u32 width;
    u32 height;
    b32 vsync;

    D3D11Buffer buffers[D3D11_MAX_BUFFERS];
    D3D11Texture textures[D3D11_MAX_TEXTURES];
    D3D11Shader shaders[D3D11_MAX_SHADERS];
    D3D11Pipeline pipelines[D3D11_MAX_PIPELINES];
    D3D11RenderTarget render_targets[D3D11_MAX_RENDER_TARGETS];

    ID3D11RenderTargetView *current_rtv;
    ID3D11DepthStencilView *current_dsv;
    u32 current_rt_width;
    u32 current_rt_height;

    u32 current_pipeline_idx;
    DXGI_FORMAT current_index_format;

    // Blit resources (created lazily)
    ID3D11VertexShader *blit_vs;
    ID3D11PixelShader *blit_ps;
    ID3D11SamplerState *blit_sampler;
    ID3D11RasterizerState *blit_rasterizer;
    ID3D11DepthStencilState *blit_depth_stencil;
    ID3D11BlendState *blit_blend;
} D3D11State;

local_persist D3D11State d3d11;

internal DXGI_FORMAT d3d11_vertex_format(GpuVertexFormat fmt) {
    switch (fmt) {
        case GPU_VERTEX_FORMAT_FLOAT2: return DXGI_FORMAT_R32G32_FLOAT;
        case GPU_VERTEX_FORMAT_FLOAT3: return DXGI_FORMAT_R32G32B32_FLOAT;
        case GPU_VERTEX_FORMAT_FLOAT4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        default: return DXGI_FORMAT_UNKNOWN;
    }
}

internal DXGI_FORMAT d3d11_index_format(GpuIndexFormat fmt) {
    switch (fmt) {
        case GPU_INDEX_FORMAT_U16: return DXGI_FORMAT_R16_UINT;
        case GPU_INDEX_FORMAT_U32: return DXGI_FORMAT_R32_UINT;
        default: return DXGI_FORMAT_R16_UINT;
    }
}

internal D3D11_PRIMITIVE_TOPOLOGY d3d11_topology(GpuPrimitiveTopology topo) {
    switch (topo) {
        case GPU_PRIMITIVE_TRIANGLES: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case GPU_PRIMITIVE_LINES: return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        default: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

internal D3D11_CULL_MODE d3d11_cull_mode(GpuCullMode mode) {
    switch (mode) {
        case GPU_CULL_BACK:  return D3D11_CULL_BACK;
        case GPU_CULL_NONE:  return D3D11_CULL_NONE;
        case GPU_CULL_FRONT: return D3D11_CULL_FRONT;
        default: return D3D11_CULL_BACK;
    }
}

internal D3D11_COMPARISON_FUNC d3d11_compare_func(GpuCompareFunc func) {
    switch (func) {
        case GPU_COMPARE_LESS:          return D3D11_COMPARISON_LESS;
        case GPU_COMPARE_NEVER:         return D3D11_COMPARISON_NEVER;
        case GPU_COMPARE_EQUAL:         return D3D11_COMPARISON_EQUAL;
        case GPU_COMPARE_LESS_EQUAL:    return D3D11_COMPARISON_LESS_EQUAL;
        case GPU_COMPARE_GREATER:       return D3D11_COMPARISON_GREATER;
        case GPU_COMPARE_NOT_EQUAL:     return D3D11_COMPARISON_NOT_EQUAL;
        case GPU_COMPARE_GREATER_EQUAL: return D3D11_COMPARISON_GREATER_EQUAL;
        case GPU_COMPARE_ALWAYS:        return D3D11_COMPARISON_ALWAYS;
        default: return D3D11_COMPARISON_LESS;
    }
}

internal D3D11_BLEND d3d11_blend_factor(GpuBlendFactor factor) {
    switch (factor) {
        case GPU_BLEND_ZERO:                 return D3D11_BLEND_ZERO;
        case GPU_BLEND_ONE:                  return D3D11_BLEND_ONE;
        case GPU_BLEND_SRC_COLOR:            return D3D11_BLEND_SRC_COLOR;
        case GPU_BLEND_ONE_MINUS_SRC_COLOR:  return D3D11_BLEND_INV_SRC_COLOR;
        case GPU_BLEND_SRC_ALPHA:            return D3D11_BLEND_SRC_ALPHA;
        case GPU_BLEND_ONE_MINUS_SRC_ALPHA:  return D3D11_BLEND_INV_SRC_ALPHA;
        case GPU_BLEND_DST_COLOR:            return D3D11_BLEND_DEST_COLOR;
        case GPU_BLEND_ONE_MINUS_DST_COLOR:  return D3D11_BLEND_INV_DEST_COLOR;
        case GPU_BLEND_DST_ALPHA:            return D3D11_BLEND_DEST_ALPHA;
        case GPU_BLEND_ONE_MINUS_DST_ALPHA:  return D3D11_BLEND_INV_DEST_ALPHA;
        default: return D3D11_BLEND_ONE;
    }
}

internal D3D11_BLEND_OP d3d11_blend_op(GpuBlendOp op) {
    switch (op) {
        case GPU_BLEND_OP_ADD:              return D3D11_BLEND_OP_ADD;
        case GPU_BLEND_OP_SUBTRACT:         return D3D11_BLEND_OP_SUBTRACT;
        case GPU_BLEND_OP_REVERSE_SUBTRACT: return D3D11_BLEND_OP_REV_SUBTRACT;
        default: return D3D11_BLEND_OP_ADD;
    }
}

internal void d3d11_create_backbuffer_views(void) {
    ID3D11Texture2D *backbuffer;
    IDXGISwapChain1_GetBuffer(d3d11.swapchain, 0, &D3D11_IID_ID3D11Texture2D, (void **)&backbuffer);
    ID3D11Device_CreateRenderTargetView(d3d11.device, (ID3D11Resource *)backbuffer, NULL, &d3d11.backbuffer_rtv);
    ID3D11Texture2D_Release(backbuffer);

    D3D11_TEXTURE2D_DESC depth_desc = {
        .Width = d3d11.width,
        .Height = d3d11.height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_DEPTH_STENCIL,
    };
    ID3D11Device_CreateTexture2D(d3d11.device, &depth_desc, NULL, &d3d11.backbuffer_depth);
    ID3D11Device_CreateDepthStencilView(d3d11.device, (ID3D11Resource *)d3d11.backbuffer_depth, NULL, &d3d11.backbuffer_dsv);
}

internal void d3d11_release_backbuffer_views(void) {
    if (d3d11.backbuffer_rtv) {
        ID3D11RenderTargetView_Release(d3d11.backbuffer_rtv);
        d3d11.backbuffer_rtv = NULL;
    }
    if (d3d11.backbuffer_dsv) {
        ID3D11DepthStencilView_Release(d3d11.backbuffer_dsv);
        d3d11.backbuffer_dsv = NULL;
    }
    if (d3d11.backbuffer_depth) {
        ID3D11Texture2D_Release(d3d11.backbuffer_depth);
        d3d11.backbuffer_depth = NULL;
    }
}

void gpu_backend_init(GpuPlatformDesc *desc) {
    HWND hwnd = (HWND)desc->window_handle;
    d3d11.width = desc->width;
    d3d11.height = desc->height;
    d3d11.vsync = desc->vsync;

    UINT flags = 0;
    if (desc->debug) {
        flags |= D3D11_CREATE_DEVICE_DEBUG;
    }

    D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL feature_level;

    HRESULT hr = D3D11CreateDevice(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        flags,
        feature_levels,
        1,
        D3D11_SDK_VERSION,
        &d3d11.device,
        &feature_level,
        &d3d11.context
    );

    if (FAILED(hr)) {
        LOG_ERROR("D3D11CreateDevice failed: %", FMT_UINT(hr));
        return;
    }

    hr = ID3D11DeviceContext_QueryInterface(d3d11.context, &D3D11_IID_ID3D11DeviceContext1, (void **)&d3d11.context1);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get ID3D11DeviceContext1: %", FMT_UINT(hr));
        return;
    }

    IDXGIDevice *dxgi_device;
    ID3D11Device_QueryInterface(d3d11.device, &D3D11_IID_IDXGIDevice, (void **)&dxgi_device);

    IDXGIAdapter *adapter;
    IDXGIDevice_GetAdapter(dxgi_device, &adapter);

    IDXGIFactory2 *factory;
    IDXGIAdapter_GetParent(adapter, &D3D11_IID_IDXGIFactory2, (void **)&factory);

    DXGI_SWAP_CHAIN_DESC1 sc_desc = {
        .Width = d3d11.width,
        .Height = d3d11.height,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 2,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH,
    };

    hr = IDXGIFactory2_CreateSwapChainForHwnd(factory, (IUnknown *)d3d11.device, hwnd, &sc_desc, NULL, NULL, &d3d11.swapchain);
    if (FAILED(hr)) {
        LOG_ERROR("CreateSwapChainForHwnd failed: %", FMT_UINT(hr));
        return;
    }

    IDXGIFactory2_Release(factory);
    IDXGIAdapter_Release(adapter);
    IDXGIDevice_Release(dxgi_device);

    d3d11_create_backbuffer_views();

    LOG_INFO("D3D11 backend initialized (%x%)", FMT_UINT(d3d11.width), FMT_UINT(d3d11.height));
}

void gpu_backend_shutdown(void) {
    d3d11_release_backbuffer_views();
    if (d3d11.swapchain) IDXGISwapChain1_Release(d3d11.swapchain);
    if (d3d11.context1) ID3D11DeviceContext1_Release(d3d11.context1);
    if (d3d11.context) ID3D11DeviceContext_Release(d3d11.context);
    if (d3d11.device) ID3D11Device_Release(d3d11.device);
}

void gpu_backend_make_buffer(u32 idx, GpuBufferDesc *desc) {
    D3D11_BUFFER_DESC buf_desc = {
        .ByteWidth = desc->size,
        .Usage = desc->data ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DYNAMIC,
        .CPUAccessFlags = desc->data ? 0 : D3D11_CPU_ACCESS_WRITE,
    };

    switch (desc->type) {
        case GPU_BUFFER_VERTEX:  buf_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER; break;
        case GPU_BUFFER_INDEX:   buf_desc.BindFlags = D3D11_BIND_INDEX_BUFFER; break;
        case GPU_BUFFER_UNIFORM: buf_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER; break;
        case GPU_BUFFER_STORAGE:
            buf_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            buf_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
            break;
    }

    D3D11_SUBRESOURCE_DATA init_data = {.pSysMem = desc->data};
    ID3D11Device_CreateBuffer(d3d11.device, &buf_desc, desc->data ? &init_data : NULL, &d3d11.buffers[idx].buffer);
    d3d11.buffers[idx].type = desc->type;
    d3d11.buffers[idx].srv = NULL;

    if (desc->type == GPU_BUFFER_STORAGE) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {
            .Format = DXGI_FORMAT_R32_TYPELESS,
            .ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX,
            .BufferEx = {
                .FirstElement = 0,
                .NumElements = desc->size / 4,
                .Flags = D3D11_BUFFEREX_SRV_FLAG_RAW,
            },
        };
        HRESULT hr = ID3D11Device_CreateShaderResourceView(d3d11.device,
            (ID3D11Resource *)d3d11.buffers[idx].buffer, &srv_desc, &d3d11.buffers[idx].srv);
        if (FAILED(hr)) {
            LOG_ERROR("CreateShaderResourceView for storage buffer failed: %", FMT_UINT(hr));
        }
    }
}

void gpu_backend_update_buffer(u32 idx, void *data, u32 size) {
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = ID3D11DeviceContext_Map(d3d11.context, (ID3D11Resource *)d3d11.buffers[idx].buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        memcpy(mapped.pData, data, size);
        ID3D11DeviceContext_Unmap(d3d11.context, (ID3D11Resource *)d3d11.buffers[idx].buffer, 0);
    }
}

void gpu_backend_destroy_buffer(u32 idx) {
    if (d3d11.buffers[idx].srv) {
        ID3D11ShaderResourceView_Release(d3d11.buffers[idx].srv);
        d3d11.buffers[idx].srv = NULL;
    }
    if (d3d11.buffers[idx].buffer) {
        ID3D11Buffer_Release(d3d11.buffers[idx].buffer);
        d3d11.buffers[idx].buffer = NULL;
    }
}

void gpu_backend_make_shader(u32 idx, GpuShaderDesc *desc) {
    D3D11Shader *shd = &d3d11.shaders[idx];
    HRESULT hr;

    const void *vs_bytecode = desc->vs_code;
    const u8 *vs_ptr = (const u8 *)vs_bytecode;
    size_t vs_len = vs_ptr[24] | (vs_ptr[25] << 8) | (vs_ptr[26] << 16) | (vs_ptr[27] << 24);

    hr = ID3D11Device_CreateVertexShader(d3d11.device, vs_bytecode, vs_len, NULL, &shd->vs);
    if (FAILED(hr)) {
        LOG_ERROR("CreateVertexShader failed: %", FMT_UINT(hr));
        return;
    }

    shd->vs_blob_len = vs_len;
    shd->vs_blob = (void *)vs_bytecode;

    const void *fs_bytecode = desc->fs_code;
    const u8 *fs_ptr = (const u8 *)fs_bytecode;
    size_t fs_len = fs_ptr[24] | (fs_ptr[25] << 8) | (fs_ptr[26] << 16) | (fs_ptr[27] << 24);

    hr = ID3D11Device_CreatePixelShader(d3d11.device, fs_bytecode, fs_len, NULL, &shd->ps);
    if (FAILED(hr)) {
        LOG_ERROR("CreatePixelShader failed: %", FMT_UINT(hr));
        return;
    }
}

void gpu_backend_destroy_shader(u32 idx) {
    D3D11Shader *shd = &d3d11.shaders[idx];
    if (shd->vs) ID3D11VertexShader_Release(shd->vs);
    if (shd->ps) ID3D11PixelShader_Release(shd->ps);
    memset(shd, 0, sizeof(*shd));
}

void gpu_backend_make_pipeline(u32 idx, GpuPipelineDesc *desc, GpuShaderSlot *shader) {
    D3D11Pipeline *pip = &d3d11.pipelines[idx];
    D3D11Shader *shd = &d3d11.shaders[desc->shader.idx];
    HRESULT hr;

    pip->shader_idx = desc->shader.idx;
    pip->topology = d3d11_topology(desc->primitive);

    for (u32 i = 0; i < shader->uniform_blocks.len; i++) {
        u32 binding = shader->uniform_blocks.items[i].binding;
        u32 size = shader->uniform_blocks.items[i].size;
        pip->ub_sizes[binding] = (size + 255) / 256 * 256;
    }

    pip->tex_count = (u32)shader->texture_bindings.len;
    for (u32 i = 0; i < shader->texture_bindings.len; i++) {
        pip->tex_stages[i] = shader->texture_bindings.items[i].stage;
        pip->tex_bindings[i] = shader->texture_bindings.items[i].texture_binding;
        pip->sampler_bindings[i] = shader->texture_bindings.items[i].sampler_binding;
    }

    D3D11_INPUT_ELEMENT_DESC input_elems[GPU_MAX_VERTEX_ATTRS];
    u32 attr_count = desc->vertex_layout.attrs.len;
    for (u32 i = 0; i < attr_count; i++) {
        GpuVertexAttr *attr = &desc->vertex_layout.attrs.items[i];
        input_elems[i] = (D3D11_INPUT_ELEMENT_DESC){
            .SemanticName = "TEXCOORD",
            .SemanticIndex = attr->shader_location,
            .Format = d3d11_vertex_format(attr->format),
            .InputSlot = 0,
            .AlignedByteOffset = attr->offset,
            .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
            .InstanceDataStepRate = 0,
        };
    }

    hr = ID3D11Device_CreateInputLayout(d3d11.device, input_elems, attr_count,
                                         shd->vs_blob, shd->vs_blob_len, &pip->input_layout);
    if (FAILED(hr)) {
        LOG_ERROR("CreateInputLayout failed: % (0x%)", FMT_UINT(hr), FMT_UINT(hr));
        return;
    }

    pip->vb_strides[0] = desc->vertex_layout.stride;

    D3D11_RASTERIZER_DESC rs_desc = {
        .FillMode = D3D11_FILL_SOLID,
        .CullMode = d3d11_cull_mode(desc->cull_mode),
        .FrontCounterClockwise = (desc->face_winding == GPU_FACE_CCW),
        .DepthClipEnable = TRUE,
        .ScissorEnable = FALSE,
    };
    hr = ID3D11Device_CreateRasterizerState(d3d11.device, &rs_desc, &pip->rasterizer);
    if (FAILED(hr)) {
        LOG_ERROR("CreateRasterizerState failed: %", FMT_UINT(hr));
        return;
    }

    D3D11_DEPTH_STENCIL_DESC ds_desc = {
        .DepthEnable = desc->depth_test,
        .DepthWriteMask = desc->depth_write ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO,
        .DepthFunc = d3d11_compare_func(desc->depth_compare),
        .StencilEnable = FALSE,
    };
    hr = ID3D11Device_CreateDepthStencilState(d3d11.device, &ds_desc, &pip->depth_stencil);
    if (FAILED(hr)) {
        LOG_ERROR("CreateDepthStencilState failed: %", FMT_UINT(hr));
        return;
    }

    GpuBlendState *blend = &desc->blend;
    D3D11_BLEND_DESC blend_desc = {
        .RenderTarget[0] = {
            .BlendEnable = blend->enabled,
            .SrcBlend = blend->enabled ? d3d11_blend_factor(blend->src_factor) : D3D11_BLEND_ONE,
            .DestBlend = blend->enabled ? d3d11_blend_factor(blend->dst_factor) : D3D11_BLEND_ZERO,
            .BlendOp = blend->enabled ? d3d11_blend_op(blend->op) : D3D11_BLEND_OP_ADD,
            .SrcBlendAlpha = blend->enabled ? d3d11_blend_factor(blend->src_factor_alpha) : D3D11_BLEND_ONE,
            .DestBlendAlpha = blend->enabled ? d3d11_blend_factor(blend->dst_factor_alpha) : D3D11_BLEND_ZERO,
            .BlendOpAlpha = blend->enabled ? d3d11_blend_op(blend->op_alpha) : D3D11_BLEND_OP_ADD,
            .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
        },
    };
    hr = ID3D11Device_CreateBlendState(d3d11.device, &blend_desc, &pip->blend);
    if (FAILED(hr)) {
        LOG_ERROR("CreateBlendState failed: %", FMT_UINT(hr));
        return;
    }
}

void gpu_backend_destroy_pipeline(u32 idx) {
    D3D11Pipeline *pip = &d3d11.pipelines[idx];
    if (pip->input_layout) ID3D11InputLayout_Release(pip->input_layout);
    if (pip->rasterizer) ID3D11RasterizerState_Release(pip->rasterizer);
    if (pip->depth_stencil) ID3D11DepthStencilState_Release(pip->depth_stencil);
    if (pip->blend) ID3D11BlendState_Release(pip->blend);
    memset(pip, 0, sizeof(*pip));
}

void gpu_backend_begin_pass(GpuPassDesc *desc) {
    if (handle_equals(desc->render_target, INVALID_HANDLE)) {
        d3d11.current_rtv = d3d11.backbuffer_rtv;
        d3d11.current_dsv = d3d11.backbuffer_dsv;
        d3d11.current_rt_width = d3d11.width;
        d3d11.current_rt_height = d3d11.height;
    } else {
        D3D11RenderTarget *rt = &d3d11.render_targets[desc->render_target.idx];
        d3d11.current_rtv = rt->rtv;
        d3d11.current_dsv = rt->dsv;
        d3d11.current_rt_width = rt->width;
        d3d11.current_rt_height = rt->height;
    }

    ID3D11DeviceContext_OMSetRenderTargets(d3d11.context, 1, &d3d11.current_rtv, d3d11.current_dsv);

    D3D11_VIEWPORT viewport = {
        .TopLeftX = 0,
        .TopLeftY = 0,
        .Width = (f32)d3d11.current_rt_width,
        .Height = (f32)d3d11.current_rt_height,
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f,
    };
    ID3D11DeviceContext_RSSetViewports(d3d11.context, 1, &viewport);

    f32 clear_color[4] = {desc->clear_color.r, desc->clear_color.g, desc->clear_color.b, desc->clear_color.a};
    ID3D11DeviceContext_ClearRenderTargetView(d3d11.context, d3d11.current_rtv, clear_color);
    ID3D11DeviceContext_ClearDepthStencilView(d3d11.context, d3d11.current_dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, desc->clear_depth, 0);
}

void gpu_backend_apply_pipeline(u32 handle_idx) {
    D3D11Pipeline *pip = &d3d11.pipelines[handle_idx];
    D3D11Shader *shd = &d3d11.shaders[pip->shader_idx];

    d3d11.current_pipeline_idx = handle_idx;

    ID3D11DeviceContext_IASetInputLayout(d3d11.context, pip->input_layout);
    ID3D11DeviceContext_IASetPrimitiveTopology(d3d11.context, pip->topology);
    ID3D11DeviceContext_VSSetShader(d3d11.context, shd->vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(d3d11.context, shd->ps, NULL, 0);
    ID3D11DeviceContext_RSSetState(d3d11.context, pip->rasterizer);
    ID3D11DeviceContext_OMSetDepthStencilState(d3d11.context, pip->depth_stencil, 0);
    ID3D11DeviceContext_OMSetBlendState(d3d11.context, pip->blend, NULL, 0xFFFFFFFF);
}

void gpu_backend_end_pass(void) {
}

void gpu_backend_commit(void) {
    IDXGISwapChain1_Present(d3d11.swapchain, d3d11.vsync ? 1 : 0, 0);
}

void gpu_backend_upload_uniforms(u32 buf_idx, void *data, u32 size) {
    gpu_backend_update_buffer(buf_idx, data, size);
}

void gpu_backend_apply_bindings(GpuBindings *bindings, u32 ub_idx, u32 ub_count, u32 *ub_offsets) {
    D3D11Pipeline *pip = &d3d11.pipelines[d3d11.current_pipeline_idx];

    ID3D11Buffer *vbs[GPU_MAX_VERTEX_BUFFERS] = {0};
    UINT vb_offsets[GPU_MAX_VERTEX_BUFFERS] = {0};
    for (u32 i = 0; i < bindings->vertex_buffers.len; i++) {
        vbs[i] = d3d11.buffers[bindings->vertex_buffers.items[i].idx].buffer;
    }
    ID3D11DeviceContext_IASetVertexBuffers(d3d11.context, 0, bindings->vertex_buffers.len,
                                            vbs, pip->vb_strides, vb_offsets);

    if (!handle_equals(bindings->index_buffer, INVALID_HANDLE)) {
        d3d11.current_index_format = d3d11_index_format(bindings->index_format);
        ID3D11DeviceContext_IASetIndexBuffer(d3d11.context,
            d3d11.buffers[bindings->index_buffer.idx].buffer,
            d3d11.current_index_format, 0);
    }

    ID3D11Buffer *ub = d3d11.buffers[ub_idx].buffer;
    for (u32 i = 0; i < ub_count; i++) {
        UINT first_constant = ub_offsets[i] / 16;
        UINT num_constants = pip->ub_sizes[i] / 16;
        if (num_constants == 0) num_constants = 16;
        ID3D11DeviceContext1_VSSetConstantBuffers1(d3d11.context1, i, 1, &ub, &first_constant, &num_constants);
        ID3D11DeviceContext1_PSSetConstantBuffers1(d3d11.context1, i, 1, &ub, &first_constant, &num_constants);
    }

    if (bindings->storage_buffers.len > 0) {
        ID3D11ShaderResourceView *srvs[GPU_MAX_STORAGE_BUFFER_SLOTS] = {0};
        for (u32 i = 0; i < bindings->storage_buffers.len; i++) {
            srvs[i] = d3d11.buffers[bindings->storage_buffers.items[i].idx].srv;
        }
        ID3D11DeviceContext_VSSetShaderResources(d3d11.context, 0, bindings->storage_buffers.len, srvs);
    }

    if (bindings->textures.len > 0) {
        u32 max_tex_slot = 0, max_sampler_slot = 0;
        ID3D11ShaderResourceView *vs_srvs[GPU_MAX_TEXTURE_SLOTS] = {0};
        ID3D11ShaderResourceView *ps_srvs[GPU_MAX_TEXTURE_SLOTS] = {0};
        ID3D11SamplerState *vs_samplers[GPU_MAX_TEXTURE_SLOTS] = {0};
        ID3D11SamplerState *ps_samplers[GPU_MAX_TEXTURE_SLOTS] = {0};

        for (u32 i = 0; i < bindings->textures.len; i++) {
            D3D11Texture *tex = &d3d11.textures[bindings->textures.items[i].idx];
            GpuShaderStage stage = pip->tex_stages[i];
            u32 tex_slot = pip->tex_bindings[i];
            u32 sampler_slot = pip->sampler_bindings[i];

            if (tex_slot >= max_tex_slot) max_tex_slot = tex_slot + 1;
            if (sampler_slot >= max_sampler_slot) max_sampler_slot = sampler_slot + 1;

            if (stage & GPU_STAGE_VERTEX) {
                vs_srvs[tex_slot] = tex->srv;
                vs_samplers[sampler_slot] = tex->sampler;
            }
            if (stage & GPU_STAGE_FRAGMENT) {
                ps_srvs[tex_slot] = tex->srv;
                ps_samplers[sampler_slot] = tex->sampler;
            }
        }

        if (max_tex_slot > 0) {
            ID3D11DeviceContext_VSSetShaderResources(d3d11.context, 0, max_tex_slot, vs_srvs);
            ID3D11DeviceContext_PSSetShaderResources(d3d11.context, 0, max_tex_slot, ps_srvs);
        }
        if (max_sampler_slot > 0) {
            ID3D11DeviceContext_VSSetSamplers(d3d11.context, 0, max_sampler_slot, vs_samplers);
            ID3D11DeviceContext_PSSetSamplers(d3d11.context, 0, max_sampler_slot, ps_samplers);
        }
    }
}

void gpu_backend_draw(u32 vertex_count, u32 instance_count) {
    ID3D11DeviceContext_DrawInstanced(d3d11.context, vertex_count, instance_count, 0, 0);
}

void gpu_backend_draw_indexed(u32 index_count, u32 instance_count) {
    ID3D11DeviceContext_DrawIndexedInstanced(d3d11.context, index_count, instance_count, 0, 0, 0);
}

void gpu_backend_load_texture(u32 idx, const char *path) {
    ThreadContext *tctx = tctx_current();
    Allocator temp_alloc = make_arena_allocator(&tctx->temp_arena);

    PlatformFileData file = os_read_file(path, &temp_alloc);
    if (!file.success) {
        LOG_ERROR("Failed to read texture file: %", FMT_STR(path));
        return;
    }

    int width, height, channels;
    u8 *data = stbi_load_from_memory(file.buffer, (int)file.buffer_len, &width, &height, &channels, 4);
    if (!data) {
        LOG_ERROR("Failed to decode texture: %", FMT_STR(path));
        return;
    }

    gpu_backend_make_texture_data(idx, (u32)width, (u32)height, data);
    stbi_image_free(data);
}

void gpu_backend_make_texture_data(u32 idx, u32 width, u32 height, u8 *data) {
    D3D11Texture *tex = &d3d11.textures[idx];

    D3D11_TEXTURE2D_DESC tex_desc = {
        .Width = width,
        .Height = height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
    };

    D3D11_SUBRESOURCE_DATA init_data = {
        .pSysMem = data,
        .SysMemPitch = width * 4,
    };

    HRESULT hr = ID3D11Device_CreateTexture2D(d3d11.device, &tex_desc, &init_data, &tex->texture);
    if (FAILED(hr)) {
        LOG_ERROR("CreateTexture2D failed: %", FMT_UINT(hr));
        return;
    }

    hr = ID3D11Device_CreateShaderResourceView(d3d11.device, (ID3D11Resource *)tex->texture, NULL, &tex->srv);
    if (FAILED(hr)) {
        LOG_ERROR("CreateShaderResourceView failed: %", FMT_UINT(hr));
        return;
    }

    D3D11_SAMPLER_DESC sampler_desc = {
        .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
        .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
        .MaxLOD = D3D11_FLOAT32_MAX,
    };
    hr = ID3D11Device_CreateSamplerState(d3d11.device, &sampler_desc, &tex->sampler);
    if (FAILED(hr)) {
        LOG_ERROR("CreateSamplerState failed: %", FMT_UINT(hr));
        return;
    }

    tex->ready = true;
}

u32 gpu_backend_texture_is_ready(u32 idx) {
    return d3d11.textures[idx].ready;
}

void gpu_backend_destroy_texture(u32 idx) {
    UNUSED(idx);
}

internal DXGI_FORMAT d3d11_texture_format(GpuTextureFormat format) {
    switch (format) {
        case GPU_TEXTURE_FORMAT_RGBA8:   return DXGI_FORMAT_R8G8B8A8_UNORM;
        case GPU_TEXTURE_FORMAT_RGBA16F: return DXGI_FORMAT_R16G16B16A16_FLOAT;
        default: return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}

void gpu_backend_make_render_target(u32 idx, u32 width, u32 height, u32 format) {
    D3D11RenderTarget *rt = &d3d11.render_targets[idx];
    rt->width = width;
    rt->height = height;
    rt->format = (GpuTextureFormat)format;

    DXGI_FORMAT dxgi_format = d3d11_texture_format((GpuTextureFormat)format);

    D3D11_TEXTURE2D_DESC tex_desc = {
        .Width = width,
        .Height = height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = dxgi_format,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
    };
    ID3D11Device_CreateTexture2D(d3d11.device, &tex_desc, NULL, &rt->texture);
    ID3D11Device_CreateRenderTargetView(d3d11.device, (ID3D11Resource *)rt->texture, NULL, &rt->rtv);
    ID3D11Device_CreateShaderResourceView(d3d11.device, (ID3D11Resource *)rt->texture, NULL, &rt->srv);

    D3D11_TEXTURE2D_DESC depth_desc = {
        .Width = width,
        .Height = height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_DEPTH_STENCIL,
    };
    ID3D11Device_CreateTexture2D(d3d11.device, &depth_desc, NULL, &rt->depth_texture);
    ID3D11Device_CreateDepthStencilView(d3d11.device, (ID3D11Resource *)rt->depth_texture, NULL, &rt->dsv);
}

void gpu_backend_resize_render_target(u32 idx, u32 width, u32 height) {
    D3D11RenderTarget *rt = &d3d11.render_targets[idx];

    if (rt->rtv) ID3D11RenderTargetView_Release(rt->rtv);
    if (rt->srv) ID3D11ShaderResourceView_Release(rt->srv);
    if (rt->texture) ID3D11Texture2D_Release(rt->texture);
    if (rt->dsv) ID3D11DepthStencilView_Release(rt->dsv);
    if (rt->depth_texture) ID3D11Texture2D_Release(rt->depth_texture);

    gpu_backend_make_render_target(idx, width, height, rt->format);
}

void gpu_backend_destroy_render_target(u32 idx) {
    D3D11RenderTarget *rt = &d3d11.render_targets[idx];
    if (rt->rtv) ID3D11RenderTargetView_Release(rt->rtv);
    if (rt->srv) ID3D11ShaderResourceView_Release(rt->srv);
    if (rt->texture) ID3D11Texture2D_Release(rt->texture);
    if (rt->dsv) ID3D11DepthStencilView_Release(rt->dsv);
    if (rt->depth_texture) ID3D11Texture2D_Release(rt->depth_texture);
    memset(rt, 0, sizeof(*rt));
}

internal void d3d11_ensure_blit_resources(void) {
    if (d3d11.blit_vs) return;

    ID3D11Device_CreateVertexShader(d3d11.device, blit_vs_d3d11, sizeof(blit_vs_d3d11), NULL, &d3d11.blit_vs);
    ID3D11Device_CreatePixelShader(d3d11.device, blit_fs_d3d11, sizeof(blit_fs_d3d11), NULL, &d3d11.blit_ps);

    D3D11_SAMPLER_DESC sampler_desc = {
        .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
        .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
        .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
        .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
        .MaxLOD = D3D11_FLOAT32_MAX,
    };
    ID3D11Device_CreateSamplerState(d3d11.device, &sampler_desc, &d3d11.blit_sampler);

    D3D11_RASTERIZER_DESC raster_desc = {
        .FillMode = D3D11_FILL_SOLID,
        .CullMode = D3D11_CULL_NONE,
    };
    ID3D11Device_CreateRasterizerState(d3d11.device, &raster_desc, &d3d11.blit_rasterizer);

    D3D11_DEPTH_STENCIL_DESC ds_desc = {
        .DepthEnable = FALSE,
        .StencilEnable = FALSE,
    };
    ID3D11Device_CreateDepthStencilState(d3d11.device, &ds_desc, &d3d11.blit_depth_stencil);

    D3D11_BLEND_DESC blend_desc = {
        .RenderTarget[0] = {
            .BlendEnable = FALSE,
            .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
        },
    };
    ID3D11Device_CreateBlendState(d3d11.device, &blend_desc, &d3d11.blit_blend);
}

void gpu_backend_blit_to_screen(u32 rt_idx) {
    D3D11RenderTarget *rt = &d3d11.render_targets[rt_idx];

    d3d11_ensure_blit_resources();

    // Set backbuffer as render target
    ID3D11DeviceContext_OMSetRenderTargets(d3d11.context, 1, &d3d11.backbuffer_rtv, NULL);

    D3D11_VIEWPORT viewport = {
        .TopLeftX = 0,
        .TopLeftY = 0,
        .Width = (f32)d3d11.width,
        .Height = (f32)d3d11.height,
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f,
    };
    ID3D11DeviceContext_RSSetViewports(d3d11.context, 1, &viewport);

    // Set pipeline state
    ID3D11DeviceContext_VSSetShader(d3d11.context, d3d11.blit_vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(d3d11.context, d3d11.blit_ps, NULL, 0);
    ID3D11DeviceContext_RSSetState(d3d11.context, d3d11.blit_rasterizer);
    ID3D11DeviceContext_OMSetDepthStencilState(d3d11.context, d3d11.blit_depth_stencil, 0);
    ID3D11DeviceContext_OMSetBlendState(d3d11.context, d3d11.blit_blend, NULL, 0xFFFFFFFF);

    // Bind HDR texture and sampler
    ID3D11DeviceContext_PSSetShaderResources(d3d11.context, 0, 1, &rt->srv);
    ID3D11DeviceContext_PSSetSamplers(d3d11.context, 0, 1, &d3d11.blit_sampler);

    // Draw fullscreen triangle (no vertex buffer needed)
    ID3D11DeviceContext_IASetPrimitiveTopology(d3d11.context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_IASetInputLayout(d3d11.context, NULL);
    ID3D11DeviceContext_Draw(d3d11.context, 3, 0);

    // Unbind SRV to avoid hazards
    ID3D11ShaderResourceView *null_srv = NULL;
    ID3D11DeviceContext_PSSetShaderResources(d3d11.context, 0, 1, &null_srv);
}
