#include "headers.h"

#include "arena.cpp"

#include "context.cpp"
#include "mesh.cpp"
#include "scene.cpp"
#include "pipeline.cpp"
#include "surface.cpp"
//
#include "render.cpp"
#include "render2.cpp"
//
// #include <windows.h>

int g_debug_enabled = 0;

int main(int argc, char **argv)
{

  // char cwd[256];
  // GetCurrentDirectoryA(sizeof(cwd), cwd);
  // printf("working directory is %s\n", cwd);
  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-d") == 0)
    {
      g_debug_enabled = 1;
    }
  }
  State state = {};
  state.scratch_arena = ArenaInit(malloc(megabytes(128)), megabytes(8));
  state.permanent_arena = ArenaInit(malloc(megabytes(64)), megabytes(16));
  state.swapchain_arena = ArenaInit(malloc(megabytes(64)), megabytes(16));
  // create context
  state.context = (Context *)ArenaPush(&state.permanent_arena, sizeof(Context));
  CreateVulkanContext(&state);
  //  load swapchain
  state.swapchain =
    (Swapchain *)ArenaPush(&state.swapchain_arena, sizeof(Swapchain));
  CreateVulkanSwapchain(&state, state.swapchain->handle);
  // load meshes upfront
  const char *mesh_paths[] = {
    "assets/Cube.glb",   "assets/Cone.glb",   "assets/Cylinder.glb",
    "assets/Sphere.glb", "assets/Skull2.glb",
  };
  int num_paths = sizeof(mesh_paths) / sizeof(mesh_paths[0]);
  CreateMegaBuffer(&state, mesh_paths, num_paths);
  CreateScene(&state);
  CreatePipeline(&state);
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
    // RenderLoop2(&state, frame_index);
    frame_index = (frame_index + 1) % FRAMES_IN_FLIGHT;
  }
  return 0;
}
