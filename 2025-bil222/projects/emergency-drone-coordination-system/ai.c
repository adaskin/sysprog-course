#include "headers/ai.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
void assign_mission(Drone *drone, Coord target) {
    pthread_mutex_lock(&drone->lock);
    drone->target = target;
    drone->status = ON_MISSION;
    pthread_mutex_unlock(&drone->lock);
}

Drone *find_closest_idle_drone(Coord target) {
    Drone *closest = NULL;
    int min_distance = INT_MAX;
    pthread_mutex_lock(&drones->lock);  // List mutex
    Node *node = drones->head;
    while (node != NULL) {
        Drone *d = (Drone *)node->data;
        if (d->status == IDLE) {
            int dist = abs(d->coord.x - target.x) +
                       abs(d->coord.y - target.y);
            if (dist < min_distance) {
                min_distance = dist;
                closest = d;
            }
        }
        node = node->next;
    }
    pthread_mutex_unlock(&drones->lock);  // List mutex
    return closest;
}

void *ai_controller(void *arg) {
    while (1) {
        // TODO change the lock and unlock location to avoid deadlock

        pthread_mutex_lock(&survivors->lock);  // List mutex
        Survivor *s = NULL;
        if (s = survivors->peek(survivors)) {  
            Drone *closest = find_closest_idle_drone(s->coord);
            if (closest) {
                assign_mission(closest, s->coord); // Uses drone->lock
                printf("Drone %d assigned to survivor at (%d, %d)\n",
                       closest->id, s->coord.x, s->coord.y);


                // Remove from global list: make sure to use
                // correct removedata or removenode
                survivors->removedata(survivors,s);  
                // TODO: assuming it is helped
                s->status = 1;  // Mark as helped
                s->helped_time = s->discovery_time;  

                // // TODO: you should remove it when the drone
                // reaches the survivor
                // // Remove from map cell's survivor list
                // pthread_mutex_lock(&helpedsurvivors->lock); // List
                // mutex helpedsurvivors->add(helpedsurvivors, s); //
                // Add to helped list
                // pthread_mutex_unlock(&helpedsurvivors->lock); //
                // List mutex

                printf("Survivor %s being helped by Drone %d\n",
                       s->info, closest->id);
            }
        }
        pthread_mutex_unlock(
            &survivors->lock);  // TODO change its location

        // // Remove from map cell (if needed)
        // pthread_mutex_lock(&map.cells[s.coord.x][s.coord.y].survivors->lock);
        // map.cells[s.coord.x][s.coord.y].survivors->removedata(
        //     map.cells[s.coord.x][s.coord.y].survivors, &s);
        // pthread_mutex_unlock(&map.cells[s.coord.x][s.coord.y].survivors->lock);
        sleep(1);
    }
    return NULL;
}