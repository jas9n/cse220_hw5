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
#include "logs.h"

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
    int server_fds[NUM_PORTS];
    int opt = 1;
    struct sockaddr_in server_addr;
    socklen_t addrlen = sizeof(server_addr);

    // Initialize logging
    log_init("SERVER");
    log_player_init(MAX_PLAYERS);

    int seed = (argc >= 2) ? atoi(argv[1]) : (int)time(NULL);
    init_game_state(&game, 100, seed);

    // Set up listening sockets on NUM_PORTS
    for (int i = 0; i < NUM_PORTS; i++)
    {
        server_fds[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fds[i] < 0)
        {
            perror("socket");
            exit(EXIT_FAILURE);
        }
        setsockopt(server_fds[i], SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(BASE_PORT + i);
        if (bind(server_fds[i], (struct sockaddr *)&server_addr, addrlen) < 0)
        {
            perror("bind");
            exit(EXIT_FAILURE);
        }
        if (listen(server_fds[i], MAX_PLAYERS) < 0)
        {
            perror("listen");
            exit(EXIT_FAILURE);
        }
    }

    // JOIN phase: Accept connection for each expected player
    for (int p = 0; p < MAX_PLAYERS; p++)
    {
        int fd = accept(server_fds[p], NULL, NULL);
        if (fd < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        game.sockets[p] = fd;
        game.player_status[p] = PLAYER_ACTIVE;
    }
    // Receive JOIN packets from connected players
    server_join(&game);

    // Main game loop
    while (1)
    {
        print_game_state(&game);
        switch (game.round_stage)
        {
        case ROUND_INIT:
        {
            // Rotate dealer, reset bets/pot, shuffle deck, and verify active players
            int active = server_ready(&game);
            if (active < 2)
            {
                log_info("Not enough active players. Shutting down.");
                goto cleanup;
            }
            server_deal(&game);
            game.round_stage = ROUND_PREFLOP;
            break;
        }

        case ROUND_PREFLOP:
        case ROUND_FLOP:
        case ROUND_TURN:
        case ROUND_RIVER:
        {
            // For every active player, send an INFO packet
            for (int p = 0; p < MAX_PLAYERS; p++)
            {
                if (game.player_status[p] == PLAYER_LEFT)
                    continue;

                server_packet_t sp;
                build_info_packet(&game, p, &sp);
                info_packet_t *ip = &sp.info;

                // 1) pot / turn / dealer / bet_size
                log_info("[INFO] [INFO_PACKET] pot_size=%d, player_turn=%d, dealer=%d, bet_size=%d",
                         ip->pot_size, ip->player_turn, ip->dealer, ip->bet_size);

                // 2) your hole cards
                log_info("[INFO] [INFO_PACKET] Your Cards: %s %s",
                         card_name(ip->player_cards[0]),
                         card_name(ip->player_cards[1]));

                // 3) player information first (before community cards)
                for (int i = 0; i < MAX_PLAYERS; i++)
                {
                    log_info("[INFO] [INFO_PACKET] Player %d: stack=%d, bet=%d, status=%d",
                             i,
                             ip->player_stacks[i],
                             ip->player_bets[i],
                             ip->player_status[i]);
                }

                // 4) community cards last
                if (game.round_stage >= ROUND_FLOP)
                {
                    int num_comm = 0;
                    switch (game.round_stage)
                    {
                    case ROUND_FLOP:
                        num_comm = 3;
                        break;
                    case ROUND_TURN:
                        num_comm = 4;
                        break;
                    case ROUND_RIVER:
                        num_comm = 5;
                        break;
                    default:
                        break;
                    }

                    for (int i = 0; i < num_comm; i++)
                    {
                        log_info("[INFO] [INFO_PACKET] Community Card %d: %s",
                                 i,
                                 card_name(ip->community_cards[i]));
                    }
                }

                // Finally send the packet
                send(game.sockets[p], &sp, sizeof(sp), 0);
            }

            // Betting round uses server_bet which relies on check_betting_end internally
            int bet_result = server_bet(&game);
            if (bet_result != -1)
            {
                // Special condition (e.g. one player remaining) – directly move to showdown
                game.round_stage = ROUND_SHOWDOWN;
            }
            else
            {
                // Reveal next community card if available
                server_community(&game);
                if (game.round_stage < ROUND_RIVER)
                    game.round_stage++; // Advances: PREFLOP -> FLOP -> TURN -> RIVER
                else
                    game.round_stage = ROUND_SHOWDOWN;
            }
            break;
        }

        case ROUND_SHOWDOWN:
        {
            // Determine winner, announce results via server_end, and then reset for next hand
            server_end(&game);
            reset_game_state(&game);
            break;
        }

        default:
            log_info("Encountered invalid round stage. Shutting down.");
            goto cleanup;
        }
    }

cleanup:
    log_info("Cleaning up and shutting down server.");
    log_fini();
    for (int i = 0; i < NUM_PORTS; i++)
    {
        close(server_fds[i]);
    }
    for (int p = 0; p < MAX_PLAYERS; p++)
    {
        if (game.sockets[p] >= 0)
            close(game.sockets[p]);
    }
    return 0;
}