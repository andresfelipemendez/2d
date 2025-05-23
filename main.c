#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <execinfo.h>

#include <SDL3/SDL.h>
#include <glad.h>

#include "platform.h"

// Signal handler for debugging
void signal_handler(int sig) {
    void *array[10];
    size_t size;
    
    // Get void*'s for all entries on the stack
    size = backtrace(array, 10);
    
    // Print out all the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

// Engine interface structure - shared between main and engine
typedef struct {
    // Persistent memory block that survives reloads
    void* persistent_memory;
    size_t persistent_memory_size;
    
    // Frame memory that gets cleared each frame
    void* frame_memory;
    size_t frame_memory_size;
    
    // Platform services that engine can use
    SDL_Window* window;
    SDL_GLContext gl_context;
    
    // Shader programs compiled by main.c
    unsigned int basic_shader_program;
    
    // Timing info
    float delta_time;
    float total_time;
    
    // Input state
    const Uint8* keyboard_state;
    int mouse_x, mouse_y;
    Uint32 mouse_buttons;
    
    // Window dimensions
    int window_width;
    int window_height;
    
    // Control flags
    bool should_quit;
    bool is_reloaded;
} EngineState;

// Engine function pointers
typedef void (*engine_init_func)(EngineState* state);
typedef void (*engine_update_func)(EngineState* state);
typedef void (*engine_render_func)(EngineState* state);
typedef void (*engine_cleanup_func)(EngineState* state);

typedef struct {
    void* handle;
    engine_init_func init;
    engine_update_func update;
    engine_render_func render;
    engine_cleanup_func cleanup;
    time_t last_write_time;
} EngineLibrary;

// Shader compilation helper
static unsigned int compile_shader(const char* vertex_src, const char* fragment_src) {
    // Compile vertex shader
    unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_src, NULL);
    glCompileShader(vertex_shader);
    
    int success;
    char info_log[512];
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertex_shader, 512, NULL, info_log);
        printf("Vertex shader compilation failed: %s\n", info_log);
    }
    
    // Compile fragment shader
    unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_src, NULL);
    glCompileShader(fragment_shader);
    
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragment_shader, 512, NULL, info_log);
        printf("Fragment shader compilation failed: %s\n", info_log);
    }
    
    // Link program
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, info_log);
        printf("Shader program linking failed: %s\n", info_log);
    }
    
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    
    return program;
}

// Get last write time of library file
static time_t get_library_write_time(const char* filename) {
    struct stat file_stat;
    if (stat(filename, &file_stat) == 0) {
        return file_stat.st_mtime;
    }
    return 0;
}

// Load engine library
static bool load_engine_library(EngineLibrary* lib, const char* lib_path, const char* temp_path) {
    printf("DEBUG: load_engine_library called with lib_path=%s, temp_path=%s\n", lib_path, temp_path);
    
    // Copy library to temp location to avoid locking issues
    char copy_cmd[512];
    snprintf(copy_cmd, sizeof(copy_cmd), "cp %s %s", lib_path, temp_path);
    printf("DEBUG: Copying library with command: %s\n", copy_cmd);
    int ret = system(copy_cmd);
    if (ret != 0) {
        printf("ERROR: Failed to copy library, return code: %d\n", ret);
        return false;
    }
    
    // Load the library
    printf("DEBUG: About to dlopen %s\n", temp_path);
    lib->handle = dlopen(temp_path, RTLD_NOW);
    if (!lib->handle) {
        printf("Failed to load engine library: %s\n", dlerror());
        return false;
    }
    printf("DEBUG: dlopen successful, handle=%p\n", lib->handle);
    
    // Get function pointers
    printf("DEBUG: Getting function pointers...\n");
    lib->init = (engine_init_func)dlsym(lib->handle, "engine_init");
    printf("DEBUG: engine_init = %p\n", lib->init);
    
    lib->update = (engine_update_func)dlsym(lib->handle, "engine_update");
    printf("DEBUG: engine_update = %p\n", lib->update);
    
    lib->render = (engine_render_func)dlsym(lib->handle, "engine_render");
    printf("DEBUG: engine_render = %p\n", lib->render);
    
    lib->cleanup = (engine_cleanup_func)dlsym(lib->handle, "engine_cleanup");
    printf("DEBUG: engine_cleanup = %p\n", lib->cleanup);
    
    if (!lib->init || !lib->update || !lib->render || !lib->cleanup) {
        printf("Failed to load engine functions\n");
        printf("  init: %p\n", lib->init);
        printf("  update: %p\n", lib->update);
        printf("  render: %p\n", lib->render);
        printf("  cleanup: %p\n", lib->cleanup);
        dlclose(lib->handle);
        lib->handle = NULL;
        return false;
    }
    
    lib->last_write_time = get_library_write_time(lib_path);
    printf("DEBUG: Library loaded successfully\n");
    return true;
}

// Unload engine library
static void unload_engine_library(EngineLibrary* lib) {
    if (lib->handle) {
        dlclose(lib->handle);
        lib->handle = NULL;
    }
}

// Basic shader sources
static const char* basic_vertex_shader = 
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec3 aColor;\n"
    "out vec3 vertexColor;\n"
    "uniform mat4 transform;\n"
    "void main() {\n"
    "    gl_Position = transform * vec4(aPos, 1.0);\n"
    "    vertexColor = aColor;\n"
    "}\n";

static const char* basic_fragment_shader = 
    "#version 330 core\n"
    "in vec3 vertexColor;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    FragColor = vec4(vertexColor, 1.0);\n"
    "}\n";

