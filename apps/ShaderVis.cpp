#include "SDL.h"
#include "SDL_syswm.h"
#include "AGPU/agpu.hpp"
#include <stdio.h>
#include <vector>
#include <string>

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

        window = SDL_CreateWindow("ShaderVis", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screenWidth, screenHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
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
        currentSwapChainCreateInfo.width = screenWidth;
        currentSwapChainCreateInfo.height = screenHeight;
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

        screenWidth = swapChain->getWidth();
        screenHeight = swapChain->getHeight();

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

        screenWidth = swapChain->getWidth();
        screenHeight = swapChain->getHeight();
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

    void updateAndRender(float delta)
    {
        // Build the command list
        commandAllocator->reset();
        commandList->reset(commandAllocator, nullptr);

        auto backBuffer = swapChain->getCurrentBackBuffer();

        commandList->beginRenderPass(mainRenderPass, backBuffer, false);

        commandList->setViewport(0, 0, screenWidth, screenHeight);
        commandList->setScissor(0, 0, screenWidth, screenHeight);

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
    int screenWidth = 640;
    int screenHeight = 480;
    bool isQuitting = false;

    agpu_device_ref device;
    agpu_command_queue_ref commandQueue;
    agpu_renderpass_ref mainRenderPass;
    agpu_shader_signature_ref shaderSignature;
    agpu_command_allocator_ref commandAllocator;
    agpu_command_list_ref commandList;
    agpu_swap_chain_create_info currentSwapChainCreateInfo;
    agpu_swap_chain_ref swapChain;
};

int main(int argc, const char *argv[])
{
    return ShaderVis().main(argc, argv);
}
