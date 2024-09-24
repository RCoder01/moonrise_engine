#pragma once
#include <iostream>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include "webgpu/webgpu.h"
#include "sdl2webgpu.h"
#include "SDL.h"
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/string_cast.hpp"
#include "glm/ext.hpp"
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_USE_RAPIDJSON
#define TINYGLTF_NO_INCLUDE_RAPIDJSON
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"
#if __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define HOLDER(T) using WGPU##T##Holder = std::unique_ptr<WGPU##T##Impl, decltype(&wgpu##T##Release)>
HOLDER(Instance);
HOLDER(Adapter);
HOLDER(Device);
HOLDER(Queue);
#if defined(__EMSCRIPTEN__)
HOLDER(SwapChain);
#endif
HOLDER(Surface);
HOLDER(TextureView);
HOLDER(CommandEncoder);
HOLDER(RenderPassEncoder);
HOLDER(CommandBuffer);
HOLDER(RenderPipeline);
HOLDER(ShaderModule);
HOLDER(BindGroupLayout);
HOLDER(PipelineLayout);
HOLDER(BindGroup);
HOLDER(Sampler);

void bufferDeleter(WGPUBuffer buffer) {
    wgpuBufferDestroy(buffer);
    wgpuBufferRelease(buffer);
}
using WGPUBufferHolder = std::unique_ptr<WGPUBufferImpl, decltype(&bufferDeleter)>;

void textureDeleter(WGPUTexture texture) {
    std::cout << "Attempting to free texture: " << texture << std::endl;

    wgpuTextureDestroy(texture);
    std::cout << "Destroyed texture: " << texture << std::endl;

    wgpuTextureRelease(texture);
    std::cout << "Released texture: " << texture << std::endl;
}
using WGPUTextureHolder = std::unique_ptr<WGPUTextureImpl, decltype(&textureDeleter)>;

using SDL_WindowHolder = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;

template<typename T>
void voidDeleter(T*) {}

template<typename T, typename Deleter>
void delete_ptr(std::unique_ptr<T, Deleter>& ptr) {
    ptr.get_deleter()(ptr.release());
}

#if defined(__EMSCRIPTEN__)
constexpr const char* base_path = "";
#else
constexpr const char* base_path = "webgpu_resources/";
#endif

std::string get_file_contents(const char *filename) {
    std::FILE *fp = fopen((std::string(base_path) + filename).c_str(), "rb");
    if (!fp) {
        std::cout << "Could not open file: " << filename << std::endl;
        std::perror("Error: ");
        throw(errno);
    }
    std::string contents;
    std::fseek(fp, 0, SEEK_END);
    contents.resize(std::ftell(fp));
    std::rewind(fp);
    std::fread(&contents[0], 1, contents.size(), fp);
    std::fclose(fp);
    return(contents);
}

struct Uniforms {
    glm::mat4x4 projection;
    glm::mat4x4 view;
};
static_assert(sizeof(glm::mat4x4) == (sizeof(float) * 4 * 4));
constexpr size_t TRANSFORM_SIZE = sizeof(glm::mat4x4);

struct PrimitiveUniforms {
    glm::mat4x4 transform;
    glm::vec4 base_color;
    uint32_t has_texture; // actually just a bool
    std::array<float, 3> _padding;
};

struct ModelUniforms {
    glm::mat4x4 transform;
};

constexpr WGPUTextureFormat DEPTH_TEXTURE_FORMAT = WGPUTextureFormat_Depth24Plus;

struct Rect {
    uint32_t width;
    uint32_t height;
};
constexpr Rect SCREEN_SIZE = {640, 480};
constexpr uint32_t MAX_TEXTURE_SIZE = 4096;
constexpr uint32_t COPY_BUFFER_ALIGNMENT = 4;

struct RenderConfig {
	glm::ivec2 size = { 640, 360 };
	glm::u8vec3 clear_color = { 255, 255, 255 };
	float zoom = 1.f;

	RenderConfig();
};

void deviceLostCallback(WGPUDeviceLostReason reason, char const* message, void* /* pUserData */) {
    std::cerr << "Device lost: " << reason;
    if (message) std::cerr << " (" << message << ")";
    std::cerr << std::endl;
}

WGPUAdapter requestAdapter(WGPUInstance instance, WGPURequestAdapterOptions const * options) {
    struct UserData {
        WGPUAdapter adapter = nullptr;
        bool requestEnded = false;
    };
    UserData userData;

    auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const * message, void * pUserData) {
        UserData& userData = *reinterpret_cast<UserData*>(pUserData);
        if (status == WGPURequestAdapterStatus_Success) {
            userData.adapter = adapter;
        } else {
            std::cerr << "Could not get WebGPU adapter: " << message << std::endl;
        }
        userData.requestEnded = true;
    };

    wgpuInstanceRequestAdapter(
        instance,
        options,
        onAdapterRequestEnded,
        (void*)&userData
    );

    while (!userData.requestEnded) {
        #ifdef __EMSCRIPTEN__
		emscripten_sleep(100);
        #endif
    }
    return userData.adapter;
}

WGPUDevice requestDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor) {
    struct UserData {
        WGPUDevice device = nullptr;
        bool requestEnded = false;
    };
    UserData userData;

    auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, char const * message, void * pUserData) {
        UserData& userData = *static_cast<UserData*>(pUserData);
        if (status == WGPURequestDeviceStatus_Success) {
            userData.device = device;
        } else {
            std::cerr << "Could not get WebGPU device: " << message << std::endl;
        }
        userData.requestEnded = true;
    };

    wgpuAdapterRequestDevice(adapter, descriptor, onDeviceRequestEnded, (void*)&userData);
    while (!userData.requestEnded) {
        #ifdef __EMSCRIPTEN__
		emscripten_sleep(100);
        #endif
    }
    return userData.device;
}

WGPUInstanceHolder getInstance() {
    WGPUInstanceDescriptor desc = {nullptr};
    #if defined(__EMSCRIPTEN__)
    WGPUInstance instance = wgpuCreateInstance(nullptr);
    #else
    WGPUInstance instance = wgpuCreateInstance(&desc);
    #endif
    if (!instance) {
        std::cerr << "Could not initialize WebGPU!" << std::endl;
        exit(1);
    }
    std::cout << "WGPU instance: " << instance << std::endl;
    return WGPUInstanceHolder(instance, wgpuInstanceRelease);
}

SDL_Window* initSDL(const char* window_title, const RenderConfig& config) {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Could not initialize SDL! Error: " << SDL_GetError() << std::endl;
        exit(1);
    }
    SDL_Window *window = SDL_CreateWindow(
        window_title,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        config.size.x,
        config.size.y,
        SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "Could not create window! Error: " << SDL_GetError() << std::endl;
        exit(1);
    }
    return window;
}

std::vector<WGPUFeatureName> getAdapterFeatures(WGPUAdapter adapter) {
    std::vector<WGPUFeatureName> features;
    size_t featureCount = wgpuAdapterEnumerateFeatures(adapter, nullptr);
    features.resize(featureCount);
    wgpuAdapterEnumerateFeatures(adapter, features.data());
    return features;
}

WGPUAdapterHolder getAdapter(WGPUInstance instance, WGPUSurface surface) {
    std::cout << "Requesting adapter..." << std::endl;
    WGPURequestAdapterOptions adapterOpts = {
        .nextInChain = nullptr,
        .compatibleSurface = surface,
        .powerPreference = WGPUPowerPreference_Undefined,
        .forceFallbackAdapter = false,
    };
    WGPUAdapter adapter = requestAdapter(instance, &adapterOpts);
    if (!adapter) {
        std::cerr << "Could not get WebGPU adapter!" << std::endl;
        exit(1);
    }

    std::cout << "Received adapter: " << adapter << std::endl;
    std::vector<WGPUFeatureName> features = getAdapterFeatures(adapter);
    std::cout << "Adapter features:" << std::endl;
    for (WGPUFeatureName f : features) {
        std::cout << " - " << f << std::endl;
    }
    return WGPUAdapterHolder(adapter, wgpuAdapterRelease);
}

