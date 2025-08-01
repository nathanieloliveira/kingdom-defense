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

static void error_callback(void *userdata, const char *error)
{
    (void)userdata;
    fprintf(stderr, "Error: %s\n", error);
    exit(1);
}

static void dump_resource_list(spvc_compiler compiler, spvc_resources resources, spvc_resource_type type, const char *tag)
{
    const spvc_reflected_resource *list = NULL;
    size_t count = 0;
    size_t i;
    CHECK_RET(spvc_resources_get_resource_list_for_type(resources, type, &list, &count), SPVC_SUCCESS, "Failed to get resource list for type: %d", type);
    printf("%s\n", tag);
    for (i = 0; i < count; i++)
    {
        printf("ID: %u, BaseTypeID: %u, TypeID: %u, Name: %s\n", list[i].id, list[i].base_type_id, list[i].type_id,
               list[i].name);
        printf("  Set: %u, Binding: %u\n",
               spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationDescriptorSet),
               spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationBinding));
    }
}

static void dump_resources(spvc_compiler compiler, spvc_resources resources)
{
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


SDL_GPUShader* load_shader(SDL_GPUDevice *device, const char *path, SDL_GPUShaderStage stage) {
    size_t size = 0;
    void* file = SDL_LoadFile(path, &size);
    if (file == NULL) {
        return NULL;
    }

    spvc_context context;
    spvc_parsed_ir ir;
    spvc_compiler compiler;
    spvc_compiler_options options;
    spvc_resources resources;
    CHECK_RET(spvc_context_create(&context), SPVC_SUCCESS, "Failed to create spvc_context");
    spvc_context_set_error_callback(context, error_callback, NULL);
    CHECK_RET(spvc_context_parse_spirv(context, file, size / sizeof(SpvId), &ir), SPVC_SUCCESS, "Failed to parse spirv ir");
    CHECK_RET(spvc_context_create_compiler(context, SPVC_BACKEND_NONE, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler), SPVC_SUCCESS, "Failed to create SPVC_BACKEND_NONE compiler");
    CHECK_RET(spvc_compiler_create_compiler_options(compiler, &options), SPVC_SUCCESS, "Failed to create compiler options");
    CHECK_RET(spvc_compiler_install_compiler_options(compiler, options), SPVC_SUCCESS, "Failed to install compiler options");
    CHECK_RET(spvc_compiler_create_shader_resources(compiler, &resources), SPVC_SUCCESS, "Failed to create shader resources");
    dump_resources(compiler, resources);

    spvc_context_release_allocations(context);

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_GPU_SHADER_CREATE_NAME_STRING, path);
    SDL_GPUShaderCreateInfo info = {
            .code = file,
            .code_size = size,
            .entrypoint = "main",
            .format = SDL_GPU_SHADERFORMAT_SPIRV,
            .stage = stage,
            .props = props,
    };
    SDL_GPUShader *shader = SDL_CreateGPUShader(device, &info);
    SDL_free(file);
    return shader;
}

int main(void) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_PRINT_ERROR_AND_EXIT("SDL_Init");
    }

    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* window = SDL_CreateWindow("Kingdom Defense", (int)(800 * main_scale), (int)(600 * main_scale), window_flags);
    if (window == NULL) {
        SDL_PRINT_ERROR_AND_EXIT("Failed to create window");
    }

    SDL_GPUDevice *gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, NULL);
    if (!gpu) {
        SDL_PRINT_ERROR_AND_EXIT("Create gpu failed");
    }

    SDL_GPUShader *simple_vert = load_shader(gpu, "simple.frag.hlsl.spirv", SDL_GPU_SHADERSTAGE_VERTEX);
    if (simple_vert == NULL) {
        SDL_PRINT_ERROR_AND_EXIT("failed to load simple_vert shader");
    }


    if (!SDL_ClaimWindowForGPUDevice(gpu, window)) {
        SDL_PRINT_ERROR_AND_EXIT("Failed to claim window for gpu device");
    }

    igCreateContext(NULL);
    ImGuiIO* io = igGetIO_Nil();
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    igStyleColorsDark(NULL);

    ImGuiStyle* style = igGetStyle();
    ImGuiStyle_ScaleAllSizes(style, main_scale);
    style->FontScaleDpi = main_scale;

    ImGui_ImplSDL3_InitForSDLGPU(window);
    ImGui_ImplSDLGPU3_InitInfo imgui_init_info = {
            .Device = gpu,
            .MSAASamples = 4,
            .ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpu, window),
    };
    ImGui_ImplSDLGPU3_Init(&imgui_init_info);

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

        /*if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            continue;
        }*/

        SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(gpu);

        SDL_GPUTexture *window_texture;
        uint32_t width = 0;
        uint32_t height = 0;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window, &window_texture, &width, &height)) {
            SDL_PRINT_ERROR_AND_EXIT("Failed to acquire GPU Swapchain texture");
        }

        SDL_GPUColorTargetInfo target_info = {
                .texture = window_texture,
                .clear_color = (SDL_FColor) { .r = 0.1f, .g = 0.1f, .b = 0.1f, .a = 1.0f },
                .mip_level = 0,
                .load_op = SDL_GPU_LOADOP_CLEAR,
                .store_op = SDL_GPU_STOREOP_STORE,
        };
        SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, NULL);
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

    SDL_ReleaseWindowFromGPUDevice(gpu, window);
    SDL_DestroyGPUDevice(gpu);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
