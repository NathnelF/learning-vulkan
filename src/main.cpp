#include "headers.h"

#include "arena.cpp"

#include "context.cpp"
#include "surface.cpp"
//
#include "render.cpp"
#include <SDL3/SDL_events.h>
//

int g_debug_enabled = 0;

int main(int argc, char **argv)
{

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-d") == 0)
        {
            g_debug_enabled = 1;
        }
    }
    State state = {};
    state.scratch_arena = ArenaInit(malloc(megabytes(8)), megabytes(8));
    state.permanent_arena = ArenaInit(malloc(megabytes(16)), megabytes(16));
    state.swapchain_arena = ArenaInit(malloc(megabytes(16)), megabytes(16));
    // create context
    state.context = (Context *)ArenaPush(&state.permanent_arena, sizeof(Context));
    CreateVulkanContext(&state);
    //  load swapchain
    state.swapchain = (Swapchain *)ArenaPush(&state.swapchain_arena, sizeof(Swapchain));
    CreateVulkanSwapchain(&state, state.swapchain->handle);
    // TODO(Nate): load data
    // TODO(Nate): create pipeline
    int running = 1;
    int frame_index = 0;
    SDL_Event event;
    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                debug("Window quit");
                running = 0;
            }
            if (event.type == SDL_EVENT_WINDOW_RESIZED)
            {
                debug("resize!");
                state.resize_ticker = 10;
                RecreateVulkanSwapchain(&state);
            }
        }
        // if (state.resize_ticker > 0)
        // {
        //     state.resize_ticker--;
        //     if (state.resize_ticker == 0)
        //     {
        //         RecreateVulkanSwapchain(&state);
        //     }
        // }
        RenderLoop(&state, frame_index);
        frame_index = (frame_index + 1) % FRAMES_IN_FLIGHT;
    }
    return 0;
}
