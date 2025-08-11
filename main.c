#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <SDL3/SDL.h>

#include <cimgui.h>
#include <cimgui_impl.h>

#include <spirv_cross_c.h>

#include "shader_inputs.h"

#define ARRAY_SIZE(_array) (sizeof(_array) / sizeof(_array[0]))

#define SDL_PRINT_ERROR_AND_EXIT(_err)                \
    do {                                              \
        fprintf(stderr, _err ": %s", SDL_GetError()); \
        exit(-1);                                     \
    } while (false)

#define CHECK_RET(_bool, _ERR, ...) \
    do {                    \
        if (!(_bool)) {\
            fprintf(stderr, "failed: "_ERR" at %s:%d", ##__VA_ARGS__, __FILE__, __LINE__); \
            exit(1);\
        }\
    } while (false)

typedef struct {
    SDL_GPUShaderCreateInfo info;
    SDL_GPUShader *shader;
} shader_t;

static void error_callback(void *userdata, const char *error) {
    (void) userdata;
    fprintf(stderr, "Error: %s\n", error);
    exit(1);
}

static void
dump_resource_list(spvc_compiler compiler, spvc_resources resources, spvc_resource_type type, const char *tag) {
    const spvc_reflected_resource *list = NULL;
    size_t count = 0;
    size_t i;
    CHECK_RET(spvc_resources_get_resource_list_for_type(resources, type, &list, &count) == SPVC_SUCCESS,
              "Failed to get resource list for type: %d", type);
    printf("%s\n", tag);
    for (i = 0; i < count; i++) {
        printf("ID: %u, BaseTypeID: %u, TypeID: %u, Name: %s\n", list[i].id, list[i].base_type_id, list[i].type_id,
               list[i].name);
        printf("  Set: %u, Binding: %u\n",
               spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationDescriptorSet),
               spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationBinding));
    }
}

static void dump_resources(spvc_compiler compiler, spvc_resources resources) {
    dump_resource_list(compiler, resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, "UBO");
    dump_resource_list(compiler, resources, SPVC_RESOURCE_TYPE_STORAGE_BUFFER, "SSBO");
    dump_resource_list(compiler, resources, SPVC_RESOURCE_TYPE_PUSH_CONSTANT, "Push");
    dump_resource_list(compiler, resources, SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS, "Samplers");
    dump_resource_list(compiler, resources, SPVC_RESOURCE_TYPE_SEPARATE_IMAGE, "Image");
    dump_resource_list(compiler, resources, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, "Combined image samplers");
    dump_resource_list(compiler, resources, SPVC_RESOURCE_TYPE_STAGE_INPUT, "Stage input");
    dump_resource_list(compiler, resources, SPVC_RESOURCE_TYPE_STAGE_OUTPUT, "Stage output");
    dump_resource_list(compiler, resources, SPVC_RESOURCE_TYPE_STORAGE_IMAGE, "Storage image");
    dump_resource_list(compiler, resources, SPVC_RESOURCE_TYPE_SUBPASS_INPUT, "Subpass input");
}

typedef struct {
    float x;
    float y;
    float color[4];
} vertex_pos_color_t;

