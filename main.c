#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <SDL3/SDL.h>

#include <cimgui.h>
#include <cimgui_impl.h>

#include <spirv_cross_c.h>

#define SDL_PRINT_ERROR_AND_EXIT(_err)                \
    do {                                              \
        fprintf(stderr, _err ": %s", SDL_GetError()); \
        exit(-1);                                     \
    } while (false)

#define CHECK_RET(x, _COND, _ERR, ...) \
    do {                    \
        if ((x) != _COND) {\
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
    CHECK_RET(spvc_resources_get_resource_list_for_type(resources, type, &list, &count), SPVC_SUCCESS,
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
    CHECK_RET(spvc_context_parse_spirv(context, file, size / sizeof(SpvId), &ir), SPVC_SUCCESS,
              "Failed to parse spirv ir");
    CHECK_RET(spvc_context_create_compiler(context, SPVC_BACKEND_NONE, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler),
              SPVC_SUCCESS, "Failed to create SPVC_BACKEND_NONE compiler");
    CHECK_RET(spvc_compiler_create_compiler_options(compiler, &options), SPVC_SUCCESS,
              "Failed to create compiler options");
    CHECK_RET(spvc_compiler_install_compiler_options(compiler, options), SPVC_SUCCESS,
              "Failed to install compiler options");
    CHECK_RET(spvc_compiler_create_shader_resources(compiler, &resources), SPVC_SUCCESS,
              "Failed to create shader resources");

    size_t num_uniform_buffers;
    const spvc_reflected_resource *list = NULL;
    CHECK_RET(spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &list,
                                                        &num_uniform_buffers), SPVC_SUCCESS,
              "Failed to get resource list for type: %d", SPVC_RESOURCE_TYPE_UNIFORM_BUFFER);
    size_t num_storage_buffers;
    CHECK_RET(spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STORAGE_BUFFER, &list,
                                                        &num_storage_buffers), SPVC_SUCCESS,
              "Failed to get resource list for type: %d", SPVC_RESOURCE_TYPE_STORAGE_BUFFER);
    size_t num_samplers;
    CHECK_RET(spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS, &list,
                                                        &num_samplers), SPVC_SUCCESS,
              "Failed to get resource list for type: %d", SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS);
    size_t num_storage_textures;
    CHECK_RET(spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STORAGE_IMAGE, &list,
                                                        &num_storage_textures), SPVC_SUCCESS,
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

int main(void) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_PRINT_ERROR_AND_EXIT("SDL_Init");
    }

    spvc_context context;
    CHECK_RET(spvc_context_create(&context), SPVC_SUCCESS, "Failed to create spvc_context");
    spvc_context_set_error_callback(context, error_callback, NULL);

    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window *window = SDL_CreateWindow("Kingdom Defense", (int) (800 * main_scale), (int) (600 * main_scale),
                                          window_flags);
    if (window == NULL) {
        SDL_PRINT_ERROR_AND_EXIT("Failed to create window");
    }

    SDL_GPUDevice *gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, NULL);
    if (!gpu) {
        SDL_PRINT_ERROR_AND_EXIT("Create gpu failed");
    }

    shader_t *simple_frag = load_shader(gpu, "simple.frag.hlsl.spirv", SDL_GPU_SHADERSTAGE_FRAGMENT, context);
    if (simple_frag == NULL) {
        SDL_PRINT_ERROR_AND_EXIT("failed to load simple_frag shader");
    }

    shader_t *simple_vert = load_shader(gpu, "simple.vert.hlsl.spirv", SDL_GPU_SHADERSTAGE_VERTEX, context);
    if (simple_vert == NULL) {
        SDL_PRINT_ERROR_AND_EXIT("failed to load simple_vert shader");
    }

    if (!SDL_ClaimWindowForGPUDevice(gpu, window)) {
        SDL_PRINT_ERROR_AND_EXIT("Failed to claim window for gpu device");
    }

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
            .Device = gpu,
            .MSAASamples = 4,
            .ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpu, window),
    };
    ImGui_ImplSDLGPU3_Init(&imgui_init_info);

    SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
            .vertex_shader = simple_vert->shader,
            .fragment_shader = simple_frag->shader,
            .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
            .target_info = {
                    .num_color_targets = 1,
                    .color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
                        .format = SDL_GetGPUSwapchainTextureFormat(gpu, window),
                    }},
            },
            .rasterizer_state = {
                    .fill_mode = SDL_GPU_FILLMODE_FILL,
            },
            .vertex_input_state = (SDL_GPUVertexInputState) {
                .num_vertex_buffers = 1,
                .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {{
                        .slot = 0,
                        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                        .instance_step_rate = 0,
                        .pitch = sizeof(vertex_pos_color_t),
                }},
                .num_vertex_attributes = 2,
                .vertex_attributes = (SDL_GPUVertexAttribute[]) {{
                    .buffer_slot = 0,
                    .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                    .location = 0,
                    .offset = 0,
                }, {
                        .buffer_slot = 0,
                        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                        .location = 1,
                        .offset = offsetof(vertex_pos_color_t, color),
                }},
            },
    };
    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(gpu, &pipelineCreateInfo);

    SDL_GPUBufferCreateInfo buffer_info = {
            .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
            .size = sizeof(vertex_pos_color_t) * 3,
    };
    SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(gpu, &buffer_info);

    SDL_GPUTransferBufferCreateInfo transfer_buffer_info = {
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = buffer_info.size,
    };
    SDL_GPUTransferBuffer *transfer_buffer = SDL_CreateGPUTransferBuffer(gpu, &transfer_buffer_info);
    vertex_pos_color_t* mem = SDL_MapGPUTransferBuffer(gpu, transfer_buffer, false);
    mem[0] = (vertex_pos_color_t){ -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f };
    mem[1] = (vertex_pos_color_t){ 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f };
    mem[2] = (vertex_pos_color_t){ 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f };
    SDL_UnmapGPUTransferBuffer(gpu, transfer_buffer);

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

        SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(gpu);

        SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);
        SDL_UploadToGPUBuffer(copy_pass,
                              &(SDL_GPUTransferBufferLocation) {
                                      .transfer_buffer = transfer_buffer,
                                },
                              &(SDL_GPUBufferRegion) {
                                      .buffer = buffer,
                                      .size = buffer_info.size,
                                },
                              false
        );
        SDL_EndGPUCopyPass(copy_pass);

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

        SDL_BindGPUGraphicsPipeline(render_pass, pipeline);
        SDL_BindGPUVertexBuffers(render_pass, 0, &(SDL_GPUBufferBinding){
                .buffer = buffer,
            }, 1);
        SDL_DrawGPUPrimitives(render_pass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(render_pass);

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
        if (!SDL_WaitForGPUFences(gpu, true, &fence, 1)) {
            SDL_PRINT_ERROR_AND_EXIT("Wait for Gpu Fences failed");
        }
        SDL_ReleaseGPUFence(gpu, fence);
    }

    SDL_WaitForGPUIdle(gpu);
    ImGui_ImplSDL3_Shutdown();
    ImGui_ImplSDLGPU3_Shutdown();
    igDestroyContext(NULL);

    SDL_free(simple_frag);

    SDL_ReleaseWindowFromGPUDevice(gpu, window);
    SDL_DestroyGPUDevice(gpu);
    SDL_DestroyWindow(window);
    SDL_Quit();
    spvc_context_destroy(context);
    return 0;
}
