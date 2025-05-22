#include <SDL3/SDL.h>
#include <glad.h>
#include <SDL3/SDL_opengl.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>

//

typedef struct{
    float rotation;
    float color_time;
    int triangle_count;
    bool wireframe_mode;
    float scale;

    GLuint vao;
    GLuint vbo;
    GLuint shader_program;
} EngineState;

static EngineState* g_engine_state = NULL;

// Simple vertex shader
const char* vertex_shader_source = 
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "uniform float u_rotation;\n"
    "uniform float u_scale;\n"
    "void main() {\n"
    "    float c = cos(u_rotation);\n"
    "    float s = sin(u_rotation);\n"
    "    mat2 rot = mat2(c, -s, s, c);\n"
    "    vec2 rotated = rot * aPos.xy * u_scale;\n"
    "    gl_Position = vec4(rotated, aPos.z, 1.0);\n"
    "}\n";

// Simple fragment shader with animated color
const char* fragment_shader_source = 
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform float u_time;\n"
    "void main() {\n"
    "    vec3 color = vec3(\n"
    "        sin(u_time) * 0.5 + 0.5,\n"
    "        sin(u_time + 2.0) * 0.5 + 0.5,\n"
    "        sin(u_time + 4.0) * 0.5 + 0.5\n"
    "    );\n"
    "    FragColor = vec4(color, 1.0);\n"
    "}\n";

GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, 512, NULL, info_log);
        printf("Shader compilation failed: %s\n", info_log);
        return 0;
    }
    
    return shader;
}

GLuint create_shader_program() {
    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    
    if (!vertex_shader || !fragment_shader) {
        return 0;
    }
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(program, 512, NULL, info_log);
        printf("Shader program linking failed: %s\n", info_log);
        return 0;
    }
    
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    
    return program;
}

void setup_triangle_geometry(EngineState* state) {
    // Triangle vertices
    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f,-0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };
    
    glGenVertexArrays(1, &state->vao);
    glGenBuffers(1, &state->vbo);
    
    glBindVertexArray(state->vao);
    glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

bool engine_init(int width, int height) {
    printf("Engine initializing... (width: %d, height: %d)\n", width, height);
    
    // Allocate engine state
    g_engine_state = malloc(sizeof(EngineState));
    if (!g_engine_state) {
        return false;
    }
    
    // Initialize state
    g_engine_state->rotation = 0.0f;
    g_engine_state->color_time = 0.0f;
    g_engine_state->triangle_count = 1;
    g_engine_state->wireframe_mode = false;
    g_engine_state->scale = 1.0f;
    
    // Set viewport
    glViewport(0, 0, width, height);
    
    // Create shader program
    g_engine_state->shader_program = create_shader_program();
    if (!g_engine_state->shader_program) {
        free(g_engine_state);
        g_engine_state = NULL;
        return false;
    }
    
    // Setup geometry
    setup_triangle_geometry(g_engine_state);
    
    // Enable some OpenGL features
    glEnable(GL_DEPTH_TEST);
    
    printf("Engine initialized successfully!\n");
    printf("Try modifying this message and recompiling to see hot reload!\n");
    
    return true;
}

void engine_update(float delta_time) {
    if (!g_engine_state) return;
    
    // Update rotation - change this value to see immediate effect!
    g_engine_state->rotation += delta_time * 1.5f; // Try changing this speed!
    
    // Update color animation time
    g_engine_state->color_time += delta_time * 2.0f; // Try changing this too!
    
    // Pulsing scale effect - modify this for different effects!
    g_engine_state->scale = 0.5f + 0.3f * sin(g_engine_state->color_time * 0.5f);
}


void engine_render(void) {
    if (!g_engine_state) return;
    
    // Clear with a dark background - try changing this color!
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f); // Try: (0.2f, 0.1f, 0.1f, 1.0f) for red tint
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Use shader program
    glUseProgram(g_engine_state->shader_program);
    
    // Set uniforms
    GLint rotation_loc = glGetUniformLocation(g_engine_state->shader_program, "u_rotation");
    GLint time_loc = glGetUniformLocation(g_engine_state->shader_program, "u_time");
    GLint scale_loc = glGetUniformLocation(g_engine_state->shader_program, "u_scale");
    
    glUniform1f(rotation_loc, g_engine_state->rotation);
    glUniform1f(time_loc, g_engine_state->color_time);
    glUniform1f(scale_loc, g_engine_state->scale);
    
    // Set wireframe mode if enabled
    if (g_engine_state->wireframe_mode) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    
    // Draw triangle(s) - try increasing triangle_count for multiple triangles!
    glBindVertexArray(g_engine_state->vao);
    for (int i = 0; i < g_engine_state->triangle_count; i++) {
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glBindVertexArray(0);
}

void engine_cleanup(void) {
    if (!g_engine_state) return;
    
    printf("Engine cleanup\n");
    
    // Cleanup OpenGL objects
    if (g_engine_state->vao) {
        glDeleteVertexArrays(1, &g_engine_state->vao);
    }
    if (g_engine_state->vbo) {
        glDeleteBuffers(1, &g_engine_state->vbo);
    }
    if (g_engine_state->shader_program) {
        glDeleteProgram(g_engine_state->shader_program);
    }
    
    free(g_engine_state);
    g_engine_state = NULL;
}

bool engine_handle_event(SDL_Event* event) {
    if (!g_engine_state) return false;
    
    if (event->type == SDL_EVENT_KEY_DOWN) {
        switch (event->key.key) {
            case SDLK_SPACE:
                g_engine_state->wireframe_mode = !g_engine_state->wireframe_mode;
                printf("Wireframe mode: %s\n", g_engine_state->wireframe_mode ? "ON" : "OFF");
                return true;
                
            case SDLK_UP:
                g_engine_state->triangle_count++;
                printf("Triangle count: %d\n", g_engine_state->triangle_count);
                return true;
                
            case SDLK_DOWN:
                if (g_engine_state->triangle_count > 1) {
                    g_engine_state->triangle_count--;
                    printf("Triangle count: %d\n", g_engine_state->triangle_count);
                }
                return true;
        }
    }
    
    return false; // Event not handled
}

// Hot reload support
void engine_on_reload(void* old_state) {
    if (old_state) {
        // Transfer state from old instance
        EngineState* old = (EngineState*)old_state;
        
        printf("Hot reload: Transferring state (rotation: %.2f)\n", old->rotation);
        
        // Copy old state to new instance
        if (g_engine_state) {
            g_engine_state->rotation = old->rotation;
            g_engine_state->color_time = old->color_time;
            g_engine_state->triangle_count = old->triangle_count;
            g_engine_state->wireframe_mode = old->wireframe_mode;
            g_engine_state->scale = old->scale;
        }
        
        free(old_state);
    }
    
    printf("=== HOT RELOAD COMPLETE ===\n");
    printf("Try modifying render colors, rotation speed, or add new features!\n");
    printf("Controls: SPACE=wireframe, UP/DOWN=triangle count\n");
}

void* engine_get_state(void) {
    if (!g_engine_state) return NULL;
    
    // Create a copy of current state for transfer
    EngineState* state_copy = malloc(sizeof(EngineState));
    if (state_copy) {
        *state_copy = *g_engine_state;
        // Don't copy OpenGL objects - they'll be recreated
        state_copy->vao = 0;
        state_copy->vbo = 0;
        state_copy->shader_program = 0;
    }
    
    return state_copy;
}