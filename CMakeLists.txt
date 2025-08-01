cmake_minimum_required(VERSION 3.31)
project(kingdom_defense C CXX)
set(CMAKE_C_STANDARD 11)

include(FetchContent)

# cimgui
FetchContent_Declare(
        sdl3
        URL https://github.com/libsdl-org/SDL/releases/download/release-3.2.18/SDL3-3.2.18.tar.gz
)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(sdl3)
include(cimgui/backend_test/cmake/GenerateCimguiBindings.cmake)
set(IMGUI_STATIC true)
set(includelist ${sdl3_SOURCE_DIR}/include)
GenerateCimguiBindings(cimgui_with_backend sdl3 sdlgpu3 includelist)
target_link_libraries(cimgui_with_backend PRIVATE SDL3::SDL3)


# shadercross
file(GLOB shader_sources "${CMAKE_SOURCE_DIR}/shaders/*.hlsl")
foreach (SRC ${shader_sources})
    get_filename_component(FILENAME ${SRC} NAME)
    set(OUT_FILE "${CMAKE_BINARY_DIR}/${FILENAME}.spirv")

    add_custom_command(
            OUTPUT ${OUT_FILE}
            COMMAND ${CMAKE_SOURCE_DIR}/shadercross/bin/shadercross ${SRC} -s HLSL -d SPIRV -o ${OUT_FILE}
            DEPENDS ${SRC}
            COMMENT "Running command for ${SRC}"
            VERBATIM
    )

    # Collect outputs to tie into the custom target
    list(APPEND SHADER_TARGETS ${OUT_FILE})
endforeach ()
add_custom_target(shaders ALL DEPENDS ${SHADER_TARGETS})

FetchContent_Declare(
        SPIRV-Cross
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
        GIT_TAG vulkan-sdk-1.4.321.0
)
FetchContent_MakeAvailable(SPIRV-Cross)

# main executable
add_executable(kingdom_defense
        main.c
)
set_target_properties(kingdom_defense PROPERTIES LINKER_LANGUAGE CXX)
target_compile_definitions(
        kingdom_defense
        PRIVATE
        CIMGUI_DEFINE_ENUMS_AND_STRUCTS=1
        CIMGUI_USE_SDL3=1
        CIMGUI_USE_SDLGPU3=1
)

target_link_libraries(kingdom_defense SDL3::SDL3)
target_link_libraries(kingdom_defense cimgui_with_backend)
target_link_libraries(kingdom_defense spirv-cross-reflect)
target_link_libraries(kingdom_defense spirv-cross-c)
add_dependencies(kingdom_defense shaders)
