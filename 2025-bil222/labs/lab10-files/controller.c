// controller.c
#include "headers/globals.h"
#include "headers/map.h"
#include "headers/drone.h"
#include "headers/survivor.h"
#include "headers/ai.h"
#include "headers/list.h"
#include "headers/view.h"
#include <stdio.h>
List *survivors, *helpedsurvivors, *drones;


int main() {
    // Initialize global lists
    survivors = create_list(sizeof(Survivor), 1000);     // Survivors waiting for help
    helpedsurvivors = create_list(sizeof(Survivor), 1000); // Helped survivors
    drones = create_list(sizeof(Drone), 100);            // Active drones

    // Initialize map (depends on survivors list for cells)
    init_map(40, 30); // Example: 40x30 grid

    // Initialize drones (spawn threads)
    initialize_drones();

    // Start survivor generator thread
    pthread_t survivor_thread;
    pthread_create(&survivor_thread, NULL, survivor_generator, NULL);

    // Start AI controller thread
    pthread_t ai_thread;
    pthread_create(&ai_thread, NULL, ai_controller, NULL);

    // Start SDL visualization
    init_sdl_window();
    while (!check_events()) {
        draw_map();
        SDL_Delay(100);
    }
    printf("Exiting...\n");
    // Cleanup
    freemap();
    survivors->destroy(survivors);
    helpedsurvivors->destroy(helpedsurvivors);
    drones->destroy(drones);
    quit_all();
    return 0;
}