shader_t *load_shader(SDL_GPUDevice *device, const char *path, SDL_GPUShaderStage stage, spvc_context context) {
    size_t size = 0;
    void *file = SDL_LoadFile(path, &size);
    if (size == 0) {
        SDL_free(file);
        return NULL;
    }

    spvc_parsed_ir ir;
    spvc_compiler compiler;
    spvc_compiler_options options;
    spvc_resources resources;
    CHECK_RET(spvc_context_parse_spirv(context, file, size / sizeof(SpvId), &ir) == SPVC_SUCCESS,
              "Failed to parse spirv ir");
    CHECK_RET(
            spvc_context_create_compiler(context, SPVC_BACKEND_NONE, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler) ==
            SPVC_SUCCESS, "Failed to create SPVC_BACKEND_NONE compiler");
    CHECK_RET(spvc_compiler_create_compiler_options(compiler, &options) == SPVC_SUCCESS,
              "Failed to create compiler options");
    CHECK_RET(spvc_compiler_install_compiler_options(compiler, options) == SPVC_SUCCESS,
              "Failed to install compiler options");
    CHECK_RET(spvc_compiler_create_shader_resources(compiler, &resources) == SPVC_SUCCESS,
              "Failed to create shader resources");

    size_t num_uniform_buffers;
    const spvc_reflected_resource *list = NULL;
    CHECK_RET(spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &list,
                                                        &num_uniform_buffers) == SPVC_SUCCESS,
              "Failed to get resource list for type: %d", SPVC_RESOURCE_TYPE_UNIFORM_BUFFER);
    size_t num_storage_buffers;
    CHECK_RET(spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STORAGE_BUFFER, &list,
                                                        &num_storage_buffers) == SPVC_SUCCESS,
              "Failed to get resource list for type: %d", SPVC_RESOURCE_TYPE_STORAGE_BUFFER);
    size_t num_samplers;
    CHECK_RET(spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS, &list,
                                                        &num_samplers) == SPVC_SUCCESS,
              "Failed to get resource list for type: %d", SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS);
    size_t num_storage_textures;
    CHECK_RET(spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STORAGE_IMAGE, &list,
                                                        &num_storage_textures) == SPVC_SUCCESS,
              "Failed to get resource list for type: %d", SPVC_RESOURCE_TYPE_STORAGE_IMAGE);

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_GPU_SHADER_CREATE_NAME_STRING, path);
    SDL_GPUShaderCreateInfo info = {
            .code = file,
            .code_size = size,
            .entrypoint = "main",
            .format = SDL_GPU_SHADERFORMAT_SPIRV,
            .stage = stage,
            .props = props,
            .num_samplers = num_samplers,
            .num_storage_buffers = num_storage_buffers,
            .num_uniform_buffers = num_uniform_buffers,
            .num_storage_textures = num_storage_textures,
    };
    SDL_GPUShader *shader = SDL_CreateGPUShader(device, &info);
    SDL_free(file);
    if (shader == NULL) {
        return NULL;
    }
    shader_t *ret = SDL_malloc(sizeof(shader_t));
    ret->shader = shader;
    ret->info = info;
    return ret;
}

void free_shader(SDL_GPUDevice *device, shader_t *shader) {
    assert(device && "device is null");
    assert(shader && "shader is null");
    SDL_ReleaseGPUShader(device, shader->shader);
    SDL_free(shader);
}

typedef void (*convert_format_fun_t)(void *input, size_t input_size, void *out, size_t *out_size);

typedef struct {
    SDL_PixelFormat surface;
    SDL_GPUTextureFormat gpu;
    convert_format_fun_t convert;
    SDL_GPUTextureFormat convert_format;
    uint8_t bits_per_pixel;
} sdl_surface_format_to_gpu_format;

void convert_bgr24_to_rgba8(void *input, size_t input_size, void *out, size_t *out_size) {
    uint8_t *input_as_bytes = (uint8_t *) input;
    uint8_t *out_as_bytes = (uint8_t *) out;
    size_t pixels = input_size;
    for (size_t i = 0; i < pixels; i++) {
        size_t input_px = i * 3;
        size_t out_px = i * 4;

        uint8_t b = input_as_bytes[input_px + 0];
        uint8_t g = input_as_bytes[input_px + 1];
        uint8_t r = input_as_bytes[input_px + 2];

        out_as_bytes[out_px + 0] = r;
        out_as_bytes[out_px + 1] = g;
        out_as_bytes[out_px + 2] = b;
        out_as_bytes[out_px + 3] = UINT8_MAX;

        *out_size += 4;
    }
}

sdl_surface_format_to_gpu_format pixel_to_gpu[] = {
        {SDL_PIXELFORMAT_BGR24, SDL_GPU_TEXTUREFORMAT_INVALID, convert_bgr24_to_rgba8,
         SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 32},
};

#define PIXEL_TO_GPU_SIZE (sizeof(pixel_to_gpu) / sizeof(sdl_surface_format_to_gpu_format))

sdl_surface_format_to_gpu_format *gpu_format_for_surface(SDL_PixelFormat pixel_format) {
    for (size_t i = 0; i < PIXEL_TO_GPU_SIZE; i++) {
        if (pixel_to_gpu[i].surface == pixel_format) {
            return &pixel_to_gpu[i];
        }
    }
    return NULL;
}