WGPUDeviceHolder getDevice(WGPUAdapter adapter, const WGPULimits& adapterLimits) {
    std::cout << "Requesting device..." << std::endl;
    WGPURequiredLimits requiredLimits = {
        .nextInChain = nullptr,
        .limits = {
            .maxTextureDimension1D = MAX_TEXTURE_SIZE,
            .maxTextureDimension2D = MAX_TEXTURE_SIZE,
            .maxTextureArrayLayers = 1,
            .maxBindGroups = 3,
            .maxBindingsPerBindGroup = 3,
            .maxSampledTexturesPerShaderStage = 1,
            .maxSamplersPerShaderStage = 1,
            .maxStorageTexturesPerShaderStage = 1,
            .maxUniformBuffersPerShaderStage = 1,
            .maxUniformBufferBindingSize = 1 << 12,
            .minUniformBufferOffsetAlignment = adapterLimits.minUniformBufferOffsetAlignment,
            .minStorageBufferOffsetAlignment = adapterLimits.minStorageBufferOffsetAlignment,
            .maxVertexBuffers = 4,
            .maxBufferSize = 1 << 21,
            .maxVertexAttributes = 7,
            .maxVertexBufferArrayStride = TRANSFORM_SIZE,
            .maxInterStageShaderComponents = 5,
        }
    };
    WGPUDeviceDescriptor deviceDesc = {
        .nextInChain = nullptr,
        .label = "My Device",
        .requiredLimits = &requiredLimits,
        .defaultQueue = {
            .nextInChain = nullptr,
            .label = "The default queue",
        },
        .deviceLostCallback = &deviceLostCallback,
    };
    WGPUDevice device = requestDevice(adapter, &deviceDesc);
    auto onDeviceError = [](WGPUErrorType type, char const* message, void* /* pUserData */) {
        std::cerr << "Uncaptured device error: type " << type;
        if (message) std::cerr << " (" << message << ")";
        std::cerr << std::endl;
    };
    wgpuDeviceSetUncapturedErrorCallback(device, onDeviceError, nullptr /* pUserData */);
    if (!device) {
        std::cerr << "Could not get WebGPU device!" << std::endl;
        exit(1);
    }
    std::cout << "Received device: " << device << std::endl;
    return WGPUDeviceHolder(device, wgpuDeviceRelease);
}

WGPUQueueHolder getQueue(WGPUDevice device) {
    std::cout << "Requesting queue..." << std::endl;
    WGPUQueue queue = wgpuDeviceGetQueue(device);
    auto onQueueWorkDone = [](WGPUQueueWorkDoneStatus status, void* /* pUserData */) {
        std::cout << "Queued work finished with status: " << status << std::endl;
    };
    wgpuQueueOnSubmittedWorkDone(queue, onQueueWorkDone, nullptr /* pUserData */);
    if (!queue) {
        std::cerr << "Could not get WebGPU queue!" << std::endl;
        exit(1);
    }
    std::cout << "Received queue: " << queue << std::endl;
    return WGPUQueueHolder(queue, wgpuQueueRelease);
}

#if defined(__EMSCRIPTEN__)
WGPUSwapChainHolder getSwapChain(WGPUSurface surface, WGPUDevice device, WGPUTextureFormat swapChainFormat, Rect screen_size) {
    std::cout << "Creating swapchain device..." << std::endl;
    WGPUSwapChainDescriptor swapChainDesc = {
        .nextInChain = nullptr,
        .label = "Swap chain",
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = swapChainFormat,
        .width = screen_size.width,
        .height = screen_size.height,
        .presentMode = WGPUPresentMode_Fifo,
    };
    WGPUSwapChain swapChain = wgpuDeviceCreateSwapChain(device, surface, &swapChainDesc);
    if (!swapChain) {
        std::cerr << "Could not create WebGPU swap chain!" << std::endl;
        exit(1);
    }
    std::cout << "Received swapchain: " << swapChain << std::endl;
    return WGPUSwapChainHolder(swapChain, wgpuSwapChainRelease);
}

WGPUTextureViewHolder getNextTexture(WGPUSwapChain swapChain) {
    WGPUTextureView nextTexture = wgpuSwapChainGetCurrentTextureView(swapChain);
    if (!nextTexture) {
        std::cerr << "Cannot acquire next swap chain texture" << std::endl;
        exit(1);
    }
    return WGPUTextureViewHolder(nextTexture, wgpuTextureViewRelease);
}
#endif

WGPUShaderModuleHolder createShaderModule(WGPUDevice device) {
    std::string shader = get_file_contents("resources/shader.wgsl");
    std::size_t pos = shader.find("DO_COLOR_CORRECTION");
    if (pos != std::string::npos) {
        #if defined(WEBGPU_BACKEND_WGPU)
        shader.replace(pos, 19, "true               ");
        #elif defined(WEBGPU_BACKEND_DAWN) or defined(__EMSCRIPTEN__)
        shader.replace(pos, 19, "false              ");
        #endif
    }
    char* shader_text = new char[shader.size() + 1];
    std::copy(shader.begin(), shader.end(), shader_text);
    shader_text[shader.size()] = '\0';

    WGPUShaderModuleWGSLDescriptor shaderCodeDesc = {
        .chain = {
            .next = nullptr,
            .sType = WGPUSType_ShaderModuleWGSLDescriptor,
        },
        .code = shader_text,
    };
    WGPUShaderModuleDescriptor shaderDesc = {
        .nextInChain = &shaderCodeDesc.chain,
        .label = "Shader module",
        #ifdef WEBGPU_BACKEND_WGPU
        .hintCount = 0,
        .hints = nullptr,
        #endif
    };
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);
    if (!shaderModule) {
        std::cerr << "Could not create shader module!" << std::endl;
        exit(1);
    }
    std::cout << "Created shader module: " << shaderModule << std::endl;
    return WGPUShaderModuleHolder(shaderModule, wgpuShaderModuleRelease);
}

WGPUBindGroupLayoutHolder createBindGroupLayout(WGPUDevice device) {
    WGPUBindGroupLayoutEntry bindGroupLayoutEntry = {
        .binding = 0,
        .visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment,
        .buffer = {
            .type = WGPUBufferBindingType_Uniform,
            .hasDynamicOffset = false,
            .minBindingSize = sizeof(Uniforms),
        },
        .sampler = {},
        .texture = {},
    };
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {
        .nextInChain = nullptr,
        .label = "Bind group layout",
        .entryCount = 1,
        .entries = &bindGroupLayoutEntry,
    };
    WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);
    if (!bindGroupLayout) {
        std::cerr << "Could not create bind group layout!" << std::endl;
        exit(1);
    }
    std::cout << "Created bind group layout: " << bindGroupLayout << std::endl;
    return WGPUBindGroupLayoutHolder(bindGroupLayout, wgpuBindGroupLayoutRelease);
}

WGPUBindGroupHolder createBindGroup(WGPUDevice device, WGPUBindGroupDescriptor layout) {
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, &layout);
    if (!bindGroup) {
        std::cerr << "Could not create bind group!" << std::endl;
        exit(1);
    }
    std::cout << "Created bind group: " << bindGroup << std::endl;
    return WGPUBindGroupHolder(bindGroup, wgpuBindGroupRelease);
}

WGPUBindGroupHolder createUniformsBindGroup(WGPUDevice device, WGPUBindGroupLayout layout, WGPUBuffer buffer) {
    WGPUBindGroupEntry bindGroupEntry = {
        .binding = 0,
        .buffer = buffer,
        .offset = 0,
        .size = sizeof(Uniforms),
    };
    WGPUBindGroupDescriptor bindGroupDesc = {
        .nextInChain = nullptr,
        .label = "Uniform bind group",
        .layout = layout,
        .entryCount = 1,
        .entries = &bindGroupEntry,
    };
    return createBindGroup(device, bindGroupDesc);
}

WGPUBindGroupLayoutHolder createModelBindGroupLayout(WGPUDevice device) {
    std::array bindGroupLayoutEntry = {
        WGPUBindGroupLayoutEntry{
            .binding = 0,
            .visibility = WGPUShaderStage_Vertex,
            .buffer = {
                .type = WGPUBufferBindingType_Uniform,
                .hasDynamicOffset = false,
                .minBindingSize = sizeof(ModelUniforms),
            },
            .sampler = {},
            .texture = {},
        },
    };
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {
        .nextInChain = nullptr,
        .label = "Model bind group layout",
        .entryCount = static_cast<uint32_t>(bindGroupLayoutEntry.size()),
        .entries = bindGroupLayoutEntry.data(),
    };
    WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);
    if (!bindGroupLayout) {
        std::cerr << "Could not create model bind group layout!" << std::endl;
        exit(1);
    }
    std::cout << "Created model bind group layout: " << bindGroupLayout << std::endl;
    return WGPUBindGroupLayoutHolder(bindGroupLayout, wgpuBindGroupLayoutRelease);

}

