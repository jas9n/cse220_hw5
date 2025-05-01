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

typedef struct
{
    int socket;
    struct sockaddr_in address;
} player_t;

game_state_t game; // global variable to store our game state info (this is a huge hint for you)

int main(int argc, char **argv)
{
    int server_fds[NUM_PORTS], client_socket, player_count = 0;
    int opt = 1;
    struct sockaddr_in server_address;
    player_t players[MAX_PLAYERS];
    char buffer[BUFFER_SIZE] = {0};
    socklen_t addrlen = sizeof(struct sockaddr_in);

    // Setup the server infrastructre and accept the 6 players on ports 2201, 2202, 2203, 2204, 2205, 2206
    for (int i = 0; i < NUM_PORTS; i++)
    {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
        {
            perror("socket");
            exit(EXIT_FAILURE);
        }

        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(BASE_PORT + i);

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            perror("bind");
            exit(EXIT_FAILURE);
        }

        if (listen(fd, 1) < 0)
        {
            perror("listen");
            exit(EXIT_FAILURE);
        }
        server_fds[i] = fd;
    }

    int rand_seed = argc == 2 ? atoi(argv[1]) : 0;
    init_game_state(&game, 100, rand_seed);

    // Join state?
    for (int pid = 0; pid < MAX_PLAYERS; pid++)
    {
        int client_fd = accept(server_fds[pid], NULL, NULL);
        if (client_fd < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        game.sockets[pid] = client_fd;
        game.player_status[pid] = PLAYER_ACTIVE;
        // read JOIN packet from client
        client_packet_t pkt;
        recv(client_fd, &pkt, sizeof(pkt), 0);
        // validate pkt.packet_type == JOIN
    }

    game.num_players = MAX_PLAYERS;
    game.round_stage = ROUND_INIT;

    while (1)
    {
        switch (game.round_stage)
        {
        case ROUND_JOIN:
            game.round_stage = ROUND_INIT;
            break;
        case ROUND_INIT:
            server_ready(&game);
            server_deal(&game);
            game.round_stage = ROUND_PREFLOP;
            break;

        case ROUND_PREFLOP:
            server_bet(&game);
            server_community(&game); // flop
            game.round_stage = ROUND_FLOP;
            break;

        case ROUND_FLOP:
            server_bet(&game);
            server_community(&game); // turn
            game.round_stage = ROUND_TURN;
            break;

        case ROUND_TURN:
            server_bet(&game);
            server_community(&game); // river
            game.round_stage = ROUND_RIVER;
            break;

        case ROUND_RIVER:
            server_bet(&game);
            game.round_stage = ROUND_SHOWDOWN;
            break;

        case ROUND_SHOWDOWN:
            server_end(&game);
            reset_game_state(&game);
            game.round_stage = ROUND_INIT;
            break;
        }
    }

    printf("[Server] Shutting down.\n");

    // Close all fds (you're welcome)
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        close(server_fds[i]);
        if (game.player_status[i] != PLAYER_LEFT)
        {
            close(game.sockets[i]);
        }
    }

    return 0;
}