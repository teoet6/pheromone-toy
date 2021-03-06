#include <iostream>
#include "pishtov.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define WIDTH  800
#define HEIGHT 600
#define AGENTS 100000
#define RADIUS 100.f
#define SIZE 50.f

float AGENT_SPEED = 1.f;
float LOOK_ANGLE = M_PI / 12.f;
float STEER_MAX_ANGLE = M_PI / 12.f;
float LOOK_DISTANCE = 100.f;
float DECAY_RATE = .99f;
bool SQUARE_BIAS = 0;
Color FG = 0x000040ff;
Color BG = 0xffffbfff;

struct Agent {
    float angle;
    float speed;
    Vec2 pos;
};

Image board;
Image backboard;
Agent agents[AGENTS];

float frand() {
    return (float)(rand() % 10000) / 10000.f;
}

void random_params() {
    AGENT_SPEED = frand() * 3.f;
    LOOK_ANGLE = frand() * M_PI / 2.f;
    LOOK_DISTANCE  = frand() * 50.f;
    STEER_MAX_ANGLE = frand() * M_PI / 2.f;
    DECAY_RATE = frand() * .5f + .5f;
    SQUARE_BIAS = floor(frand() * 2.f);
    FG = rgb(frand() * 32.f, frand() * 32.f, frand() * 32.f);
    BG = (0xffffffff - FG) | 0xff;
    printf("%f %f %f %f %f %d %x %x\n", AGENT_SPEED, LOOK_ANGLE, LOOK_DISTANCE , STEER_MAX_ANGLE, DECAY_RATE, SQUARE_BIAS, FG, BG);
}

void setup() {
    //random_params();
    board.width  = WIDTH;
    board.height = HEIGHT;
    board.data.resize(board.width * board.height);
    backboard.width  = board.width;
    backboard.height = board.height;
    backboard.data.resize(backboard.width * backboard.height);
    for (int i = 0; i < board.height; ++i) for (int j = 0; j < board.width; ++j)
        board.data[i * board.width + j] = FG & ~0xff;
    //for (int i = 0; i < LEN(agents); ++i)
    //    agents[i] = {(float)(rand() % 628) / 100.f, AGENT_SPEED, {rand() % WIDTH, rand() % HEIGHT}};
    //for (int i = 0; i < LEN(agents); ++i)
    //    agents[i] = {(float)(rand() % 628) / 100.f, AGENT_SPEED, {board.width / 2, board.height / 2}};
    for (int i = 0; i < LEN(agents); ++i)
        agents[i] = {(float)i / LEN(agents) * 2 * M_PI + M_PI / 2, AGENT_SPEED, 
            board.width  / 2 + RADIUS * cos((float)i / LEN(agents) * 2 * M_PI) + (rand() % (int)SIZE) - SIZE / 2, 
            board.height / 2 + RADIUS * sin((float)i / LEN(agents) * 2 * M_PI) + (rand() % (int)SIZE) - SIZE / 2};
}

Vec2 teleport(Vec2 pos) {
    if (pos.x <  0.f)    pos.x += WIDTH;
    if (pos.y <  0.f)    pos.y += HEIGHT;
    if (pos.x >= WIDTH)  pos.x -= WIDTH;
    if (pos.y >= HEIGHT) pos.y -= HEIGHT;
    return pos;
}

Color get_board(int i, int j) {
    Vec2 tp = teleport({j, i});
    return board.data[tp.y * board.width + tp.x];
}

void set_board(int i, int j, Color c) {
    Vec2 tp = teleport({j, i});
    board.data[tp.y * board.width + tp.x] = c;
}