WGPUBindGroupLayoutHolder createPrimitiveBindGroupLayout(WGPUDevice device) {
    std::array bindGroupLayoutEntry = {
        WGPUBindGroupLayoutEntry{
            .binding = 0,
            .visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment,
            .buffer = {
                .type = WGPUBufferBindingType_Uniform,
                .hasDynamicOffset = false,
                .minBindingSize = sizeof(PrimitiveUniforms),
            },
            .sampler = {},
            .texture = {},
        },
        WGPUBindGroupLayoutEntry{
            .binding = 1,
            .visibility = WGPUShaderStage_Fragment,
            .buffer = {},
            .sampler = {},
            .texture = {
                .sampleType = WGPUTextureSampleType_Float,
                .viewDimension = WGPUTextureViewDimension_2D,
                .multisampled = false,
            },
        },
        WGPUBindGroupLayoutEntry{
            .binding = 2,
            .visibility = WGPUShaderStage_Fragment,
            .buffer = {},
            .sampler = {
                .type = WGPUSamplerBindingType_Filtering,
            },
            .texture = {},
        },
    };
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {
        .nextInChain = nullptr,
        .label = "Primitive bind group layout",
        .entryCount = static_cast<uint32_t>(bindGroupLayoutEntry.size()),
        .entries = bindGroupLayoutEntry.data(),
    };
    WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);
    if (!bindGroupLayout) {
        std::cerr << "Could not create primitive bind group layout!" << std::endl;
        exit(1);
    }
    std::cout << "Created primitive bind group layout: " << bindGroupLayout << std::endl;
    return WGPUBindGroupLayoutHolder(bindGroupLayout, wgpuBindGroupLayoutRelease);
}

template<unsigned int N>
WGPUPipelineLayoutHolder createPipelineLayout(WGPUDevice device, std::array<WGPUBindGroupLayout, N> bindGroupLayouts) {
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {
        .nextInChain = nullptr,
        .label = "Pipeline layout",
        .bindGroupLayoutCount = bindGroupLayouts.size(),
        .bindGroupLayouts = bindGroupLayouts.data(),
    };
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);
    if (!pipelineLayout) {
        std::cerr << "Could not create pipeline layout!" << std::endl;
        exit(1);
    }
    std::cout << "Created pipeline layout: " << pipelineLayout << std::endl;
    return WGPUPipelineLayoutHolder(pipelineLayout, wgpuPipelineLayoutRelease);
}

WGPURenderPipelineHolder createRenderPipeline(WGPUDevice device, WGPUPipelineLayout layout, WGPUShaderModule shaders, WGPUTextureFormat swapChainFormat) {
    WGPUBlendState blendState = {
        .color = {
            .operation = WGPUBlendOperation_Add,
            .srcFactor = WGPUBlendFactor_SrcAlpha,
            .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
        },
        .alpha = {
            .operation = WGPUBlendOperation_Add,
            .srcFactor = WGPUBlendFactor_Zero,
            .dstFactor = WGPUBlendFactor_One,
        }
    };
    WGPUColorTargetState colorTargetState = {
        .nextInChain = nullptr,
        .format = swapChainFormat,
        .blend = &blendState,
        .writeMask = WGPUColorWriteMask_All,
    };
    WGPUVertexAttribute positionAttrib = {
        .format = WGPUVertexFormat_Float32x3,
        .offset = 0,
        .shaderLocation = 0,
    };
    WGPUVertexBufferLayout vertexPositionBufferLayout = {
        .arrayStride = 12,
        .stepMode = WGPUVertexStepMode_Vertex,
        .attributeCount = 1,
        .attributes = &positionAttrib,
    };
    WGPUVertexAttribute normalAttrib = {
        .format = WGPUVertexFormat_Float32x3,
        .offset = 0,
        .shaderLocation = 1,
    };
    WGPUVertexBufferLayout vertexNormalBufferLayout = {
        .arrayStride = 12,
        .stepMode = WGPUVertexStepMode_Vertex,
        .attributeCount = 1,
        .attributes = &normalAttrib,
    };
    WGPUVertexAttribute texcoordAttrib = {
        .format = WGPUVertexFormat_Float32x2,
        .offset = 0,
        .shaderLocation = 2,
    };
    WGPUVertexBufferLayout vertexTexcoordBufferLayout = {
        .arrayStride = 8,
        .stepMode = WGPUVertexStepMode_Vertex,
        .attributeCount = 1,
        .attributes = &texcoordAttrib,
    };
    std::array instanceTransformAttribs = {
        WGPUVertexAttribute{
            .format = WGPUVertexFormat_Float32x4,
            .offset = 0,
            .shaderLocation = 3,
        },
        WGPUVertexAttribute{
            .format = WGPUVertexFormat_Float32x4,
            .offset = sizeof(float) * 4,
            .shaderLocation = 4,
        },
        WGPUVertexAttribute{
            .format = WGPUVertexFormat_Float32x4,
            .offset = sizeof(float) * 8,
            .shaderLocation = 5,
        },
        WGPUVertexAttribute{
            .format = WGPUVertexFormat_Float32x4,
            .offset = sizeof(float) * 12,
            .shaderLocation = 6,
        },
    };
    WGPUVertexBufferLayout instanceTransformBufferLayout = {
        .arrayStride = TRANSFORM_SIZE,
        .stepMode = WGPUVertexStepMode_Instance,
        .attributeCount = static_cast<uint32_t>(instanceTransformAttribs.size()),
        .attributes = instanceTransformAttribs.data(),
    };
    std::array vertexBufferLayouts = {
        vertexPositionBufferLayout,
        vertexNormalBufferLayout,
        vertexTexcoordBufferLayout,
        instanceTransformBufferLayout,
    };
    WGPUFragmentState fragment = {
        .nextInChain = nullptr,
        .module = shaders,
        .entryPoint = "fragment_main",
        .constantCount = 0,
        .constants = nullptr,
        .targetCount = 1,
        .targets = &colorTargetState,
    };
    WGPUDepthStencilState depthStencilState = {
        .nextInChain = nullptr,
        .format = DEPTH_TEXTURE_FORMAT,
        .depthWriteEnabled = true,
        .depthCompare = WGPUCompareFunction_Less,
        .stencilFront = {
            .compare = WGPUCompareFunction_Always,
            .failOp = WGPUStencilOperation_Keep,
            .depthFailOp = WGPUStencilOperation_Keep,
            .passOp = WGPUStencilOperation_Keep,
        },
        .stencilBack = {
            .compare = WGPUCompareFunction_Always,
            .failOp = WGPUStencilOperation_Keep,
            .depthFailOp = WGPUStencilOperation_Keep,
            .passOp = WGPUStencilOperation_Keep,
        },
        .stencilReadMask = 0,
        .stencilWriteMask = 0,
        .depthBias = 0,
        .depthBiasSlopeScale = 0.,
        .depthBiasClamp = 0.,
    };
    WGPURenderPipelineDescriptor pipelineDesc = {
        .nextInChain = nullptr,
        .label = "Render pipeline",
        .layout = layout,
        .vertex = {
            .nextInChain = nullptr,
            .module = shaders,
            .entryPoint = "vertex_main",
            .constantCount = 0,
            .constants = nullptr,
            .bufferCount = static_cast<uint32_t>(vertexBufferLayouts.size()),
            .buffers = vertexBufferLayouts.data(),
        },
        .primitive = {
            .nextInChain = nullptr,
            .topology = WGPUPrimitiveTopology_TriangleList,
            .stripIndexFormat = WGPUIndexFormat_Undefined,
            .frontFace = WGPUFrontFace_CCW,
            .cullMode = WGPUCullMode_None,
        },
        .depthStencil = &depthStencilState,
        .multisample = {
            .nextInChain = nullptr,
            .count = 1,
            .mask = static_cast<uint32_t>(-1),
            .alphaToCoverageEnabled = false,
        },
        .fragment = &fragment,
    };
    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);
    if (!pipeline) {
        std::cerr << "Could not create render pipeline!" << std::endl;
        exit(1);
    }
    std::cout << "Created render pipeline: " << pipeline << std::endl;
    return WGPURenderPipelineHolder(pipeline, wgpuRenderPipelineRelease);
}

WGPUTextureViewHolder createDepthTextureView(WGPUTexture texture) {
    WGPUTextureViewDescriptor depthTextureViewDescriptor = {
        .nextInChain = nullptr,
        .label = "Depth texture view",
        .format = DEPTH_TEXTURE_FORMAT,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_DepthOnly,
    };
    WGPUTextureView depthTextureView = wgpuTextureCreateView(texture, &depthTextureViewDescriptor);
    if (!depthTextureView) {
        std::cerr << "Could not create depth texture view!" << std::endl;
        exit(1);
    }
    std::cout << "Created depth texture view: " << depthTextureView << std::endl;
    return WGPUTextureViewHolder(depthTextureView, wgpuTextureViewRelease);
}

