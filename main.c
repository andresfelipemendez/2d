#include <stdio.h>
#include <stdbool.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>


int main( int argc, char* args[] ){
    if(!SDL_Init(SDL_INIT_VIDEO)){
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "test",
        800,600,
        SDL_WINDOW_RESIZABLE
    );

    if(!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event event;

    printf("Window created successfully!\n");
    printf("Press ESC or close button to exit.\n");

    while(running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                printf("Window resized to %dx%x\n",
                    event.window.data1,event.window.data2);
                break;
            default:
                break;
            }
        }

        SDL_SetRenderDrawColor(renderer, 70,130,180,255);
        SDL_RenderClear(renderer);

        SDL_FRect rect = {
            .x = 350.0f,
            .y = 250.0f,
            .w = 100.0f,
            .h = 100.0f
        };


        SDL_SetRenderDrawColor(renderer, 255,255,255,255);
        SDL_RenderFillRect(renderer,&rect);

        SDL_RenderPresent(renderer);

        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}