#include "SDL.h"
#include "SDL_syswm.h"
#include "AGPU/agpu.hpp"
#include <stdio.h>
#include <memory>
#include <vector>
#include <string>

struct ScreenAndUIState
{
    int screenWidth = 640;
    int screenHeight = 480;
    bool flipVertically = false;
    float screenScale = 10.0f;
    float screenOffsetX = 0.0f;
    float screenOffsetY = 0.0f;
};

class ShaderVis
{
public:
    ShaderVis() = default;
    ~ShaderVis() = default;

    int main(int argc, const char *argv[])
    {
        bool vsyncDisabled = false;
        bool debugLayerEnabled = false;
    #ifdef _DEBUG
        debugLayerEnabled= true;
    #endif
        agpu_uint platformIndex = 0;
        agpu_uint gpuIndex = 0;
        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];
            if (arg == "-no-vsync")
            {
                vsyncDisabled = true;
            }
            else if (arg == "-platform")
            {
                platformIndex = agpu_uint(atoi(argv[++i]));
            }
            else if (arg == "-gpu")
            {
                gpuIndex = agpu_uint(atoi(argv[++i]));
            }
            else if (arg == "-debug")
            {
                debugLayerEnabled = true;
            }
        }

        // Get the platform.
        agpu_uint numPlatforms;
        agpuGetPlatforms(0, nullptr, &numPlatforms);
        if (numPlatforms == 0)
        {
            fprintf(stderr, "No agpu platforms are available.\n");
            return 1;
        }
        else if (platformIndex >= numPlatforms)
        {
            fprintf(stderr, "Selected platform index is not available.\n");
            return 1;
        }

        std::vector<agpu_platform*> platforms;
        platforms.resize(numPlatforms);
        agpuGetPlatforms(numPlatforms, &platforms[0], nullptr);
        auto platform = platforms[platformIndex];

        printf("Choosen platform: %s\n", agpuGetPlatformName(platform));

        SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
        SDL_Init(SDL_INIT_VIDEO);

        window = SDL_CreateWindow("ShaderVis", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screenAndUIState.screenWidth, screenAndUIState.screenHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        if(!window)
        {
            fprintf(stderr, "Failed to create window.\n");
            return 1;
        }

        // Get the window info.
        SDL_SysWMinfo windowInfo;
        SDL_VERSION(&windowInfo.version);
        SDL_GetWindowWMInfo(window, &windowInfo);

        // Open the device
        agpu_device_open_info openInfo = {};
        openInfo.gpu_index = gpuIndex;
        openInfo.debug_layer = debugLayerEnabled;
        memset(&currentSwapChainCreateInfo, 0, sizeof(currentSwapChainCreateInfo));
        switch(windowInfo.subsystem)
        {
    #if defined(SDL_VIDEO_DRIVER_WINDOWS)
        case SDL_SYSWM_WINDOWS:
            currentSwapChainCreateInfo.window = (agpu_pointer)windowInfo.info.win.window;
            break;
    #endif
    #if defined(SDL_VIDEO_DRIVER_X11)
        case SDL_SYSWM_X11:
            openInfo.display = (agpu_pointer)windowInfo.info.x11.display;
            currentSwapChainCreateInfo.window = (agpu_pointer)(uintptr_t)windowInfo.info.x11.window;
            break;
    #endif
    #if defined(SDL_VIDEO_DRIVER_COCOA)
        case SDL_SYSWM_COCOA:
            currentSwapChainCreateInfo.window = (agpu_pointer)windowInfo.info.cocoa.window;
            break;
    #endif
        default:
            fprintf(stderr, "Unsupported window system\n");
            return -1;
        }

        currentSwapChainCreateInfo.colorbuffer_format = AGPU_TEXTURE_FORMAT_B8G8R8A8_UNORM_SRGB;
        currentSwapChainCreateInfo.width = screenAndUIState.screenWidth;
        currentSwapChainCreateInfo.height = screenAndUIState.screenHeight;
        currentSwapChainCreateInfo.buffer_count = 3;
        currentSwapChainCreateInfo.flags = AGPU_SWAP_CHAIN_FLAG_APPLY_SCALE_FACTOR_FOR_HI_DPI;
        if (vsyncDisabled)
        {
            currentSwapChainCreateInfo.presentation_mode = AGPU_SWAP_CHAIN_PRESENTATION_MODE_MAILBOX;
            currentSwapChainCreateInfo.fallback_presentation_mode = AGPU_SWAP_CHAIN_PRESENTATION_MODE_IMMEDIATE;
        }

        device = platform->openDevice(&openInfo);
        if(!device)
        {
            fprintf(stderr, "Failed to open the device\n");
            return false;
        }

        // Get the default command queue
        commandQueue = device->getDefaultCommandQueue();

        // Create the swap chain.
        swapChain = device->createSwapChain(commandQueue, &currentSwapChainCreateInfo);
        if(!swapChain)
        {
            fprintf(stderr, "Failed to create the swap chain\n");
            return false;
        }

        screenAndUIState.screenWidth = swapChain->getWidth();
        screenAndUIState.screenHeight = swapChain->getHeight();

        // Create the render pass
        {
            agpu_renderpass_color_attachment_description colorAttachment = {};
            colorAttachment.format = AGPU_TEXTURE_FORMAT_B8G8R8A8_UNORM_SRGB;
            colorAttachment.begin_action = AGPU_ATTACHMENT_CLEAR;
            colorAttachment.end_action = AGPU_ATTACHMENT_KEEP;
            colorAttachment.clear_value.r = 0;
            colorAttachment.clear_value.g = 0;
            colorAttachment.clear_value.b = 0;
            colorAttachment.clear_value.a = 0;
            colorAttachment.sample_count = 1;

            agpu_renderpass_description description = {};
            description.color_attachment_count = 1;
            description.color_attachments = &colorAttachment;

            mainRenderPass = device->createRenderPass(&description);
        }

        // Create the shader signature
        {

            auto builder = device->createShaderSignatureBuilder();
            // Sampler
            builder->beginBindingBank(1);
            builder->addBindingBankElement(AGPU_SHADER_BINDING_TYPE_SAMPLER, 1);

            builder->beginBindingBank(1);
            builder->addBindingBankElement(AGPU_SHADER_BINDING_TYPE_UNIFORM_BUFFER, 1); // Screen and UI state
            builder->addBindingBankElement(AGPU_SHADER_BINDING_TYPE_STORAGE_BUFFER, 1); // UI Data
            builder->addBindingBankElement(AGPU_SHADER_BINDING_TYPE_SAMPLED_IMAGE, 1); // Bitmap font

            shaderSignature = builder->build();
            if(!shaderSignature)
                return 1;
        }

        // Samplers binding
        {
            agpu_sampler_description samplerDesc = {};
            samplerDesc.address_u = AGPU_TEXTURE_ADDRESS_MODE_CLAMP;
            samplerDesc.address_v = AGPU_TEXTURE_ADDRESS_MODE_CLAMP;
            samplerDesc.address_w = AGPU_TEXTURE_ADDRESS_MODE_CLAMP;
            samplerDesc.filter = AGPU_FILTER_MIN_LINEAR_MAG_LINEAR_MIPMAP_NEAREST;
            sampler = device->createSampler(&samplerDesc);
            if(!sampler)
            {
                fprintf(stderr, "Failed to create the sampler.\n");
                return 1;
            }

            samplersBinding = shaderSignature->createShaderResourceBinding(0);
            samplersBinding->bindSampler(0, sampler);
        }

        // Screen and UI State buffer
        {
            agpu_buffer_description desc = {};
            desc.size = (sizeof(ScreenAndUIState) + 255) & (-256);
            desc.heap_type = AGPU_MEMORY_HEAP_TYPE_HOST_TO_DEVICE;
            desc.usage_modes = agpu_buffer_usage_mask(AGPU_COPY_DESTINATION_BUFFER | AGPU_UNIFORM_BUFFER);
            desc.main_usage_mode = AGPU_UNIFORM_BUFFER;
	        desc.mapping_flags = AGPU_MAP_DYNAMIC_STORAGE_BIT;
            screenAndUIStateUniformBuffer = device->createBuffer(&desc, nullptr);
        }

        // Data binding
        dataBinding = shaderSignature->createShaderResourceBinding(1);
        dataBinding->bindUniformBuffer(0, screenAndUIStateUniformBuffer);

        // Screen quad pipeline state.
        screenQuadVertex = compileShaderWithSourceFile("assets/shaders/screenQuad.glsl", AGPU_VERTEX_SHADER);
        screenQuadFragment = compileShaderWithSourceFile("assets/shaders/voronoiNoise.glsl", AGPU_FRAGMENT_SHADER);
        screenAndUIState.flipVertically = device->hasTopLeftNdcOrigin() == device->hasBottomLeftTextureCoordinates();

        if(!screenQuadVertex || !screenQuadFragment)
            return 1;

        {
            auto builder = device->createPipelineBuilder();
            builder->setRenderTargetFormat(0, AGPU_TEXTURE_FORMAT_B8G8R8A8_UNORM_SRGB);
            builder->setShaderSignature(shaderSignature);
            builder->attachShader(screenQuadVertex);
            builder->attachShader(screenQuadFragment);
            builder->setPrimitiveType(AGPU_TRIANGLES);
            screenQuadPipeline = finishBuildingPipeline(builder);
        }

        // Create the command allocator and command list
        commandAllocator = device->createCommandAllocator(AGPU_COMMAND_LIST_TYPE_DIRECT, commandQueue);
        commandList = device->createCommandList(AGPU_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr);
        commandList->close();

        // Main loop
        auto oldTime = SDL_GetTicks();
        while(!isQuitting)
        {
            auto newTime = SDL_GetTicks();
            auto deltaTime = newTime - oldTime;
            oldTime = newTime;

            processEvents();
            updateAndRender(deltaTime * 0.001f);
        }

        commandQueue->finishExecution();
        swapChain.reset();
        commandQueue.reset();

        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    }

    std::string readWholeFile(const std::string &fileName)
    {
        FILE *file = fopen(fileName.c_str(), "rb");
        if(!file)
        {
            fprintf(stderr, "Failed to open file %s\n", fileName.c_str());
            return std::string();
        }

        // Allocate the data.
        std::vector<char> data;
        fseek(file, 0, SEEK_END);
        data.resize(ftell(file));
        fseek(file, 0, SEEK_SET);

        // Read the file
        if(fread(&data[0], data.size(), 1, file) != 1)
        {
            fprintf(stderr, "Failed to read file %s\n", fileName.c_str());
            fclose(file);
            return std::string();
        }

        fclose(file);
        return std::string(data.begin(), data.end());
    }

    agpu_shader_ref compileShaderWithSourceFile(const std::string &sourceFileName, agpu_shader_type type)
    {
        return compileShaderWithSource(sourceFileName, readWholeFile(sourceFileName), type);
    }

    agpu_shader_ref compileShaderWithSource(const std::string &name, const std::string &source, agpu_shader_type type)
    {
        if(source.empty())
            return nullptr;

        // Create the shader compiler.
        agpu_offline_shader_compiler_ref shaderCompiler = device->createOfflineShaderCompiler();
        shaderCompiler->setShaderSource(AGPU_SHADER_LANGUAGE_VGLSL, type, source.c_str(), (agpu_string_length)source.size());
        auto errorCode = agpuCompileOfflineShader(shaderCompiler.get(), AGPU_SHADER_LANGUAGE_DEVICE_SHADER, nullptr);
        if(errorCode)
        {
            auto logLength = shaderCompiler->getCompilationLogLength();
            std::unique_ptr<char[]> logBuffer(new char[logLength+1]);
            shaderCompiler->getCompilationLog(logLength+1, logBuffer.get());
            fprintf(stderr, "Compilation error of '%s':%s\n", name.c_str(), logBuffer.get());
            return nullptr;
        }

        // Create the shader and compile it.
        return shaderCompiler->getResultAsShader();
    }

    agpu_pipeline_state_ref finishBuildingPipeline(const agpu_pipeline_builder_ref &builder)
    {
        auto pipeline = builder->build();
        if(!pipeline)
        {
            fprintf(stderr, "Failed to build pipeline.\n");
        }
        return pipeline;
    }

    void processEvents()
    {
        SDL_Event event;
        while(SDL_PollEvent(&event))
            processEvent(event);
    }

    void processEvent(const SDL_Event &event)
    {
        switch(event.type)
        {
        case SDL_QUIT:
            isQuitting = true;
            break;
        case SDL_KEYDOWN:
            onKeyDown(event.key);
            break;
        case SDL_MOUSEMOTION:
            onMouseMotion(event.motion);
            break;
        case SDL_MOUSEWHEEL:
            onMouseWheel(event.wheel);
            break;
        case SDL_WINDOWEVENT:
            {
                switch(event.window.event)
                {
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    recreateSwapChain();
                    break;
                default:
                    break;
                }
            }
            break;
        default:
            break;
        }
    }

    void recreateSwapChain()
    {
        int w, h;
        SDL_GetWindowSize(window, &w, &h);

        device->finishExecution();
        auto newSwapChainCreateInfo = currentSwapChainCreateInfo;
        newSwapChainCreateInfo.width = w;
        newSwapChainCreateInfo.height = h;
        newSwapChainCreateInfo.old_swap_chain = swapChain.get();
        swapChain = device->createSwapChain(commandQueue, &newSwapChainCreateInfo);

        screenAndUIState.screenWidth = swapChain->getWidth();
        screenAndUIState.screenHeight = swapChain->getHeight();
        if(swapChain)
            currentSwapChainCreateInfo = newSwapChainCreateInfo;
    }

    void onKeyDown(const SDL_KeyboardEvent &event)
    {
        switch(event.keysym.sym)
        {
        case SDLK_ESCAPE:
            isQuitting = true;
            break;
        default:
            break;
        }
    }

    void onMouseMotion(const SDL_MouseMotionEvent &event)
    {
        if(event.state & SDL_BUTTON_LMASK)
        {
            float scaleFactor = screenAndUIState.screenScale *0.001f;
            screenAndUIState.screenOffsetX += event.xrel*scaleFactor;
            screenAndUIState.screenOffsetY -= event.yrel*scaleFactor;
        }
    }

    void onMouseWheel(const SDL_MouseWheelEvent &event)
    {
        if(event.y > 0)
            screenAndUIState.screenScale /= 1.1;
        else if(event.y < 0)
            screenAndUIState.screenScale *= 1.1;
    }

    void updateAndRender(float delta)
    {
        // Upload the data buffers.
        screenAndUIStateUniformBuffer->uploadBufferData(0, sizeof(screenAndUIState), &screenAndUIState);

        // Build the command list
        commandAllocator->reset();
        commandList->reset(commandAllocator, nullptr);

        auto backBuffer = swapChain->getCurrentBackBuffer();

        commandList->setShaderSignature(shaderSignature);
        commandList->beginRenderPass(mainRenderPass, backBuffer, false);

        commandList->setViewport(0, 0, screenAndUIState.screenWidth, screenAndUIState.screenHeight);
        commandList->setScissor(0, 0, screenAndUIState.screenWidth, screenAndUIState.screenHeight);

        // Draw the screen quad.
        commandList->usePipelineState(screenQuadPipeline);
        commandList->useShaderResources(samplersBinding);
        commandList->useShaderResources(dataBinding);

        // Draw the objects
        commandList->drawArrays(3, 1, 0, 0);

        // Finish the command list
        commandList->endRenderPass();
        commandList->close();

        // Queue the command list
        commandQueue->addCommandList(commandList);

        swapBuffers();
        commandQueue->finishExecution();
    }

    void swapBuffers()
    {
        auto errorCode = agpuSwapBuffers(swapChain.get());
        if(!errorCode)
            return;

        if(errorCode == AGPU_OUT_OF_DATE)
            recreateSwapChain();
    }

    SDL_Window *window = nullptr;
    bool isQuitting = false;

    agpu_device_ref device;
    agpu_command_queue_ref commandQueue;
    agpu_renderpass_ref mainRenderPass;
    agpu_shader_signature_ref shaderSignature;
    agpu_command_allocator_ref commandAllocator;
    agpu_command_list_ref commandList;
    agpu_swap_chain_create_info currentSwapChainCreateInfo;
    agpu_swap_chain_ref swapChain;

    agpu_shader_ref screenQuadVertex;
    agpu_shader_ref screenQuadFragment;
    agpu_pipeline_state_ref screenQuadPipeline;

    agpu_sampler_ref sampler;
    agpu_shader_resource_binding_ref samplersBinding;

    agpu_buffer_ref screenAndUIStateUniformBuffer;
    agpu_buffer_ref uiDataBuffer;
    agpu_texture_ref bitmapFont;
    agpu_shader_resource_binding_ref dataBinding;

    ScreenAndUIState screenAndUIState;
};

int main(int argc, const char *argv[])
{
    return ShaderVis().main(argc, argv);
}
