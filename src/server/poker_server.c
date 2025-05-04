#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#include "poker_client.h"
#include "client_action_handler.h"
#include "game_logic.h"

#define BASE_PORT 2201
#define NUM_PORTS 6
#define BUFFER_SIZE 1024

typedef struct
{
    int socket;
    struct sockaddr_in address;
} player_t;

game_state_t game;

int main(int argc, char **argv)
{
    int server_fds[NUM_PORTS], client_socket;
    int opt = 1;
    struct sockaddr_in server_address;
    player_t players[MAX_PLAYERS];
    char buffer[BUFFER_SIZE] = {0};
    socklen_t addrlen = sizeof(struct sockaddr_in);

    // Initialize server sockets
    for (int i = 0; i < NUM_PORTS; i++)
    {
        server_fds[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fds[i] < 0)
        {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }

        // Set socket options
        if (setsockopt(server_fds[i], SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
        {
            perror("Setsockopt failed");
            exit(EXIT_FAILURE);
        }

        // Configure server address
        memset(&server_address, 0, sizeof(server_address));
        server_address.sin_family = AF_INET;
        server_address.sin_addr.s_addr = INADDR_ANY;
        server_address.sin_port = htons(BASE_PORT + i);

        // Bind socket
        if (bind(server_fds[i], (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
        {
            perror("Bind failed");
            exit(EXIT_FAILURE);
        }

        // Listen for connections
        if (listen(server_fds[i], 1) < 0)
        {
            perror("Listen failed");
            exit(EXIT_FAILURE);
        }
    }

    // Initialize game state
    int rand_seed = argc == 2 ? atoi(argv[1]) : time(NULL);
    init_game_state(&game, 1000, rand_seed); // Starting stack of 1000

    // Accept initial connections
    for (int i = 0; i < NUM_PORTS; i++)
    {
        client_socket = accept(server_fds[i], (struct sockaddr *)&players[i].address, &addrlen);
        if (client_socket < 0)
        {
            perror("Accept failed");
            continue;
        }
        game.sockets[i] = client_socket;
        game.player_status[i] = PLAYER_ACTIVE;
    }

    // Main game loop
    while (1)
    {
        switch (game.round_stage)
        {
        case ROUND_JOIN:
            server_join(&game);
            break;

        case ROUND_INIT:
            if (server_ready(&game) < 2)
            {
                // Not enough players - send HALT
                server_packet_t halt_pkt = {.packet_type = HALT};
                for (int i = 0; i < game.num_players; i++)
                {
                    if (game.player_status[i] != PLAYER_LEFT)
                    {
                        send(game.sockets[i], &halt_pkt, sizeof(halt_pkt), 0);
                    }
                }
                goto cleanup;
            }
            server_deal(&game);
            break;

        case ROUND_PREFLOP:
        case ROUND_FLOP:
        case ROUND_TURN:
        case ROUND_RIVER:
            if (server_bet(&game) < 0)
            {
                goto cleanup;
            }
            server_community(&game);
            if (game.round_stage == ROUND_RIVER)
            {
                game.round_stage = ROUND_SHOWDOWN;
            }
            break;

        case ROUND_SHOWDOWN:
            server_end(&game);
            reset_game_state(&game);
            break;

        default:
            fprintf(stderr, "Invalid game stage\n");
            goto cleanup;
        }
    }

cleanup:
    // Close all sockets
    for (int i = 0; i < NUM_PORTS; i++)
    {
        close(server_fds[i]);
        if (game.sockets[i] > 0)
        {
            close(game.sockets[i]);
        }
    }
    return 0;
}