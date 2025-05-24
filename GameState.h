#ifndef GAME_STATE
#define GAME_STATE
typedef struct {
    bool initialized;
    float player_x, player_y;
    float player_rotation;
    float player_speed;
    unsigned int vao, vbo;
    int reload_count;
    float color_r, color_g, color_b;
} GameState;
#endif