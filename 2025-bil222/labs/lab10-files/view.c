// #include "headers/view.h"
#include <SDL2/SDL.h>

#include "headers/drone.h"
#include "headers/map.h"
#include "headers/survivor.h"

#define CELL_SIZE 20  // Pixels per map cell

// SDL globals
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_Event event;
int window_width, window_height;

// Colors
const SDL_Color BLACK = {0, 0, 0, 255};
const SDL_Color RED = {255, 0, 0, 255};
const SDL_Color BLUE = {0, 0, 255, 255};
const SDL_Color GREEN = {0, 255, 0, 255};
const SDL_Color WHITE = {255, 255, 255, 255};

int init_sdl_window() {
    window_width = map.width * CELL_SIZE;
    window_height = map.height * CELL_SIZE;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    window =
        SDL_CreateWindow("Drone Simulator", SDL_WINDOWPOS_CENTERED,
                         SDL_WINDOWPOS_CENTERED, window_width,
                         window_height, SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n",
                SDL_GetError());
        SDL_Quit();
        return 1;
    }

    renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_DestroyWindow(window);
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n",
                SDL_GetError());
        SDL_Quit();
        return 1;
    }

    return 0;
}

void draw_cell(int x, int y, SDL_Color color) {
    SDL_Rect rect = {.x = y * CELL_SIZE,
                     .y = x * CELL_SIZE,
                     .w = CELL_SIZE,
                     .h = CELL_SIZE};
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b,
                           color.a);
    SDL_RenderFillRect(renderer, &rect);
}

void draw_drones() {
    for (int i = 0; i < num_drones; i++) {
        pthread_mutex_lock(&drone_fleet[i].lock);
        SDL_Color color =
            (drone_fleet[i].status == IDLE) ? BLUE : GREEN;
        draw_cell(drone_fleet[i].coord.x, drone_fleet[i].coord.y,
                  color);

        // Draw mission line if on mission
        if (drone_fleet[i].status == ON_MISSION) {
            SDL_SetRenderDrawColor(renderer, GREEN.r, GREEN.g,
                                   GREEN.b, GREEN.a);
            SDL_RenderDrawLine(
                renderer,
                drone_fleet[i].coord.y * CELL_SIZE + CELL_SIZE / 2,
                drone_fleet[i].coord.x * CELL_SIZE + CELL_SIZE / 2,
                drone_fleet[i].target.y * CELL_SIZE + CELL_SIZE / 2,
                drone_fleet[i].target.x * CELL_SIZE + CELL_SIZE / 2);
        }
        pthread_mutex_unlock(&drone_fleet[i].lock);
    }
}

void draw_survivors() {
    for (int i = 0; i < map.height; i++) {
        for (int j = 0; j < map.width; j++) {
            pthread_mutex_lock(&map.cells[i][j].survivors->lock);
            if (map.cells[i][j].survivors->number_of_elements > 0) {
                draw_cell(i, j, RED);
            }
            pthread_mutex_unlock(&map.cells[i][j].survivors->lock);
        }
    }
}

void draw_grid() {
    SDL_SetRenderDrawColor(renderer, WHITE.r, WHITE.g, WHITE.b,
                           WHITE.a);
    for (int i = 0; i <= map.height; i++) {
        SDL_RenderDrawLine(renderer, 0, i * CELL_SIZE, window_width,
                           i * CELL_SIZE);
    }
    for (int j = 0; j <= map.width; j++) {
        SDL_RenderDrawLine(renderer, j * CELL_SIZE, 0, j * CELL_SIZE,
                           window_height);
    }
}

int draw_map() {
    SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b,
                           BLACK.a);
    SDL_RenderClear(renderer);

    draw_survivors();
    draw_drones();
    draw_grid();

    SDL_RenderPresent(renderer);
    return 0;
}

int check_events() {
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) return 1;
        if (event.type == SDL_KEYDOWN &&
            event.key.keysym.sym == SDLK_ESCAPE)
            return 1;
    }
    return 0;
}

void quit_all() {
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}