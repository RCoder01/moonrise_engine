cmake_minimum_required(VERSION 3.0...3.25)

project(
    moonrise_engine
    VERSION 0.1.0
    LANGUAGES CXX C
)

add_executable(game_engine_webgpu source.cpp)

set_target_properties(game_engine_webgpu PROPERTIES
    CXX_STANDARD 20
    CXX_EXTENSIONS OFF
    # COMPILE_WARNING_AS_ERROR ON
)

if (MSVC)
    target_compile_options(game_engine_webgpu PRIVATE /W4)
else()
    target_compile_options(game_engine_webgpu PRIVATE -Wall -Wextra -pedantic)
endif()

if(XCODE)
    set_target_properties(game_engine_webgpu PROPERTIES
        XCODE_GENERATE_SCHEME ON
        XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal")
endif()


if (NOT EMSCRIPTEN)
    set(SDL2_DIR "Third Party/SDL2-2.28.5/cmake")
    set(SDL2_mixer_DIR "Third Party/SDL2_mixer-2.6.3/cmake")
    find_package(SDL2_mixer REQUIRED CONFIG REQUIRED COMPONENTS SDL2_mixer)
endif()
find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)

set(sdl2webgpu_DIR "Third Party/sdl2webgpu-main")
add_subdirectory(${sdl2webgpu_DIR} sdl2webgpu)
# find_package(sdl2webgpu REQUIRED CONFIG REQUIRED COMPONENTS sdl2webgpu)
set(webgpu_DIR "Third Party/WebGPU")
add_subdirectory(${webgpu_DIR} webgpu)

include_directories("Third Party/rapidjson-1.1.0/include")
include_directories("Third Party/glm-0.9.9.8")
include(include_lua.cmake)
add_subdirectory("Third Party/LuaBridge-2.8")

set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE INTERNAL "" FORCE)
set(TINYGLTF_USE_RAPIDJSON ON CACHE INTERNAL "" FORCE)
set(tinygltf_DIR "Third Party/tinygltf-2.8.21")
add_subdirectory(${tinygltf_DIR} tinygltf)


target_link_libraries(game_engine_webgpu PRIVATE 
    SDL2::SDL2
    webgpu
    sdl2webgpu
    tinygltf
    lua
    LuaBridge
)

if (NOT EMSCRIPTEN)
    target_link_libraries(game_engine_webgpu PRIVATE SDL2_mixer::SDL2_mixer)
endif()

if (EMSCRIPTEN)
    target_link_options(game_engine_webgpu PRIVATE
        -sWASM=1
        --use-port=sdl2
        --use-port=sdl2_mixer
        -sUSE_WEBGPU
        -sASYNCIFY
        --preload-file "${CMAKE_CURRENT_SOURCE_DIR}/webgpu_resources@/"
        -sSTACK_SIZE=1048576
        -sALLOW_MEMORY_GROWTH
        -O3
    )
    set_target_properties(game_engine_webgpu PROPERTIES SUFFIX ".html")
endif()