WGPUBufferHolder createBuffer(WGPUDevice device, WGPUBufferDescriptor bufferDesc) {
    std::string label = bufferDesc.label ? bufferDesc.label : "";
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &bufferDesc);
    if (!buffer) {
        std::cerr << "Could not create buffer!" << std::endl;
        exit(1);
    }
    std::cout << "Created buffer: " << buffer;
    if (label != "") {
        std::cout << " (" << label << ")";
    }
    std::cout << std::endl;
    return WGPUBufferHolder(buffer, bufferDeleter);
}

WGPUTextureHolder createTexture(WGPUDevice device, WGPUTextureDescriptor textureDesc) {
    std::string label = textureDesc.label ? textureDesc.label : "";
    WGPUTexture texture = wgpuDeviceCreateTexture(device, &textureDesc);
    if (!texture) {
        std::cerr << "Could not create texture!" << std::endl;
        exit(1);
    }
    std::cout << "Created texture: " << texture;
    if (label != "") {
        std::cout << " (" << label << ")";
    }
    std::cout << std::endl;
    return WGPUTextureHolder(texture, textureDeleter);
}

WGPUSamplerHolder createSampler(WGPUDevice device, WGPUSamplerDescriptor samplerDesc) {
    WGPUSampler sampler = wgpuDeviceCreateSampler(device, &samplerDesc);
    if (!sampler) {
        std::cerr << "Could not create sampler!" << std::endl;
        exit(1);
    }
    std::cout << "Created sampler: " << sampler << std::endl;
    return WGPUSamplerHolder(sampler, wgpuSamplerRelease);
}

class ModelHandle {
    size_t index;

    friend class Renderer;
    ModelHandle(size_t index) : index(index) {}
public:
    ModelHandle() : index(std::numeric_limits<size_t>::max()) {}

    bool operator==(const ModelHandle& other) const {
        return index == other.index;
    }
};

struct Transform {
    glm::vec3 translation;
    glm::vec3 rotation;
    glm::vec3 scale;

    glm::mat4x4 toMatrix() const {
        glm::mat4x4 model_scale = glm::scale(glm::mat4x4(1.f), scale);
        glm::mat4x4 model_translation = glm::translate(glm::mat4x4(1.f), translation);
        glm::mat4x4 model_rotation = glm::yawPitchRoll(-rotation.x, rotation.z, rotation.y);
        return model_translation * model_rotation * model_scale;
    }

    Transform() : translation(0.f), rotation(0.f, 0.f, 0.f), scale(1.f) {}

    Transform(glm::vec3 translation, glm::vec3 rotation, glm::vec3 scale) : translation(translation), rotation(rotation), scale(scale) {}
};

template<typename T>
class Table {
    struct Item {
        T data;
        size_t generation;
    };

    std::vector<Item> items;
    std::unordered_set<size_t> free;

public:
    class Index {
        size_t index;
        size_t generation;

        friend class Table;
        Index(size_t index, size_t generation) : index(index), generation(generation) {}
    public:
        size_t get_index() {
            return index;
        }

        bool operator==(const Index& other) const {
            return index == other.index && generation == other.generation;
        }
    };

    Table() {}

    Index add(T data) {
        size_t index;
        if (free.empty()) {
            index = items.size();
            items.push_back({std::move(data), 0});
        } else {
            auto it = free.begin();
            index = *it;
            free.erase(it);
            items[index] = {std::move(data), items[index].generation + 1};
        }
        return Index(index, items[index].generation);
    }

    void remove(Index index) {
        if (index.generation != items[index.index].generation) {
            return;
        }
        free.insert(index.index);
        items[index.index].generation++;
    }

    T& operator[](Index index) {
        if (index.generation != items[index.index].generation) {
            throw std::runtime_error("Invalid index");
        }
        return items[index.index].data;
    }

    const T& operator[](Index index) const {
        if (index.generation != items[index.index].generation) {
            throw std::runtime_error("Invalid index");
        }
        return items[index.index].data;
    }

    bool valid(size_t index) const {
        return index < items.size() && free.find(index) == free.end();
    }

    T& rawget(size_t index) {
        return items[index].data;
    }

    const T& rawget(size_t index) const {
        return items[index].data;
    }

    size_t size() const {
        return items.size() - free.size();
    }

    size_t capacity() const {
        return items.capacity();
    }
};

constexpr size_t UNCHANGED = std::numeric_limits<size_t>::max();
struct DynamicTransformBuffer {
    WGPUDevice device;
    WGPUQueue queue;
    WGPUBufferDescriptor descriptor;
    WGPUBufferHolder buffer = WGPUBufferHolder(nullptr, voidDeleter);
    Table<Transform> data;
    bool changed = false;
    size_t current_instance_count = 0;

public:
    using Index = Table<Transform>::Index;

    DynamicTransformBuffer(WGPUDevice device, WGPUQueue queue, WGPUBufferDescriptor descriptor) : device(device), queue(queue), descriptor(descriptor) {
        this->descriptor.size = 0;
    }

    const Transform& operator[](Index index) const {
        return data[index];
    }

    Transform& operator[](Index index) {
        changed = true;
        return data[index];
    }

    Index add(const Transform& value) {
        changed = true;
        return data.add(value);
    }

    void remove(Index index) {
        changed = true;
        data.remove(index);
    }

    WGPUBuffer getBuffer(std::vector<glm::mat4x4>& ancilla) {
        if (!changed) {
            return buffer.get();
        }
        if (descriptor.size < data.size() * sizeof(glm::mat4x4)) {
            std::cout << "Resizing buffer \"" << descriptor.label << "\" to " << data.size() * sizeof(glm::mat4x4) << " bytes" << std::endl;
            descriptor.size = data.capacity() * sizeof(glm::mat4x4);
            buffer = createBuffer(device, descriptor);
        }
        ancilla.clear();
        for (size_t i = 0; i <= data.capacity(); i++) {
            if (data.valid(i)) {
                ancilla.push_back(data.rawget(i).toMatrix());
            }
        }
        wgpuQueueWriteBuffer(
            queue,
            buffer.get(),
            0,
            ancilla.data(),
            ancilla.size() * sizeof(glm::mat4x4));
        changed = false;
        current_instance_count = data.size();
        return buffer.get();
    }

    size_t count() const {
        return current_instance_count;
    }

    size_t size() const {
        return current_instance_count * sizeof(glm::mat4x4);
    }
};

struct GameConfig;

const char* get_window_title(GameConfig* config);

class Renderer {
    struct ModelPrimitive {
        WGPUBufferHolder position_buffer;
        WGPUBufferHolder normal_buffer;
        WGPUBufferHolder index_buffer;
        WGPUBufferHolder texcoord_buffer;

        WGPUBufferHolder uniform_buffer;
        WGPUTextureHolder texture;
        WGPUTextureViewHolder texture_view;
        WGPUSamplerHolder sampler;
        WGPUBindGroupHolder bind_group;
    };

    struct ModelType {
        std::vector<ModelPrimitive> primitives;
        DynamicTransformBuffer transforms;
        WGPUBufferHolder model_uniform_buffer;
        WGPUBindGroupHolder model_bind_group;
    };
    std::vector<glm::mat4x4> transform_matrix_buffer;
    std::unordered_map<std::string, ModelHandle> model_types;
    std::vector<ModelType> models;

    std::shared_ptr<GameConfig> game_config;
    RenderConfig render_config;

    Transform camera_transform;
    WGPUBufferHolder uniform_buffer = {nullptr, voidDeleter};
    WGPUBindGroupHolder bind_group = {nullptr, voidDeleter};

    WGPUInstanceHolder instance = {nullptr, voidDeleter};
    SDL_WindowHolder window = {nullptr, voidDeleter};
    WGPUSurfaceHolder surface = {nullptr, voidDeleter};
    WGPUAdapterHolder adapter = {nullptr, voidDeleter};
    WGPULimits adapter_limits;
    WGPUDeviceHolder device = {nullptr, voidDeleter};
    WGPULimits device_limits;
    WGPUQueueHolder queue = {nullptr, voidDeleter};
    WGPUShaderModuleHolder shader_module = {nullptr, voidDeleter};
    WGPUBindGroupLayoutHolder bind_group_layout = {nullptr, voidDeleter};
    WGPUPipelineLayoutHolder pipeline_layout = {nullptr, voidDeleter};
    WGPURenderPipelineHolder pipeline = {nullptr, voidDeleter};
    WGPUTextureHolder depth_texture = {nullptr, voidDeleter};
    WGPUTextureViewHolder depth_texture_view = {nullptr, voidDeleter};
    WGPUTextureFormat surface_preferred_format;
    #if defined(__EMSCRIPTEN__)
    WGPUSwapChainHolder swap_chain = {nullptr, voidDeleter};
    #endif
    WGPUBufferHolder default_model_uniform_buffer = {nullptr, voidDeleter};
    WGPUBindGroupHolder default_model_bind_group = {nullptr, voidDeleter};
    WGPUBufferHolder default_primitive_uniform_buffer = {nullptr, voidDeleter};
    WGPUBindGroupLayoutHolder model_bind_group_layout = {nullptr, voidDeleter};
    WGPUBindGroupLayoutHolder primitive_bind_group_layout = {nullptr, voidDeleter};
    WGPUTextureHolder default_texture = {nullptr, voidDeleter};
    WGPUTextureViewHolder default_texture_view = {nullptr, voidDeleter};
    WGPUSamplerHolder default_sampler = {nullptr, voidDeleter};

