//
// Created by nth on 27/07/25.
//

#include <backends/imgui_impl_sdlgpu3.h>

typedef struct {
    SDL_GPUDevice*       Device             = nullptr;
    SDL_GPUTextureFormat ColorTargetFormat  = SDL_GPU_TEXTUREFORMAT_INVALID;
    SDL_GPUSampleCount   MSAASamples        = SDL_GPU_SAMPLECOUNT_1;
} imgui_sdl3gpu_wrapper_init_info_t;

bool ImGui_ImplSDLGPU3_Init_wrapper(imgui_sdl3gpu_wrapper_init_info_t* info) {
    ImGui_ImplSDLGPU3_InitInfo i = {
            .Device             = info->Device,
            .ColorTargetFormat  = info->ColorTargetFormat,
            .MSAASamples        = info->MSAASamples,
    };
    return ImGui_ImplSDLGPU3_Init(&i);
}

void ImGui_ImplSDLGPU3_Shutdown_wrapper() {
    ImGui_ImplSDLGPU3_Shutdown();
}

void ImGui_ImplSDLGPU3_NewFrame_wrapper() {
    ImGui_ImplSDLGPU3_NewFrame();
}

void     ImGui_ImplSDLGPU3_PrepareDrawData_wrapper(ImDrawData* draw_data, SDL_GPUCommandBuffer* command_buffer) {

}

void     ImGui_ImplSDLGPU3_RenderDrawData_wrapper(ImDrawData* draw_data, SDL_GPUCommandBuffer* command_buffer, SDL_GPURenderPass* render_pass, SDL_GPUGraphicsPipeline* pipeline = nullptr);

// Use if you want to reset your rendering device without losing Dear ImGui state.
void     ImGui_ImplSDLGPU3_CreateDeviceObjects_wrapper() {
    ImGui_ImplSDLGPU3_CreateDeviceObjects();
}
void     ImGui_ImplSDLGPU3_DestroyDeviceObjects_wrapper() {
    ImGui_ImplSDLGPU3_DestroyDeviceObjects();
}

// (Advanced) Use e.g. if you need to precisely control the timing of texture updates (e.g. for staged rendering), by setting ImDrawData::Textures = NULL to handle this manually.
void     ImGui_ImplSDLGPU3_UpdateTexture_wrapper(ImTextureData* tex);