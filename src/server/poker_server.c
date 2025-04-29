#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "poker_client.h"
#include "client_action_handler.h"
#include "game_logic.h"

#define BASE_PORT 2201
#define NUM_PORTS 6
#define BUFFER_SIZE 1024

typedef struct {
    int socket;
    struct sockaddr_in address;
} player_t;

game_state_t game; //global variable to store our game state info (this is a huge hint for you)

int main(int argc, char **argv) {
    int server_fds[NUM_PORTS], client_socket, player_count = 0;
    int opt = 1;
    struct sockaddr_in server_address;
    player_t players[MAX_PLAYERS];
    char buffer[BUFFER_SIZE] = {0};
    socklen_t addrlen = sizeof(struct sockaddr_in);

    //Setup the server infrastructre and accept the 6 players on ports 2201, 2202, 2203, 2204, 2205, 2206

    int rand_seed = argc == 2 ? atoi(argv[1]) : 0;
    init_game_state(&game, 100, rand_seed);

    //Join state?

    while (1) {
       
        // READY
        
        // DEAL TO PLAYERS

        // PREFLOP BETTING

        // PLACE FLOP CARDS

        // FLOP BETTING

        // PLACE TURN CARDS

        // TURN BETTING

        // PLACE RIVER CARDS

        // RIVER BETTING
        
        // ROUND_SHOWDOWN
    }

    printf("[Server] Shutting down.\n");

    // Close all fds (you're welcome)
    for (int i = 0; i < MAX_PLAYERS; i++) {
        close(server_fds[i]);
        if (game.player_status[i] != PLAYER_LEFT) {
            close(game.sockets[i]);
        }
    }

    return 0;
}