int main(int argc, char* argv[]) {
    // Install signal handlers for debugging
    signal(SIGSEGV, signal_handler);
    signal(SIGABRT, signal_handler);
    
    printf("=== Hot Reload Engine Starting ===\n");
    printf("Platform: %s\n", PLATFORM_NAME);
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    
    // Set OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    
    // Create window
    SDL_Window* window = SDL_CreateWindow(
        "Hot Reload Engine",
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    
    if (!window) {
        printf("Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    // Create OpenGL context
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        printf("OpenGL context creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Enable vsync
    SDL_GL_SetSwapInterval(1);
    
    // Load OpenGL functions with GLAD
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        printf("Failed to initialize GLAD\n");
       // SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
    printf("GLSL Version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    
    // Set up OpenGL state
    glViewport(0, 0, 800, 600);
    glEnable(GL_DEPTH_TEST);
    
    // Compile shaders in main (since OpenGL state isn't shared)
    unsigned int basic_shader = compile_shader(basic_vertex_shader, basic_fragment_shader);
    
    // Allocate persistent memory for engine
    const size_t persistent_size = 64 * 1024 * 1024; // 64MB
    const size_t frame_size = 16 * 1024 * 1024;      // 16MB
    
    void* persistent_memory = malloc(persistent_size);
    void* frame_memory = malloc(frame_size);
    
    if (!persistent_memory || !frame_memory) {
        printf("Failed to allocate memory\n");
        return 1;
    }
    
    // Initialize engine state
    EngineState engine_state = {
        .persistent_memory = persistent_memory,
        .persistent_memory_size = persistent_size,
        .frame_memory = frame_memory,
        .frame_memory_size = frame_size,
        .window = window,
        .gl_context = gl_context,
        .basic_shader_program = basic_shader,
        .delta_time = 0.0f,
        .total_time = 0.0f,
        .keyboard_state = NULL,
        .mouse_x = 0,
        .mouse_y = 0,
        .mouse_buttons = 0,
        .window_width = 800,
        .window_height = 600,
        .should_quit = false,
        .is_reloaded = false
    };
    
    // Engine library paths
    const char* lib_name = "libengine" DYLIB_EXTENSION;
    const char* temp_lib_name = "./libengine_temp" DYLIB_EXTENSION;  // Force current directory
    
    // Check if engine library exists
    if (access(lib_name, F_OK) != 0) {
        printf("ERROR: Engine library '%s' not found!\n", lib_name);
        printf("Make sure to build the engine library first.\n");
        return 1;
    }
    
    // Load engine library
    EngineLibrary engine = {0};
    printf("DEBUG: Loading engine library from %s\n", lib_name);
    if (!load_engine_library(&engine, lib_name, temp_lib_name)) {
        printf("Failed to load engine library\n");
        return 1;
    }
    printf("DEBUG: Engine library loaded successfully\n");
    
    // Initialize engine
    printf("DEBUG: Calling engine.init\n");
    if (engine.init) {
        engine.init(&engine_state);
        printf("DEBUG: engine.init completed\n");
    } else {
        printf("ERROR: engine.init is NULL!\n");
    }
    
    // Main loop
    Uint64 last_time = SDL_GetPerformanceCounter();
    bool running = true;
    
    while (running && !engine_state.should_quit) {
        // Check for library changes
        time_t current_write_time = get_library_write_time(lib_name);
        if (current_write_time != engine.last_write_time && current_write_time != 0) {
            printf("\n=== Reloading engine library ===\n");
            
            // Call cleanup on old version
            if (engine.cleanup) {
                engine.cleanup(&engine_state);
            }
            
            // Unload old library
            unload_engine_library(&engine);
            
            // Wait a bit for file write to complete
            usleep(100000); // 100ms
            
            // Load new library
            if (load_engine_library(&engine, lib_name, temp_lib_name)) {
                engine_state.is_reloaded = true;
                engine.init(&engine_state);
                printf("Engine reloaded successfully\n");
            } else {
                printf("Failed to reload engine\n");
                break;
            }
        }
        
        // Calculate delta time
        Uint64 current_time = SDL_GetPerformanceCounter();
        engine_state.delta_time = (float)(current_time - last_time) / SDL_GetPerformanceFrequency();
        engine_state.total_time += engine_state.delta_time;
        last_time = current_time;
        
        // Clear frame memory
        memset(engine_state.frame_memory, 0, engine_state.frame_memory_size);
        
        // Handle events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                engine_state.window_width = event.window.data1;
                engine_state.window_height = event.window.data2;
                glViewport(0, 0, engine_state.window_width, engine_state.window_height);
            }
        }
        
        // Update input state
        engine_state.keyboard_state = SDL_GetKeyboardState(NULL);
        engine_state.mouse_buttons = SDL_GetMouseState(&engine_state.mouse_x, &engine_state.mouse_y);
        
        // Update engine
        engine.update(&engine_state);
        
        // Clear screen
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Render engine
        engine.render(&engine_state);
        
        // Swap buffers
        SDL_GL_SwapWindow(window);
        
        // Reset reload flag
        engine_state.is_reloaded = false;
    }
    
    // Cleanup
    printf("\n=== Shutting down ===\n");
    
    if (engine.cleanup) {
        engine.cleanup(&engine_state);
    }
    
    unload_engine_library(&engine);
    
    glDeleteProgram(basic_shader);
    
    free(persistent_memory);
    free(frame_memory);
    
    //SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}