SDL_GPUTexture *upload_texture_to_gpu(SDL_GPUDevice *device, const char *asset_name) {
    char buffer[1024] = {0};
    snprintf(buffer, sizeof(buffer), "assets/%s", asset_name);
    SDL_Surface *surface = SDL_LoadBMP(buffer);
    if (surface == NULL) {
        SDL_PRINT_ERROR_AND_EXIT("failed to load texture");
    }

    sdl_surface_format_to_gpu_format *converter = gpu_format_for_surface(surface->format);
    CHECK_RET(converter != NULL, "Cant load texture with with SDL_PixelFormat: %d", surface->format);

    uint32_t w = surface->w;
    uint32_t h = surface->h;
    bool free_pixels = false;
    uint8_t *pixels = NULL;
    SDL_GPUTextureFormat format;
    if (converter->gpu == SDL_GPU_TEXTUREFORMAT_INVALID) {
        pixels = SDL_malloc(1024 * 1024 * 1024);
        if (pixels == NULL) {
            SDL_DestroySurface(surface);
            return NULL;
        }
        size_t out_size = 0;
        converter->convert(surface->pixels, w * h, pixels, &out_size);
        format = converter->convert_format;
        free_pixels = true;
    } else {
        pixels = surface->pixels;
        format = converter->gpu;
    }

    SDL_GPUTextureCreateInfo texture_info = {
            .width = surface->w,
            .height = surface->h,
            .format = format,
            .layer_count_or_depth = 1,
            .num_levels = 1,
            .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
    };
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &texture_info);
    if (texture == NULL) {
        goto error;
    }

    size_t size_bytes = w * h * (converter->bits_per_pixel / 8);
    SDL_GPUTransferBufferCreateInfo texture_transfer_buffer_info = {
            .size = size_bytes,
    };
    SDL_GPUTransferBuffer *texture_transfer_buffer = SDL_CreateGPUTransferBuffer(device, &texture_transfer_buffer_info);
    if (texture_transfer_buffer == NULL) {
        goto error;
    }

    void *mem = SDL_MapGPUTransferBuffer(device, texture_transfer_buffer, false);
    memcpy(mem, pixels, size_bytes);
    SDL_UnmapGPUTransferBuffer(device, texture_transfer_buffer);

    SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(device);
    if (command_buffer == NULL) {
        goto error;
    }

    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);

    SDL_UploadToGPUTexture(copy_pass,
                           &(SDL_GPUTextureTransferInfo) {
                                   .transfer_buffer = texture_transfer_buffer,
                                   .pixels_per_row = w,
                                   .rows_per_layer = h,
                           }, &(SDL_GPUTextureRegion) {
                    .texture = texture,
                    .w = w,
                    .h = h,
            }, false);

    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(command_buffer);

    SDL_ReleaseGPUTransferBuffer(device, texture_transfer_buffer);
    SDL_DestroySurface(surface);
    if (free_pixels) {
        SDL_free(pixels);
    }
    return texture;

    error:
    if (free_pixels) {
        SDL_free(pixels);
    }
    if (texture) {
        SDL_ReleaseGPUTexture(device, texture);
    }
    SDL_DestroySurface(surface);
    return NULL;
}

void imgui_init(SDL_GPUDevice *device, SDL_Window *window, float main_scale) {
    igCreateContext(NULL);
    ImGuiIO *io = igGetIO_Nil();
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    igStyleColorsDark(NULL);

    ImGuiStyle *style = igGetStyle();
    ImGuiStyle_ScaleAllSizes(style, main_scale);
    style->FontScaleDpi = main_scale;

    ImGui_ImplSDL3_InitForSDLGPU(window);
    ImGui_ImplSDLGPU3_InitInfo imgui_init_info = {
            .Device = device,
            .MSAASamples = 4,
            .ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device, window),
    };
    ImGui_ImplSDLGPU3_Init(&imgui_init_info);
}

