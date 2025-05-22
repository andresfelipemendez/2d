#include <SDL3/SDL.h>
#include <glad.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

typedef struct {
    void* engine_data;

    bool (*init)(int width,int height);
    void (*update)(float delta_time);
    void (*render)(void);
    void (*cleanup)(void);
    bool (*handle_event)(SDL_Event* event);

    void (*on_reload)(void* old_data);
    void* (*get_state)(void);
} EngineAPI;

typedef struct {
    void* lib_handle;
    EngineAPI api;
    time_t last_mod_time;
    char lib_path[256];
    char temp_lib_path[256];
    int reload_counter;
} HotReloadManager;

static HotReloadManager g_reload_manager = {0};

bool copy_library(const char* src, const char* dst) {
    FILE* src_file = fopen(src,"rb");
    if(!src_file) return false;

    FILE* dst_file = fopen(dst,"wb");
    if(!dst_file) {
        fclose(src_file);
        return false;
    }

    char buffer[4096];
    size_t bytes;
    while((bytes = fread(buffer,1,sizeof(buffer),src_file))>0){
        fwrite(buffer,1,bytes,dst_file);
    }
    fclose(src_file);
    fclose(dst_file);
    return true;
}

bool load_engine_library(HotReloadManager* manager, bool is_reload) {
    void* old_data = NULL;
    
    // If reloading, get the current state
    if (is_reload && manager->api.get_state) {
        old_data = manager->api.get_state();
        
        // Cleanup current instance
        if (manager->api.cleanup) {
            manager->api.cleanup();
        }
        
        // Close current library
        if (manager->lib_handle) {
            dlclose(manager->lib_handle);
            manager->lib_handle = NULL;
        }
    }

    if(is_reload) {
        dlclose(manager->lib_handle);
        manager->lib_handle = NULL;
    }

    snprintf(manager->temp_lib_path, sizeof(manager->temp_lib_path),"/tmp/engine_temp_%d.so",manager->reload_counter);
    if(!copy_library(manager->lib_path,manager->temp_lib_path)) {
        printf("failed to copy library for hot reload\n");
        return false;
    }

    manager->lib_handle = dlopen(manager->temp_lib_path,RTLD_NOW);
    if(!manager->lib_handle) {
        printf("failed to load engine library: %s\n", dlerror());
        return false;
    } else {
        //clear errors
        dlerror();
    }

    manager->api.init = dlsym(manager->lib_handle, "engine_init");
    manager->api.update = dlsym(manager->lib_handle, "engine_update");
    manager->api.render = dlsym(manager->lib_handle, "engine_render");
    manager->api.cleanup = dlsym(manager->lib_handle, "engine_cleanup");
    manager->api.handle_event = dlsym(manager->lib_handle, "engine_handle_event");
    manager->api.on_reload = dlsym(manager->lib_handle, "engine_on_reload");
    manager->api.get_state = dlsym(manager->lib_handle, "engine_get_state");

    if(!manager->api.init || !manager->api.update || !manager->api.render) {
        printf("Failed to load required engine functions\n");
        dlclose(manager->lib_handle);
        manager->lib_handle = NULL;
        return false;
    }

    if(is_reload && manager->api.on_reload) {
        manager->api.on_reload(old_data);
    } else {
        if(!manager->api.init(800,600)) {
            printf("Engine initialization failed\n");
            return false;
        }
    }
    
    printf("%s engine library succesfully\n", is_reload ? "Reloaded":"Loaded");
    return true;
}

bool check_for_library_changes(HotReloadManager* manager) {
    struct stat file_stat;
    if(stat(manager->lib_path,&file_stat)!=0){
        return false;
    }

    if(file_stat.st_mtime > manager->last_mod_time){
        manager->last_mod_time = file_stat.st_mtime;
        return true;
    }

    return false;
}

int main( int argc, char* args[] ) {
    if(!SDL_Init(SDL_INIT_VIDEO)){
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);

    SDL_Window* window = SDL_CreateWindow(
        "test",
        800,600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    if(!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if(!gl_context) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n",SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GL_SetSwapInterval(1);

    strcpy(g_reload_manager.lib_path, "libengine.so");

    if(!load_engine_library(&g_reload_manager, false)) {
        printf("Failed to load inital engine library\n");
        goto cleanup;
    }

    bool running = true;
    SDL_Event event;
    uint64_t last_time = SDL_GetTicks();

    printf("Engine running! Modify and recompile libengine.so to see hot reload in action.\n");
    printf("Press F5 to manually reload, ESC to quit\n");

    while(running) {
        uint64_t current_time = SDL_GetTicks();
        float delta_time = (current_time - last_time) / 1000.0f;
        last_time = current_time;

        while (SDL_PollEvent(&event)) {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                } else if(event.key.key == SDLK_F5) {
                    printf("Manual reload triggered\n");
                    load_engine_library(&g_reload_manager, true);
                }
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                printf("Window resized to %dx%x\n",
                    event.window.data1,event.window.data2);
                break;
            default:
                break;
            }

            if(g_reload_manager.api.handle_event) {
                if(g_reload_manager.api.handle_event(&event)) {

                }
            }
        }

        if(g_reload_manager.api.update) {
            g_reload_manager.api.update(delta_time);
        }

        if(g_reload_manager.api.render) {
            g_reload_manager.api.render();
        }
        
        SDL_GL_SwapWindow(window);
    }
cleanup:
    if(g_reload_manager.api.cleanup) {
        g_reload_manager.api.cleanup();
    }

    if(g_reload_manager.lib_handle) {
        dlclose(g_reload_manager.lib_handle);
    }

    if(strlen(g_reload_manager.temp_lib_path) > 0) {
        unlink(g_reload_manager.temp_lib_path);
    }

    SDL_GL_DestroyContext   (gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