    WGPUTextureHolder _current_texture = {nullptr, voidDeleter};
    WGPUTextureViewHolder getCurrentTexture() {
        #if defined(__EMSCRIPTEN__)
        return getNextTexture(swap_chain.get());
        #else
        WGPUSurfaceTexture surface_texture;
        wgpuSurfaceGetCurrentTexture(surface.get(), &surface_texture);
        _current_texture = WGPUTextureHolder(surface_texture.texture, wgpuTextureRelease);

        WGPUTextureViewDescriptor current_texture_view_descriptor = {
            .nextInChain = nullptr,
            .label = "Current texture view",
            .format = WGPUTextureFormat_Undefined,
            .dimension = WGPUTextureViewDimension_2D,
            .baseMipLevel = 0,
            .mipLevelCount = 1,
            .baseArrayLayer = 0,
            .arrayLayerCount = 1,
            .aspect = WGPUTextureAspect_All,
        };

        switch (surface_texture.status) {
        case WGPUSurfaceGetCurrentTextureStatus_Success: {
            WGPUTextureView texture_view = wgpuTextureCreateView(_current_texture.get(), &current_texture_view_descriptor);
            return WGPUTextureViewHolder(texture_view, wgpuTextureViewRelease);            
        }
        default:
            std::cerr << "Could not get current texture! (Error Code:" << surface_texture.status << ")" << std::endl;
            exit(1);
        }
        #endif
    }

    void renderFrame(WGPUTextureView current_texture) {
        glm::mat4x4 camera_transform_matrix = glm::inverse(camera_transform.toMatrix());
        wgpuQueueWriteBuffer(queue.get(), uniform_buffer.get(), offsetof(Uniforms, view), &camera_transform_matrix, TRANSFORM_SIZE);

        WGPUCommandEncoderDescriptor encoderDesc = {
            .nextInChain = nullptr,
            .label = "Command encoder",
        };
        WGPUCommandEncoderHolder encoder(wgpuDeviceCreateCommandEncoder(device.get(), &encoderDesc), wgpuCommandEncoderRelease);
        if (!encoder.get()) {
            encoder.release();
            std::cerr << "Cannot create command encoder" << std::endl;
            return;
        }

        WGPURenderPassColorAttachment renderPassColorAttachment = {
            .view = current_texture,
            #if defined(WEBGPU_BACKEND_DAWN) or defined(__EMSCRIPTEN__)
            .depthSlice = static_cast<uint32_t>(-1),
            #endif
            .resolveTarget = nullptr,
            .loadOp = WGPULoadOp_Clear,
            .storeOp = WGPUStoreOp_Store,
            .clearValue = {
                static_cast<float>(render_config.clear_color.r) / 255.f,
                static_cast<float>(render_config.clear_color.g) / 255.f,
                static_cast<float>(render_config.clear_color.b) / 255.f,
                1.f},
        };

        WGPURenderPassDepthStencilAttachment depthStencilAttachment = {
            .view = depth_texture_view.get(),
            .depthLoadOp = WGPULoadOp_Clear,
            .depthStoreOp = WGPUStoreOp_Store,
            .depthClearValue = 1.0,
            .depthReadOnly = false,
            #if defined(WEBGPU_BACKEND_DAWN) or defined(__EMSCRIPTEN__)
            .stencilLoadOp = WGPULoadOp_Undefined,
            .stencilStoreOp = WGPUStoreOp_Undefined,
            #else
            .stencilLoadOp = WGPULoadOp_Clear,
            .stencilStoreOp = WGPUStoreOp_Store,
            #endif
            .stencilClearValue = 0,
            .stencilReadOnly = true,
        };

        WGPURenderPassDescriptor renderPassDesc = {
            .nextInChain = nullptr,
            .label = "Render pass",
            .colorAttachmentCount = 1,
            .colorAttachments = &renderPassColorAttachment,
            .depthStencilAttachment = &depthStencilAttachment,
            .occlusionQuerySet = nullptr,
            .timestampWrites = nullptr,
        };

        WGPURenderPassEncoderHolder render_pass(wgpuCommandEncoderBeginRenderPass(encoder.get(), &renderPassDesc), wgpuRenderPassEncoderRelease);
        wgpuRenderPassEncoderSetPipeline(render_pass.get(), pipeline.get());
        for (auto& model_data : models) {
            renderModelType(render_pass.get(), model_data);
        }
        wgpuRenderPassEncoderEnd(render_pass.get());

        WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
        cmdBufferDescriptor.nextInChain = nullptr;
        cmdBufferDescriptor.label = "Command buffer";
        WGPUCommandBufferHolder command(wgpuCommandEncoderFinish(encoder.get(), &cmdBufferDescriptor), wgpuCommandBufferRelease);
        WGPUCommandBuffer commandPtr = command.get();

        wgpuQueueSubmit(queue.get(), 1, &commandPtr);
    }