SDL_GPUGraphicsPipeline *
load_simple_triangle_pipeline(SDL_GPUDevice *device, SDL_GPUTextureFormat color_target_format, spvc_context context) {
    shader_t *simple_vert = load_shader(device, "simple.vert.hlsl.spirv", SDL_GPU_SHADERSTAGE_VERTEX, context);
    if (simple_vert == NULL) {
        SDL_PRINT_ERROR_AND_EXIT("failed to load simple_vert shader");
    }

    shader_t *simple_frag = load_shader(device, "simple.frag.hlsl.spirv", SDL_GPU_SHADERSTAGE_FRAGMENT, context);
    if (simple_frag == NULL) {
        SDL_PRINT_ERROR_AND_EXIT("failed to load simple_frag shader");
    }

    SDL_GPUColorTargetDescription color_target_description = {
            .format = color_target_format,
    };
    SDL_GPUVertexBufferDescription vertex_buffer_description = {
            .slot = 0,
            .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
            .instance_step_rate = 0,
            .pitch = sizeof(vertex_pos_color_t),
    };
    SDL_GPUVertexAttribute vertex_attributes[] = {
            {
                    .buffer_slot = 0,
                    .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                    .location = 0,
                    .offset = 0,
            },
            {
                    .buffer_slot = 0,
                    .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                    .location = 1,
                    .offset = offsetof(vertex_pos_color_t, color),
            },
    };
    SDL_GPUGraphicsPipelineCreateInfo pipeline_create_info = {
            .vertex_shader = simple_vert->shader,
            .fragment_shader = simple_frag->shader,
            .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
            .target_info = {
                    .num_color_targets = 1,
                    .color_target_descriptions = &color_target_description,
            },
            .rasterizer_state = {
                    .fill_mode = SDL_GPU_FILLMODE_FILL,
            },
            .vertex_input_state = (SDL_GPUVertexInputState) {
                    .num_vertex_buffers = 1,
                    .vertex_buffer_descriptions = &vertex_buffer_description,
                    .num_vertex_attributes = ARRAY_SIZE(vertex_attributes),
                    .vertex_attributes = vertex_attributes,
            },
    };
    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_create_info);
    free_shader(device, simple_frag);
    free_shader(device, simple_vert);
    return pipeline;
}

SDL_GPUGraphicsPipeline *
load_quad_pipeline(SDL_GPUDevice *device, SDL_GPUTextureFormat color_target_format, spvc_context context) {
    shader_t *quad_vertex_shader = load_shader(device, "quad.vert.hlsl.spirv", SDL_GPU_SHADERSTAGE_VERTEX, context);
    if (quad_vertex_shader == NULL) {
        SDL_PRINT_ERROR_AND_EXIT("failed to load quad vertex shader");
    }

    shader_t *quad_frag_shader = load_shader(device, "quad.frag.hlsl.spirv", SDL_GPU_SHADERSTAGE_FRAGMENT, context);
    if (quad_frag_shader == NULL) {
        SDL_PRINT_ERROR_AND_EXIT("failed to load quad frag shader");
    }

    SDL_GPUColorTargetDescription color_target_description = {
            .format = color_target_format,
    };
    SDL_GPUVertexBufferDescription vertex_buffer_description = {
            .slot = 0,
            .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
            .instance_step_rate = 0,
            .pitch = sizeof(quad_vert),
    };
    SDL_GPUVertexAttribute vertex_attributes[] = {
            { // xy_pos
                    .buffer_slot = 0,
                    .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                    .location = 0,
                    .offset = offsetof(quad_vert, xy_pos),
            },
            { // uv
                    .buffer_slot = 0,
                    .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                    .location = 1,
                    .offset = offsetof(quad_vert, uv),
            },
            { // color
                    .buffer_slot = 0,
                    .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                    .location = 2,
                    .offset = offsetof(quad_vert, color),
            },
    };
    SDL_GPUGraphicsPipelineCreateInfo pipeline_create_info = {
            .vertex_shader = quad_vertex_shader->shader,
            .fragment_shader = quad_frag_shader->shader,
            .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
            .target_info = {
                    .num_color_targets = 1,
                    .color_target_descriptions = &color_target_description,
            },
            .rasterizer_state = {
                    .fill_mode = SDL_GPU_FILLMODE_FILL,
            },
            .vertex_input_state = (SDL_GPUVertexInputState) {
                    .num_vertex_buffers = 1,
                    .vertex_buffer_descriptions = &vertex_buffer_description,
                    .num_vertex_attributes = ARRAY_SIZE(vertex_attributes),
                    .vertex_attributes = vertex_attributes,
            },
    };
    SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_create_info);
    free_shader(device, quad_frag_shader);
    free_shader(device, quad_vertex_shader);
    return pipeline;
}

