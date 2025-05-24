#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

// Forward declarations for OpenGL types to avoid including GLAD
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLenum;
typedef float GLfloat;
typedef unsigned char GLboolean;
typedef void GLvoid;

// SDL types we need
typedef struct SDL_Window SDL_Window;
typedef void* SDL_GLContext;
typedef unsigned char Uint8;
typedef unsigned int Uint32;

// Engine state structure (must match the one in main.c)
typedef struct {
    void* persistent_memory;
    size_t persistent_memory_size;
    void* frame_memory;
    size_t frame_memory_size;
    SDL_Window* window;
    SDL_GLContext gl_context;
    unsigned int basic_shader_program;
    float delta_time;
    float total_time;
    const Uint8* keyboard_state;
    int mouse_x, mouse_y;
    Uint32 mouse_buttons;
    int window_width;
    int window_height;
    bool should_quit;
    bool is_reloaded;
} EngineState;

// Import the OpenGL functions we need from the main executable
extern void glGenVertexArrays(GLsizei n, GLuint *arrays);
extern void glGenBuffers(GLsizei n, GLuint *buffers);
extern void glBindVertexArray(GLuint array);
extern void glBindBuffer(GLenum target, GLuint buffer);
extern void glBufferData(GLenum target, GLsizei size, const GLvoid *data, GLenum usage);
extern void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);
extern void glEnableVertexAttribArray(GLuint index);
extern void glUseProgram(GLuint program);
extern GLint glGetUniformLocation(GLuint program, const char *name);
extern void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern void glDrawArrays(GLenum mode, GLint first, GLsizei count);
extern void glDeleteVertexArrays(GLsizei n, const GLuint *arrays);
extern void glDeleteBuffers(GLsizei n, const GLuint *buffers);

// OpenGL constants we need
#define GL_ARRAY_BUFFER          0x8892
#define GL_STATIC_DRAW           0x88E4
#define GL_FLOAT                 0x1406
#define GL_FALSE                 0
#define GL_TRIANGLES             0x0004

// SDL scancodes we need
#define SDL_SCANCODE_W 26
#define SDL_SCANCODE_A 4
#define SDL_SCANCODE_S 22
#define SDL_SCANCODE_D 7
#define SDL_SCANCODE_Q 20
#define SDL_SCANCODE_E 8
#define SDL_SCANCODE_R 21
#define SDL_SCANCODE_ESCAPE 41

// Game state that persists across reloads
typedef struct {
    bool initialized;
    float player_x, player_y;
    float player_rotation;
    float player_speed;
    unsigned int vao, vbo;
    int reload_count;
    float color_r, color_g, color_b;
} GameState;

// Simple matrix operations
typedef struct {
    float m[16];
} Mat4;

static Mat4 mat4_identity() {
    Mat4 result = {0};
    result.m[0] = result.m[5] = result.m[10] = result.m[15] = 1.0f;
    return result;
}

static Mat4 mat4_translate(float x, float y, float z) {
    Mat4 result = mat4_identity();
    result.m[12] = x;
    result.m[13] = y;
    result.m[14] = z;
    return result;
}

static Mat4 mat4_scale(float x, float y, float z) {
    Mat4 result = mat4_identity();
    result.m[0] = x;
    result.m[5] = y;
    result.m[10] = z;
    return result;
}

static Mat4 mat4_rotate_z(float angle) {
    Mat4 result = mat4_identity();
    float c = cosf(angle);
    float s = sinf(angle);
    result.m[0] = c;
    result.m[1] = s;
    result.m[4] = -s;
    result.m[5] = c;
    return result;
}

static Mat4 mat4_multiply(Mat4 a, Mat4 b) {
    Mat4 result = {0};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 4; k++) {
                result.m[i * 4 + j] += a.m[i * 4 + k] * b.m[k * 4 + j];
            }
        }
    }
    return result;
}