    void renderModelType(WGPURenderPassEncoder render_pass, ModelType& model_data) {
        WGPUBuffer transform_buffer = model_data.transforms.getBuffer(transform_matrix_buffer);
        if (model_data.transforms.count() == 0) {
            return;
        }
        wgpuRenderPassEncoderSetVertexBuffer(render_pass, 3, transform_buffer, 0, model_data.transforms.size());
        wgpuRenderPassEncoderSetBindGroup(render_pass, 0, bind_group.get(), 0, 0);
        wgpuRenderPassEncoderSetBindGroup(render_pass, 1, model_data.model_bind_group.get(), 0, 0);
        for (auto& primitive : model_data.primitives) {
            wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, primitive.position_buffer.get(), 0, wgpuBufferGetSize(primitive.position_buffer.get()));
            wgpuRenderPassEncoderSetVertexBuffer(render_pass, 1, primitive.normal_buffer.get(), 0, wgpuBufferGetSize(primitive.normal_buffer.get()));
            wgpuRenderPassEncoderSetVertexBuffer(render_pass, 2, primitive.texcoord_buffer.get(), 0, wgpuBufferGetSize(primitive.texcoord_buffer.get()));
            wgpuRenderPassEncoderSetIndexBuffer(render_pass, primitive.index_buffer.get(), WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize(primitive.index_buffer.get()));
            wgpuRenderPassEncoderSetBindGroup(render_pass, 2, primitive.bind_group.get(), 0, 0);
            wgpuRenderPassEncoderDrawIndexed(render_pass, static_cast<uint32_t>(wgpuBufferGetSize(primitive.index_buffer.get()) / sizeof(uint16_t)), static_cast<uint32_t>(model_data.transforms.count()), 0, 0, 0);
        }
    }

    void presentFrame() {
        #if not defined(__EMSCRIPTEN__)
        wgpuSurfacePresent(surface.get());
        delete_ptr(_current_texture);
        #endif
    }

    WGPUTextureHolder makeTexture(const char* label, WGPUExtent3D size, const void* data) {
        WGPUTextureDescriptor texture_desc = {
            .nextInChain = nullptr,
            .label = label,
            .usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding,
            .dimension = WGPUTextureDimension_2D,
            .size = size,
            .format = WGPUTextureFormat_RGBA8Unorm,
            .mipLevelCount = 1,
            .sampleCount = 1,
            .viewFormatCount = 0,
            .viewFormats = nullptr,
        };
        WGPUTextureHolder texture = createTexture(device.get(), texture_desc);
        WGPUImageCopyTexture image_copy_texture = {
            .texture = texture.get(),
            .mipLevel = 0,
            .origin = {0, 0, 0},
            .aspect = WGPUTextureAspect_All,
        };
        WGPUTextureDataLayout texture_data_layout = {
            .offset = 0,
            .bytesPerRow = sizeof(float) * size.width,
            .rowsPerImage = size.height,
        };
        wgpuQueueWriteTexture(
            queue.get(),
            &image_copy_texture,
            data,
            4 * size.width * size.height,
            &texture_data_layout,
            &texture_desc.size);
        return texture;
    }

    WGPUTextureViewHolder makeTextureView(WGPUTexture texture) {
        WGPUTextureViewDescriptor texture_view_desc = {
            .nextInChain = nullptr,
            .label = "Default texture view",
            .format = WGPUTextureFormat_RGBA8Unorm,
            .dimension = WGPUTextureViewDimension_2D,
            .baseMipLevel = 0,
            .mipLevelCount = 1,
            .baseArrayLayer = 0,
            .arrayLayerCount = 1,
            .aspect = WGPUTextureAspect_All,
        };
        WGPUTextureView texture_view = wgpuTextureCreateView(texture, &texture_view_desc);
        return WGPUTextureViewHolder(texture_view, wgpuTextureViewRelease);
    }

    WGPUSamplerHolder makeSampler(const char* label) {
        WGPUSamplerDescriptor color_sampler_desc = {
            .label = label,
            .addressModeU = WGPUAddressMode_ClampToEdge,
            .addressModeV = WGPUAddressMode_ClampToEdge,
            .addressModeW = WGPUAddressMode_ClampToEdge,
            .magFilter = WGPUFilterMode_Linear,
            .minFilter = WGPUFilterMode_Linear,
            .mipmapFilter = WGPUMipmapFilterMode_Linear,
            .lodMinClamp = 0.f,
            .lodMaxClamp = 1.f,
            .compare = WGPUCompareFunction_Undefined,
            .maxAnisotropy = 1,
        };
        return createSampler(device.get(), color_sampler_desc);
    }

    WGPUBindGroupHolder createModelBindGroup(const char* label, WGPUBuffer modelBuffer) {
        std::array entries = {
            WGPUBindGroupEntry{
                .binding = 0,
                .buffer = modelBuffer,
                .offset = 0,
                .size = sizeof(ModelUniforms),
            }
        };
        WGPUBindGroupDescriptor bindGroupDesc = {
            .nextInChain = nullptr,
            .label = label,
            .layout = model_bind_group_layout.get(),
            .entryCount = static_cast<uint32_t>(entries.size()),
            .entries = entries.data(),
        };
        return createBindGroup(device.get(), bindGroupDesc);
    }

    WGPUBindGroupHolder createPrimitiveBindGroup(const char* label, WGPUBuffer primitiveBuffer, WGPUTextureView colorTextureView, WGPUSampler colorSampler) {
        std::array entries = {
            WGPUBindGroupEntry{
                .binding = 0,
                .buffer = primitiveBuffer,
                .offset = 0,
                .size = sizeof(PrimitiveUniforms),
            },
            WGPUBindGroupEntry{
                .binding = 1,
                .textureView = colorTextureView,
            },
            WGPUBindGroupEntry{
                .binding = 2,
                .sampler = colorSampler,
            },
        };
        WGPUBindGroupDescriptor bindGroupDesc = {
            .nextInChain = nullptr,
            .label = label,
            .layout = primitive_bind_group_layout.get(),
            .entryCount = static_cast<uint32_t>(entries.size()),
            .entries = entries.data(),
        };
        return createBindGroup(device.get(), bindGroupDesc);
    }

    WGPUBufferHolder createModelUniformBuffer(const char* label) {
        WGPUBufferDescriptor model_uniform_buffer_desc = {
            .nextInChain = nullptr,
            .label = label,
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
            .size = sizeof(ModelUniforms),
            .mappedAtCreation = false,
        };
        return createBuffer(device.get(), model_uniform_buffer_desc);
    }

    WGPUBufferHolder createPrimitiveUniformBuffer(const char* label) {
        WGPUBufferDescriptor model_uniform_buffer_desc = {
            .nextInChain = nullptr,
            .label = label,
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
            .size = sizeof(PrimitiveUniforms),
            .mappedAtCreation = false,
        };
        return createBuffer(device.get(), model_uniform_buffer_desc);
    }

    WGPUTextureHolder createDepthTexture() {
        WGPUTextureDescriptor depthTextureDescriptor = {
            .nextInChain = nullptr,
            .label = "Depth texture",
            .usage = WGPUTextureUsage_RenderAttachment,
            .dimension = WGPUTextureDimension_2D,
            .size = {
                .width = screen_size.width,
                .height = screen_size.height,
                .depthOrArrayLayers = 1,
            },
            .format = DEPTH_TEXTURE_FORMAT,
            .mipLevelCount = 1,
            .sampleCount = 1,
            .viewFormatCount = 1,
            .viewFormats = &DEPTH_TEXTURE_FORMAT,
        };
        WGPUTexture depthTexture = wgpuDeviceCreateTexture(device.get(), &depthTextureDescriptor);
        if (!depthTexture) {
            std::cerr << "Could not create depth texture!" << std::endl;
            exit(1);
        }
        std::cout << "Created depth texture: " << depthTexture << std::endl;
        return WGPUTextureHolder(depthTexture, wgpuTextureRelease);
    }

    Uniforms make_uniforms() {
        glm::vec3 camera_origin = {-2.f, 1.f, .5f};
        glm::mat4x4 view_translation = glm::translate(glm::mat4x4(1.0), -camera_origin);
        float angle2 = -20.f * static_cast<float>(M_PI) / 180.f;
        glm::mat4x4 view_rotation = glm::rotate(glm::mat4x4(1.0), -angle2, glm::vec3(0.0, 0.0, 1.0));

        glm::mat4x4 view_matrix = view_rotation * view_translation;

        float ratio = static_cast<float>(screen_size.width) / static_cast<float>(screen_size.height);
        float near_ = .01f;
        float far_ = 500.f;
        float fov = 45.f * static_cast<float>(M_PI) / 180.f;
        glm::mat4x4 projection_matrix = glm::perspective(fov, ratio, near_, far_);
        std::cout << "Projection matrix: " << glm::to_string(projection_matrix) << std::endl;
        std::cout << "View matrix: " << glm::to_string(view_matrix) << std::endl;

        return {
            .projection = projection_matrix,
            .view = view_matrix,
        };
    }