void upload_data_to_gpu(SDL_GPUDevice *device, SDL_GPUBuffer *buffer, const void* data, size_t size) {
    // TODO: make uploads batched
    SDL_GPUTransferBufferCreateInfo transfer_buffer_info = {
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = size,
    };
    SDL_GPUTransferBuffer *transfer_buffer = SDL_CreateGPUTransferBuffer(device, &transfer_buffer_info);
    void *mem = SDL_MapGPUTransferBuffer(device, transfer_buffer, false);
    memcpy(mem, data, size);
    SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

    SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(device);

    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    SDL_GPUTransferBufferLocation source = {
        .transfer_buffer = transfer_buffer,
    };
    SDL_GPUBufferRegion destination = {
        .buffer = buffer,
        .size = size,
    };
    SDL_UploadToGPUBuffer(copy_pass, &source, &destination, false);
    SDL_EndGPUCopyPass(copy_pass);

    SDL_SubmitGPUCommandBuffer(command_buffer);
    SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
}

int main(void) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_PRINT_ERROR_AND_EXIT("SDL_Init");
    }

    spvc_context spvc_context;
    CHECK_RET(spvc_context_create(&spvc_context) == SPVC_SUCCESS, "Failed to create spvc_context");
    spvc_context_set_error_callback(spvc_context, error_callback, NULL);

    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window *window = SDL_CreateWindow("Kingdom Defense", (int) (800 * main_scale), (int) (600 * main_scale),
                                          window_flags);
    if (window == NULL) {
        SDL_PRINT_ERROR_AND_EXIT("Failed to create window");
    }

    SDL_GPUDevice *device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, NULL);
    if (!device) {
        SDL_PRINT_ERROR_AND_EXIT("Create device failed");
    }

    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_PRINT_ERROR_AND_EXIT("Failed to claim window for device device");
    }

    SDL_GPUTextureFormat swapchain_texture_format = SDL_GetGPUSwapchainTextureFormat(device, window);

    SDL_GPUTexture *grass = upload_texture_to_gpu(device, "grass.bmp");
    SDL_GPUSamplerCreateInfo grass_sampler_info = {
            .compare_op = SDL_GPU_COMPAREOP_ALWAYS,
    };
    SDL_GPUSampler *grass_sampler = SDL_CreateGPUSampler(device, &grass_sampler_info);
    if (!grass_sampler) {
        SDL_PRINT_ERROR_AND_EXIT("failed to load grass_sampler");
    }

    imgui_init(device, window, main_scale);

    SDL_GPUGraphicsPipeline *simple_triangle_pipeline = load_simple_triangle_pipeline(device, swapchain_texture_format, spvc_context);
    if (!simple_triangle_pipeline) {
        SDL_PRINT_ERROR_AND_EXIT("failed to load simple_triangle_pipeline");
    }

    SDL_GPUGraphicsPipeline *quad_pipeline = load_quad_pipeline(device, swapchain_texture_format, spvc_context);
    if (!quad_pipeline) {
        SDL_PRINT_ERROR_AND_EXIT("failed to load quad pipeline");
    }

    SDL_GPUBufferCreateInfo buffer_info = {
            .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
            .size = sizeof(vertex_pos_color_t) * 3,
    };
    SDL_GPUBuffer *triangle_vertex = SDL_CreateGPUBuffer(device, &buffer_info);

    vertex_pos_color_t rainbow_triangle_vertices[3] = {
            {-1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
            {1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f},
            {0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f},
    };
    upload_data_to_gpu(device, triangle_vertex, rainbow_triangle_vertices, sizeof(rainbow_triangle_vertices));

    quad_vert quad_vertices[] = {
            // top
            { .xy_pos = {-0.5f, 0.5f}, .uv = {0.0f, 0.0f}, .color = {1.0f, 0.0f, 0.0f, 1.0f}},
            { .xy_pos = {0.5f, 0.5f}, .uv = {1.0f, 0.0f}, .color = {0.0f, 0.0f, 1.0f, 1.0f}},
            { .xy_pos = {-0.5f, -0.5f}, .uv = {0.0f, 1.0f}, .color = {0.0f, 0.0f, 1.0f, 1.0f}},
            { .xy_pos = {0.5f, -0.5f}, .uv = {1.0f, 1.0f}, .color = {0.0f, 1.0f, 0.0f, 1.0f}},
    };
    SDL_GPUBufferCreateInfo quad_buffer_info = {
            .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
            .size = sizeof(quad_vertices),
    };
    SDL_GPUBuffer *quad_buffer = SDL_CreateGPUBuffer(device, &quad_buffer_info);
    upload_data_to_gpu(device, quad_buffer, quad_vertices, sizeof(quad_vertices));

    uint16_t quad_index[] = {
            0, 3, 1,
            0, 2, 3,
    };
    SDL_GPUBufferCreateInfo quad_index_buffer_info = {
            .usage = SDL_GPU_BUFFERUSAGE_INDEX,
            .size = sizeof(quad_index),
    };
    SDL_GPUBuffer *quad_index_buffer = SDL_CreateGPUBuffer(device, &quad_index_buffer_info);
    upload_data_to_gpu(device, quad_index_buffer, quad_index, sizeof(quad_index));

    bool quit = false;

    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL3_ProcessEvent(&e);

            if (e.type == SDL_EVENT_QUIT) {
                quit = true;
            }
            // process events
        }

        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            continue;
        }

        SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(device);

        SDL_GPUTexture *window_texture;
        uint32_t width = 0;
        uint32_t height = 0;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window, &window_texture, &width, &height)) {
            SDL_PRINT_ERROR_AND_EXIT("Failed to acquire GPU Swapchain texture");
        }

        SDL_GPUColorTargetInfo target_info = {
                .texture = window_texture,
                .clear_color = (SDL_FColor) {.r = 0.1f, .g = 0.1f, .b = 0.1f, .a = 1.0f},
                .mip_level = 0,
                .load_op = SDL_GPU_LOADOP_CLEAR,
                .store_op = SDL_GPU_STOREOP_STORE,
        };
        SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, NULL);

        SDL_BindGPUGraphicsPipeline(render_pass, simple_triangle_pipeline);
        SDL_BindGPUVertexBuffers(render_pass, 0, &(SDL_GPUBufferBinding) {
                .buffer = triangle_vertex,
        }, 1);
        SDL_DrawGPUPrimitives(render_pass, 3, 1, 0, 0);

        SDL_BindGPUGraphicsPipeline(render_pass, quad_pipeline);
        SDL_BindGPUVertexBuffers(render_pass, 0, &(SDL_GPUBufferBinding) { .buffer = quad_buffer }, 1);
        SDL_BindGPUIndexBuffer(render_pass, &(SDL_GPUBufferBinding) { .buffer = quad_index_buffer}, SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_BindGPUFragmentSamplers(render_pass, 0, &(SDL_GPUTextureSamplerBinding){ .texture = grass, .sampler = grass_sampler }, 1);
        SDL_DrawGPUIndexedPrimitives(render_pass, 6, 1, 0, 0, 0);

        SDL_EndGPURenderPass(render_pass);

        SDL_BlitGPUTexture(command_buffer, &(SDL_GPUBlitInfo){
            .source = (SDL_GPUBlitRegion) {
                    .texture = grass,
                    .w = 32,
                    .h = 32,
            },
            .destination = (SDL_GPUBlitRegion) {
                    .texture = window_texture,
                    .x = 100,
                    .y = 100,
                    .w = 32,
                    .h = 32,
            },
            .load_op = SDL_GPU_LOADOP_LOAD,
        });

        // draw imgui
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        igNewFrame();

        {
            bool open = true;
            if (open) {
                igShowDemoWindow(&open);
            }
        }

        {
            bool open = true;
            if (igBegin("Some imGui window", &open, 0)) {
                igText("something here");
            }
            igEnd();
        }

        igRender();
        ImDrawData *draw_data = igGetDrawData();

        target_info.load_op = SDL_GPU_LOADOP_LOAD;
        ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);
        SDL_GPURenderPass *imgui_render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, NULL);
        ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, imgui_render_pass, NULL);
        SDL_EndGPURenderPass(imgui_render_pass);

        SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer);
        if (!SDL_WaitForGPUFences(device, true, &fence, 1)) {
            SDL_PRINT_ERROR_AND_EXIT("Wait for Gpu Fences failed");
        }
        SDL_ReleaseGPUFence(device, fence);
    }

    SDL_WaitForGPUIdle(device);
    ImGui_ImplSDL3_Shutdown();
    ImGui_ImplSDLGPU3_Shutdown();
    igDestroyContext(NULL);

    SDL_ReleaseGPUGraphicsPipeline(device, simple_triangle_pipeline);

    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    spvc_context_destroy(spvc_context);
    return 0;
}