void engine_init(EngineState* state) {
    printf("Engine init called\n");
    
    // Get or initialize game state from persistent memory
    GameState* game = (GameState*)state->persistent_memory;
    
    if (!game->initialized || state->is_reloaded) {
        if (state->is_reloaded) {
            game->reload_count++;
            printf("Engine reloaded %d times\n", game->reload_count);
            
            // Change color on reload to show it's working
            game->color_r = (float)rand() / RAND_MAX;
            game->color_g = (float)rand() / RAND_MAX;
            game->color_b = (float)rand() / RAND_MAX;
        } else {
            // First time initialization
            game->initialized = true;
            game->player_x = 0.0f;
            game->player_y = 0.0f;
            game->player_rotation = 0.0f;
            game->player_speed = 200.0f;
            game->reload_count = 0;
            game->color_r = 1.0f;
            game->color_g = 0.5f;
            game->color_b = 0.0f;
        }
        
        // Create a triangle
        float vertices[] = {
            // positions         // colors
            -0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,
             0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,
             0.1f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f
        };
        
        // Generate vertex array and buffer
        glGenVertexArrays(1, &game->vao);
        glGenBuffers(1, &game->vbo);
        
        glBindVertexArray(game->vao);
        
        glBindBuffer(GL_ARRAY_BUFFER, game->vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        // Color attribute
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        glBindVertexArray(0);
    }
}

void engine_update(EngineState* state) {
    GameState* game = (GameState*)state->persistent_memory;
    
    // Handle input
    if (state->keyboard_state[SDL_SCANCODE_W]) {
        game->player_y += game->player_speed * state->delta_time;
    }
    if (state->keyboard_state[SDL_SCANCODE_S]) {
        game->player_y -= game->player_speed * state->delta_time;
    }
    if (state->keyboard_state[SDL_SCANCODE_A]) {
        game->player_x -= game->player_speed * state->delta_time;
    }
    if (state->keyboard_state[SDL_SCANCODE_D]) {
        game->player_x += game->player_speed * state->delta_time;
    }
    if (state->keyboard_state[SDL_SCANCODE_Q]) {
        game->player_rotation += 2.0f * state->delta_time;
    }
    if (state->keyboard_state[SDL_SCANCODE_E]) {
        game->player_rotation -= 2.0f * state->delta_time;
    }
    
    // Reset position with R
    if (state->keyboard_state[SDL_SCANCODE_R]) {
        game->player_x = 0.0f;
        game->player_y = 0.0f;
        game->player_rotation = 0.0f;
    }
    
    // Quit with ESC
    if (state->keyboard_state[SDL_SCANCODE_ESCAPE]) {
        state->should_quit = true;
    }
}

void engine_render(EngineState* state) {
    GameState* game = (GameState*)state->persistent_memory;
    
    // Use the shader program compiled in main.c
    glUseProgram(state->basic_shader_program);
    
    // Calculate aspect ratio
    float aspect = (float)state->window_width / state->window_height;
    
    // Create transformation matrix
    Mat4 scale = mat4_scale(0.5f / aspect, 0.5f, 1.0f);
    Mat4 rotate = mat4_rotate_z(game->player_rotation);
    Mat4 translate = mat4_translate(game->player_x / 400.0f, game->player_y / 300.0f, 0.0f);
    
    Mat4 transform = mat4_multiply(translate, mat4_multiply(rotate, scale));
    
    // Set transform uniform
    int transform_loc = glGetUniformLocation(state->basic_shader_program, "transform");
    glUniformMatrix4fv(transform_loc, 1, GL_FALSE, transform.m);
    
    glBindVertexArray(game->vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    
    // Draw some text info (would need text rendering in real app)
    if (state->is_reloaded) {
        printf("Reloaded! Position: (%.2f, %.2f), Rotation: %.2f, Reloads: %d\n", 
               game->player_x, game->player_y, game->player_rotation, game->reload_count);
    }
}

void engine_cleanup(EngineState* state) {
    printf("Engine cleanup called\n");
    
    GameState* game = (GameState*)state->persistent_memory;
    
    // Clean up OpenGL resources
    if (game->vao) {
        glDeleteVertexArrays(1, &game->vao);
        game->vao = 0;
    }
    if (game->vbo) {
        glDeleteBuffers(1, &game->vbo);
        game->vbo = 0;
    }
}