public:
    Rect screen_size = SCREEN_SIZE;

    class InstanceHandle {
        ModelHandle model;
        DynamicTransformBuffer::Index transform_index;

        friend class Renderer;
        InstanceHandle(ModelHandle model, DynamicTransformBuffer::Index transform_index) : model(model), transform_index(transform_index) {}

    public:
        bool operator==(const InstanceHandle& other) const {
            return model == other.model && transform_index == other.transform_index;
        }
    };

    Renderer(std::shared_ptr<GameConfig> game_config) : game_config(game_config) {
        screen_size.height = render_config.size.y;
        screen_size.width = render_config.size.x;

        WGPUSupportedLimits supported_limits{};
        supported_limits.nextInChain = nullptr;

        instance = getInstance();
        window = SDL_WindowHolder(initSDL(get_window_title(game_config.get()), render_config), SDL_DestroyWindow);
        surface = WGPUSurfaceHolder(SDL_GetWGPUSurface(instance.get(), window.get()), wgpuSurfaceRelease);
        adapter = getAdapter(instance.get(), surface.get());
        #ifdef __EMSCRIPTEN__
        // Error in Chrome so we hardcode values:
        // https://eliemichel.github.io/LearnWebGPU/appendices/building-for-the-web.html#get-limits
        supported_limits.limits.minStorageBufferOffsetAlignment = 256;
        supported_limits.limits.minUniformBufferOffsetAlignment = 256;
        #else
        wgpuAdapterGetLimits(adapter.get(), &supported_limits);
        #endif
        adapter_limits = supported_limits.limits;
        device = getDevice(adapter.get(), adapter_limits);

        surface_preferred_format = wgpuSurfaceGetPreferredFormat(surface.get(), adapter.get());

        wgpuDeviceGetLimits(device.get(), &supported_limits);
        device_limits = supported_limits.limits;
        queue = getQueue(device.get());
        #if defined(__EMSCRIPTEN__)
        swap_chain = getSwapChain(surface.get(), device.get(), surface_preferred_format, screen_size);
        #else
        WGPUSurfaceConfiguration surface_config = {
            .nextInChain = nullptr,
            .device = device.get(),
            .format = surface_preferred_format,
            .usage = WGPUTextureUsage_RenderAttachment,
            .viewFormatCount = 0,
            .viewFormats = nullptr,
            .alphaMode = WGPUCompositeAlphaMode_Auto,
            .width = screen_size.width,
            .height = screen_size.height,
            .presentMode = WGPUPresentMode_Fifo,
        };
        wgpuSurfaceConfigure(surface.get(), &surface_config);
        #endif
        shader_module = createShaderModule(device.get());
        bind_group_layout = createBindGroupLayout(device.get());
        model_bind_group_layout = createModelBindGroupLayout(device.get());
        primitive_bind_group_layout = createPrimitiveBindGroupLayout(device.get());
        pipeline_layout = createPipelineLayout<3>(device.get(), std::to_array({bind_group_layout.get(), model_bind_group_layout.get(), primitive_bind_group_layout.get()}));
        pipeline = createRenderPipeline(device.get(), pipeline_layout.get(), shader_module.get(), surface_preferred_format);
        depth_texture = createDepthTexture();
        depth_texture_view = createDepthTextureView(depth_texture.get());

        Uniforms uniforms = make_uniforms();
        WGPUBufferDescriptor uniform_buffer_desc = {
            .nextInChain = nullptr,
            .label = "Uniform buffer",
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
            .size = sizeof(uniforms),
            .mappedAtCreation = false,
        };
        uniform_buffer = createBuffer(device.get(), uniform_buffer_desc);
        wgpuQueueWriteBuffer(queue.get(), uniform_buffer.get(), 0, &uniforms, uniform_buffer_desc.size);
        bind_group = createUniformsBindGroup(device.get(), bind_group_layout.get(), uniform_buffer.get());

        default_model_uniform_buffer = createModelUniformBuffer("Default model uniform buffer");
        ModelUniforms default_model_uniform = {
            .transform = glm::mat4x4(1.f),
        };
        wgpuQueueWriteBuffer(queue.get(), default_model_uniform_buffer.get(), 0, &default_model_uniform, sizeof(ModelUniforms));
        default_model_bind_group = createModelBindGroup("Default model bind group", default_model_uniform_buffer.get());

        default_primitive_uniform_buffer = createPrimitiveUniformBuffer("Default primitive uniform buffer");
        PrimitiveUniforms default_primitive_uniform = {
            .transform = glm::mat4x4(1.f),
            .base_color = {1.f, 1.f, 1.f, 1.f},
            .has_texture = 0,
        };
        wgpuQueueWriteBuffer(queue.get(), default_primitive_uniform_buffer.get(), 0, &default_primitive_uniform, sizeof(PrimitiveUniforms));

        std::array<float, 16*16> default_image;
        for (int i = 0; i < 16*16; i++) {
            default_image[i] = 1.f;
        }
        default_texture = makeTexture("Default image", {16, 16, 1}, default_image.data());
        default_texture_view = makeTextureView(default_texture.get());
        default_sampler = makeSampler("Default sampler");
        std::cout << "Renderer initialized" << std::endl;
    }

    void resize() {
        #if defined(__EMSCRIPTEN__)
        swap_chain = getSwapChain(surface.get(), device.get(), surface_preferred_format, screen_size);
        #else
        int width, height;
        SDL_GetWindowSize(window.get(), &width, &height);
        screen_size.width = static_cast<uint32_t>(width);
        screen_size.height = static_cast<uint32_t>(height);

        WGPUSurfaceConfiguration surface_config = {
            .nextInChain = nullptr,
            .device = device.get(),
            .format = surface_preferred_format,
            .usage = WGPUTextureUsage_RenderAttachment,
            .viewFormatCount = 0,
            .viewFormats = nullptr,
            .alphaMode = WGPUCompositeAlphaMode_Auto,
            .width = screen_size.width,
            .height = screen_size.height,
            .presentMode = WGPUPresentMode_Fifo,
        };
        wgpuSurfaceConfigure(surface.get(), &surface_config);
        #endif
        depth_texture = createDepthTexture();
        depth_texture_view = createDepthTextureView(depth_texture.get());
        glm::mat4x4 projection_matrix = make_uniforms().projection;
        wgpuQueueWriteBuffer(queue.get(), uniform_buffer.get(), offsetof(Uniforms, projection), &projection_matrix, TRANSFORM_SIZE);
    }

    Transform& getCameraTransform() {
        return camera_transform;
    }

    const Transform& getCameraTransform() const {
        return camera_transform;
    }

    ModelHandle loadModel(std::string filename) {
        if (model_types.find(filename) != model_types.end()) {
            return model_types[filename];
        }

        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err, warn;
        bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
        if (!warn.empty()) {
            printf("Warn: %s\n", warn.c_str());
        }
        if (!err.empty()) {
            printf("Err: %s\n", err.c_str());
        }
        if (!ret) {
            printf("Failed to parse glTF\n");
            exit(1);
        }
        tinygltf::Scene& scene = model.scenes[model.defaultScene];
        if (scene.nodes.size() != 1) {
            std::cerr << "Model " << filename << " has " << scene.nodes.size()
                << " nodes in the default scene; only models with a single root node are supported" << std::endl;
            exit(1);
        }
        tinygltf::Node& main_node = model.nodes[scene.nodes[0]];
        std::cout << "Main node name: " << main_node.name << " (" << main_node.mesh << ')' << std::endl;
        tinygltf::Mesh& mesh = model.meshes[main_node.mesh];
        std::cout << "Main mesh name: " << mesh.name << std::endl;
        if (mesh.primitives.size() == 0) {
            std::cerr << "Model " << filename << " has no primitives" << std::endl;
            exit(1);
        }

        std::string label = std::string("Instance transform buffer for ") + filename;
        char* label_cstr = new char[label.size() + 1];
        strncpy(label_cstr, label.c_str(), label.size() + 1);
        std::cout << label_cstr << " " << label.size() << std::endl;
        WGPUBufferDescriptor bufferDesc = {
            .nextInChain = nullptr,
            .label = label_cstr,
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
            .size = 0,
            .mappedAtCreation = false,
        };

        WGPUBufferHolder model_uniform_buffer = {nullptr, voidDeleter};
        WGPUBindGroupHolder model_bind_group = {nullptr, voidDeleter};
        if (main_node.matrix.size() == 16) {
            ModelUniforms model_uniforms = {
                .transform = glm::make_mat4x4(main_node.matrix.data()),
            };
            label = "Model uniform buffer for " + filename;
            model_uniform_buffer = createModelUniformBuffer(label.c_str());
            wgpuQueueWriteBuffer(queue.get(), model_uniform_buffer.get(), 0, &model_uniforms, sizeof(ModelUniforms));
            label = "Model bind group for " + filename;
            model_bind_group = createModelBindGroup(label.c_str(), model_uniform_buffer.get());
        } else if (main_node.rotation.size() != 0 || main_node.scale.size() != 0 || main_node.translation.size() != 0) {
            Transform model_transform;
            if (main_node.rotation.size() == 4) {
                glm::quat rotation = glm::quat(main_node.rotation[3], main_node.rotation[0], main_node.rotation[1], main_node.rotation[2]);
                model_transform.rotation = glm::eulerAngles(rotation);
            }
            if (main_node.scale.size() == 3) {
                model_transform.scale = glm::make_vec3(main_node.scale.data());
            }
            if (main_node.translation.size() == 3) {
                model_transform.translation = glm::make_vec3(main_node.translation.data());
            }
            ModelUniforms model_uniform = {
                .transform = model_transform.toMatrix(),
            };
            label = "Model uniform buffer for " + filename;
            model_uniform_buffer = createModelUniformBuffer(label.c_str());
            wgpuQueueWriteBuffer(queue.get(), model_uniform_buffer.get(), 0, &model_uniform, sizeof(ModelUniforms));
            label = "Model bind group for " + filename;
            model_bind_group = createModelBindGroup(label.c_str(), model_uniform_buffer.get());
        } else {
            model_uniform_buffer = {default_model_uniform_buffer.get(), voidDeleter};
            model_bind_group = {default_model_bind_group.get(), voidDeleter};
        }

        ModelType model_type = {
            .primitives = {},
            .transforms = DynamicTransformBuffer(device.get(), queue.get(), bufferDesc),
            .model_uniform_buffer = std::move(model_uniform_buffer),
            .model_bind_group = std::move(model_bind_group),
        };

        for (int i = 0; i < mesh.primitives.size(); i++)  {
            auto& primitive = mesh.primitives[i];
            if (primitive.indices == -1) {
                std::cerr << "Model " << filename << ", primitive " << i << " has no indices; only indexed models are supported" << std::endl;
                exit(1);
            }
            if (primitive.attributes.find("POSITION") == primitive.attributes.end()) {
                std::cerr << "Model " << filename << ", primitive " << i << " has no position attribute" << std::endl;
                exit(1);
            }
            if (primitive.attributes.find("NORMAL") == primitive.attributes.end()) {
                std::cerr << "Model " << filename << ", primitive " << i << " has no normal attribute; only models with explicit normals are supported" << std::endl;
                exit(1);
            }
            tinygltf::Accessor& positionAccessor = model.accessors[primitive.attributes["POSITION"]];
            tinygltf::Accessor& normalAccessor = model.accessors[primitive.attributes["NORMAL"]];
            tinygltf::Accessor& indicesAccessor = model.accessors[primitive.indices];
            tinygltf::BufferView& positionBufferView = model.bufferViews[positionAccessor.bufferView];
            tinygltf::BufferView& normalBufferView = model.bufferViews[normalAccessor.bufferView];
            tinygltf::BufferView& indicesBufferView = model.bufferViews[indicesAccessor.bufferView];
            tinygltf::Buffer& modelPositionBuffer = model.buffers[positionBufferView.buffer];
            tinygltf::Buffer& modelNormalBuffer = model.buffers[normalBufferView.buffer];
            tinygltf::Buffer& modelIndicesBuffer = model.buffers[indicesBufferView.buffer];
            assert(positionBufferView.byteStride == 12 || positionBufferView.byteStride == 0);
            assert(positionAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
            assert(positionAccessor.type == TINYGLTF_TYPE_VEC3);
            assert(normalBufferView.byteStride == 12 || normalBufferView.byteStride == 0);
            assert(normalAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
            assert(normalAccessor.type == TINYGLTF_TYPE_VEC3);
            assert(indicesBufferView.byteStride == 0);
            assert(indicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
            assert(indicesAccessor.type == TINYGLTF_TYPE_SCALAR);

            label = std::string("Position buffer for ") + filename;
            WGPUBufferDescriptor bufferDesc = {
                .nextInChain = nullptr,
                .label = label.c_str(),
                .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
                .size = positionBufferView.byteLength,
                .mappedAtCreation = false,
            };
            WGPUBufferHolder positionBuffer = createBuffer(device.get(), bufferDesc);
            wgpuQueueWriteBuffer(queue.get(), positionBuffer.get(), 0, modelPositionBuffer.data.data() + positionBufferView.byteOffset, bufferDesc.size);

            label = std::string("Normal buffer for ") + filename;
            bufferDesc.label = label.c_str();
            bufferDesc.size = normalBufferView.byteLength;
            WGPUBufferHolder normalBuffer = createBuffer(device.get(), bufferDesc);
            wgpuQueueWriteBuffer(queue.get(), normalBuffer.get(), 0, modelNormalBuffer.data.data() + normalBufferView.byteOffset, bufferDesc.size);

            // align indices buffer size to COPY_BUFFER_ALIGNMENT
            uint32_t actual_byte_length = (indicesBufferView.byteLength + COPY_BUFFER_ALIGNMENT - 1) & ~(COPY_BUFFER_ALIGNMENT - 1);

            label = std::string("Indices buffer for ") + filename;
            bufferDesc.label = label.c_str();
            bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
            bufferDesc.size = actual_byte_length;
            WGPUBufferHolder indicesBuffer = createBuffer(device.get(), bufferDesc);
            std::cout << bufferDesc.size << std::endl;
            wgpuQueueWriteBuffer(queue.get(), indicesBuffer.get(), 0, modelIndicesBuffer.data.data() + indicesBufferView.byteOffset, bufferDesc.size);

            // if no texcoords are present, position buffer is arbitrarily used since it is at least as big as the texcoord buffer would be
            WGPUBufferHolder texcoord_buffer = {positionBuffer.get(), voidDeleter};

            glm::vec4 base_color = {1.f, 1.f, 1.f, 1.f};
            WGPUBufferHolder uniform_buffer = {default_primitive_uniform_buffer.get(), voidDeleter};
            WGPUTextureHolder color = {default_texture.get(), voidDeleter}; // use voidDeleter to not delete the default texture
            WGPUTextureViewHolder color_view = {default_texture_view.get(), voidDeleter};
            WGPUSamplerHolder color_sampler = {default_sampler.get(), voidDeleter};
            if (primitive.material != -1) {
                tinygltf::Material& material = model.materials[primitive.material];
                const auto& base_color_factor = material.pbrMetallicRoughness.baseColorFactor;
                base_color = {base_color_factor[0], base_color_factor[1], base_color_factor[2], base_color_factor[3]};
                tinygltf::TextureInfo colorInfo = material.pbrMetallicRoughness.baseColorTexture;
                bool has_texture = colorInfo.index != -1;
                bool has_texcoords = primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end();
                if (has_texture != has_texcoords) {
                    std::cerr << "Model " << filename << " has " << (has_texture ? "a" : "no")
                        << " texture but " << (has_texcoords ? "a" : "no") << " texcoord attribute" << std::endl;
                    exit(1);
                }

                if (has_texture) {
                    tinygltf::Texture& colorTexture = model.textures[colorInfo.index];
                    tinygltf::Image& image = model.images[colorTexture.source];
                    if (image.width > MAX_TEXTURE_SIZE || image.height > MAX_TEXTURE_SIZE) {
                        std::cerr << "Texture " << image.width << "x" << image.height
                            << " exceeds size limit of "
                            << MAX_TEXTURE_SIZE << "x" << MAX_TEXTURE_SIZE << std::endl;
                        exit(1);
                    }
                    label = "Color texture for " + filename;
                    if (image.name != "") {
                        label += " (" + image.name + ")";
                    }
                    color = makeTexture(
                        label.c_str(),
                        {static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height), 1},
                        image.image.data());
                    color_view = makeTextureView(color.get());

                    label = "Color sampler for " + filename;
                    // TODO: use the texture's sampler
                    color_sampler = makeSampler(label.c_str());

                    tinygltf::Accessor& texcoordAccessor = model.accessors[primitive.attributes["TEXCOORD_0"]];
                    tinygltf::BufferView& texcoordBufferView = model.bufferViews[texcoordAccessor.bufferView];
                    tinygltf::Buffer& modelTexcoordBuffer = model.buffers[texcoordBufferView.buffer];
                    assert(texcoordBufferView.byteStride == 8 || texcoordBufferView.byteStride == 0);
                    assert(texcoordAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
                    assert(texcoordAccessor.type == TINYGLTF_TYPE_VEC2);
                    label = std::string("Texcoord buffer for ") + filename;
                    bufferDesc.label = label.c_str();
                    bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
                    bufferDesc.size = texcoordBufferView.byteLength;
                    texcoord_buffer = createBuffer(device.get(), bufferDesc);
                    wgpuQueueWriteBuffer(queue.get(), texcoord_buffer.get(), 0, modelTexcoordBuffer.data.data() + texcoordBufferView.byteOffset, bufferDesc.size);
                }

                if (base_color != glm::vec4(1.f) || has_texture) {
                    label = "Uniform buffer for " + filename;
                    uniform_buffer = createPrimitiveUniformBuffer(label.c_str());
                    PrimitiveUniforms model_uniform = {
                        .transform = glm::mat4x4(1.f),
                        .base_color = base_color,
                        .has_texture = static_cast<uint32_t>(has_texture ? 1 : 0),
                    };
                    wgpuQueueWriteBuffer(queue.get(), uniform_buffer.get(), 0, &model_uniform, sizeof(PrimitiveUniforms));
                }
            }

            label = std::string("Bind group for ") + filename;
            WGPUBindGroupHolder bind_group = createPrimitiveBindGroup(label.c_str(), uniform_buffer.get(), color_view.get(), color_sampler.get());

            model_type.primitives.push_back(ModelPrimitive {
                .position_buffer = std::move(positionBuffer),
                .normal_buffer = std::move(normalBuffer),
                .index_buffer = std::move(indicesBuffer),
                .texcoord_buffer = std::move(texcoord_buffer),
                .uniform_buffer = std::move(uniform_buffer),
                .texture = std::move(color),
                .texture_view = std::move(color_view),
                .sampler = std::move(color_sampler),
                .bind_group = std::move(bind_group),
            });
            std::cout << "Loaded primitive " << i << std::endl;
        }

        models.push_back(std::move(model_type));
        ModelHandle handle = ModelHandle {models.size() - 1};
        model_types[filename] = handle;
        std::cout << "Loaded model " << filename << std::endl;
        return handle;
    }

    void renderPresentFrame() {
        WGPUTextureViewHolder texture_view = getCurrentTexture();
        renderFrame(texture_view.get());
        presentFrame();
    }

    InstanceHandle spawnInstance(ModelHandle model, Transform transform) {
        ModelType& model_data = models[model.index];
        DynamicTransformBuffer::Index transform_index = model_data.transforms.add(transform);
        return InstanceHandle {model, transform_index};
    }

    Transform& getModelInstance(InstanceHandle instance) {
        ModelType& model_data = models[instance.model.index];
        return model_data.transforms[instance.transform_index];
    }

    const Transform& getModelInstance(InstanceHandle instance) const {
        const ModelType& model_data = models[instance.model.index];
        return model_data.transforms[instance.transform_index];
    }

    void destroyInstance(InstanceHandle instance) {
        ModelType& model_data = models[instance.model.index];
        model_data.transforms.remove(instance.transform_index);
    }
};