Color average_color(int i, int j) {
    int r = 0, g = 0, b = 0, a = 0;
    Color c;
    c = get_board(i,   j);   r += (c >> 24) & 0xff; g += (c >> 16) & 0xff; b += (c >>  8) & 0xff; a += c & 0xff;
    c = get_board(i+1, j);   r += (c >> 24) & 0xff; g += (c >> 16) & 0xff; b += (c >>  8) & 0xff; a += c & 0xff;
    c = get_board(i-1, j);   r += (c >> 24) & 0xff; g += (c >> 16) & 0xff; b += (c >>  8) & 0xff; a += c & 0xff;
    c = get_board(i,   j+1); r += (c >> 24) & 0xff; g += (c >> 16) & 0xff; b += (c >>  8) & 0xff; a += c & 0xff;
    c = get_board(i,   j-1); r += (c >> 24) & 0xff; g += (c >> 16) & 0xff; b += (c >>  8) & 0xff; a += c & 0xff;
    //c = get_board(i+1, j+1); r += (c >> 24) & 0xff; g += (c >> 16) & 0xff; b += (c >>  8) & 0xff; a += c & 0xff;
    //c = get_board(i-1, j-1); r += (c >> 24) & 0xff; g += (c >> 16) & 0xff; b += (c >>  8) & 0xff; a += c & 0xff;
    //c = get_board(i-1, j+1); r += (c >> 24) & 0xff; g += (c >> 16) & 0xff; b += (c >>  8) & 0xff; a += c & 0xff;
    //c = get_board(i+1, j-1); r += (c >> 24) & 0xff; g += (c >> 16) & 0xff; b += (c >>  8) & 0xff; a += c & 0xff;
    return rgba(r/5, g/5, b/5, a/5);
}

float new_angle(Agent a) {
    float az = a.angle;
    float ap = a.angle + LOOK_ANGLE;
    float am = a.angle - LOOK_ANGLE;
    int wz = std::max(get_board(a.pos.y + sin(az) * a.speed * LOOK_DISTANCE, a.pos.x + cos(az) * a.speed * LOOK_DISTANCE) & 0xff, 1u);
    int wp = std::max(get_board(a.pos.y + sin(ap) * a.speed * LOOK_DISTANCE, a.pos.x + cos(ap) * a.speed * LOOK_DISTANCE) & 0xff, 1u);
    int wm = std::max(get_board(a.pos.y + sin(am) * a.speed * LOOK_DISTANCE, a.pos.x + cos(am) * a.speed * LOOK_DISTANCE) & 0xff, 1u);

    if (SQUARE_BIAS) {
        wz *= wz;
        wp *= wp;
        wm *= wm;
    }

    int r = rand() % (wz + wp + wm);
    if (r < wz)           return az;
    if (r < wz + wp)      return az + frand() * STEER_MAX_ANGLE;
    return az - frand() * STEER_MAX_ANGLE;
}

Color decay(Color c) {
    int gray = c & 0xff;
    if (!gray) return c;
    float fgray = gray;
    fgray *= DECAY_RATE;
    return FG & ~0xff | (int)fgray;
}

void blur() {
    for (int i = 0; i < board.height; ++i) for (int j = 0; j < board.width; ++j)
        backboard.data[i * board.width + j] = average_color(i, j);
    for (int i = 0; i < board.height; ++i) for (int j = 0; j < board.width; ++j)
        board.data[i * board.width + j] = backboard.data[i * board.width + j];
}

void mouse_pheromones() {
    int r = 5;
    for (int i = mouseY - r; i <= mouseY + r; ++i) for (int j = mouseX - r; j <= mouseX + r; ++j)
        set_board(i, j, FG);
}

void update() {
    if (isMousePressed[1]) mouse_pheromones();
    for (int i = 0; i < LEN(agents); ++i) {
        agents[i].angle = new_angle(agents[i]);
        agents[i].pos.x += cos(agents[i].angle) * agents[i].speed;
        agents[i].pos.y += sin(agents[i].angle) * agents[i].speed;
        int x = agents[i].pos.x;
        int y = agents[i].pos.y;
        agents[i].pos = teleport(agents[i].pos);
        set_board(y, x, FG);
    }
    blur();
    for (int i = 0; i < board.height; ++i) for (int j = 0; j < board.width; ++j)
        board.data[i * board.width + j] = decay(board.data[i * board.width + j]);
}

void draw() {
    fillColor(BG);
    fillRect(0, 0, windowX, windowY);
    drawImage(board, 0,           0,            board.width, board.height);
    drawImage(board, board.width, 0,            board.width, board.height);
    drawImage(board, 0,           board.height, board.width, board.height);
    drawImage(board, board.width, board.height, board.width, board.height);
}

void keydown(int key) {
    if (key == ' ') random_params();
}

void keyup(int key) {
    printf("Keyup: %d\n", key);
}

void mousedown(int button) {
}

void mouseup(int button) {
    printf("Mouseup: %d %d\n", mouseX, mouseY);
}
