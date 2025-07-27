#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>

#define SDL_PRINT_ERROR_AND_EXIT(_err)                \
    do {                                              \
        fprintf(stderr, _err ": %s", SDL_GetError()); \
        exit(-1);                                     \
    } while (false);

int main(void) {
    printf("Hello, World!\n");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_PRINT_ERROR_AND_EXIT("SDL_Init");
    }

    SDL_Window* window = SDL_CreateWindow("Kingdom Defense", 800, 600, 0);
    if (window == NULL) {
        SDL_PRINT_ERROR_AND_EXIT("Failed to create window");
    }

    SDL_GPUDevice *gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
    if (!gpu) {
        SDL_PRINT_ERROR_AND_EXIT("Create gpu failed");
    }

    if (!SDL_ClaimWindowForGPUDevice(gpu, window)) {
        SDL_PRINT_ERROR_AND_EXIT("Failed to claim window for gpu device");
    }

    bool quit = false;

    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                quit = true;
            }
            // process events
        }

        SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(gpu);

        SDL_GPUTexture *window_texture;
        uint32_t width = 0;
        uint32_t height = 0;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window, &window_texture, &width, &height)) {
            SDL_PRINT_ERROR_AND_EXIT("Failed to acquire GPU Swapchain texture");
        }

        SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(command_buffer, &((SDL_GPUColorTargetInfo) {
            .texture = window_texture,
                .clear_color = (SDL_FColor) { .r = 0.1f, .g = 0.1f, .b = 0.1f, .a = 1.0f },
                .mip_level = 0,
                .load_op = SDL_GPU_LOADOP_CLEAR,
                .store_op = SDL_GPU_STOREOP_STORE,
        }), 1, nullptr);

        SDL_EndGPURenderPass(render_pass);

        SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer);
        if (!SDL_WaitForGPUFences(gpu, true, &fence, 1)) {
            SDL_PRINT_ERROR_AND_EXIT("Wait for Gpu Fences failed");
        }
    }

    return 0;
}
