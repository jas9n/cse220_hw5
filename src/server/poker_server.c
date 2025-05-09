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

    // Create and bind sockets for each port
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

    // Accept connections on all ports
    int player_count = 0;
    for (int port = 0; port < NUM_PORTS; port++)
    {
        int fd = accept(server_fds[port], NULL, NULL);
        if (fd < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        game.sockets[player_count] = fd;
        game.player_status[player_count++] = PLAYER_ACTIVE;
        log_info("Player connected on port %d (socket %d).", BASE_PORT + port, fd);
    }

    game.num_players = player_count;

    // Log the number of active players
    int active_players = 0;
    for (int i = 0; i < game.num_players; i++)
    {
        if (game.player_status[i] == PLAYER_ACTIVE)
        {
            active_players++;
        }
    }
    log_info("Number of active players: %d", active_players);

    // Handle JOIN and READY phases
    server_join(&game);
    active_players = server_ready(&game);
    if (active_players < 2)
    {
        log_info("Not enough players to start the game. Shutting down.");
        goto cleanup;
    }

    // Main game loop
    while (1)
    {
        switch (game.round_stage)
        {
        case ROUND_INIT:
            active_players = server_ready(&game);
            if (active_players < 2)
            {
                log_info("Not enough active players. Shutting down.");
                goto cleanup;
            }
            server_deal(&game);
            game.round_stage = ROUND_PREFLOP;
            break;

        case ROUND_PREFLOP:
        case ROUND_FLOP:
        case ROUND_TURN:
        case ROUND_RIVER:
        {
            // Deal community cards if applicable
            if (game.round_stage == ROUND_FLOP)
            {
                game.community_cards[0] = game.deck[game.next_card++];
                game.community_cards[1] = game.deck[game.next_card++];
                game.community_cards[2] = game.deck[game.next_card++];
                log_info("Dealt FLOP: %s %s %s",
                         card_name(game.community_cards[0]),
                         card_name(game.community_cards[1]),
                         card_name(game.community_cards[2]));
            }
            else if (game.round_stage == ROUND_TURN)
            {
                game.community_cards[3] = game.deck[game.next_card++];
                log_info("Dealt TURN: %s", card_name(game.community_cards[3]));
            }
            else if (game.round_stage == ROUND_RIVER)
            {
                game.community_cards[4] = game.deck[game.next_card++];
                log_info("Dealt RIVER: %s", card_name(game.community_cards[4]));
            }

            // Send INFO packets to all active players
            for (int i = 0; i < game.num_players; i++)
            {
                if (game.player_status[i] == PLAYER_ACTIVE)
                {
                    server_packet_t info_pkt;
                    build_info_packet(&game, i, &info_pkt);
                    if (send(game.sockets[i], &info_pkt, sizeof(info_pkt), 0) > 0)
                    {
                        log_info("Sent INFO packet to player %d.", i);
                    }
                    else
                    {
                        log_err("Failed to send INFO packet to player %d.", i);
                    }
                }
            }

            // Handle betting round
            while (1)
            {
                server_packet_t info_pkt;
                build_info_packet(&game, game.current_player, &info_pkt);
                send(game.sockets[game.current_player], &info_pkt, sizeof(info_pkt), 0);

                client_packet_t in;
                ssize_t bytes = recv(game.sockets[game.current_player], &in, sizeof(in), 0);
                if (bytes <= 0)
                {
                    log_info("Player %d disconnected. Marked as LEFT.", game.current_player);
                    game.player_status[game.current_player] = PLAYER_LEFT;
                    close(game.sockets[game.current_player]);
                    game.sockets[game.current_player] = -1;
                    continue;
                }

                server_packet_t resp;
                if (handle_client_action(&game, game.current_player, &in, &resp) == 0)
                {
                    send(game.sockets[game.current_player], &resp, sizeof(resp), 0);
                }
                else
                {
                    resp.packet_type = NACK;
                    send(game.sockets[game.current_player], &resp, sizeof(resp), 0);
                    continue;
                }

                log_info("Game state after action:");
                log_info("Pot size: %d", game.pot_size);
                log_info("Highest bet: %d", game.highest_bet);
                for (int i = 0; i < game.num_players; i++)
                {
                    log_info("Player %d: stack=%d, bet=%d, status=%d",
                             i, game.player_stacks[i], game.current_bets[i], game.player_status[i]);
                }

                int active_players = 0;
                int all_bets_equal = 1;
                for (int i = 0; i < game.num_players; i++)
                {
                    if (game.player_status[i] == PLAYER_ACTIVE)
                    {
                        active_players++;
                        if (game.current_bets[i] != game.highest_bet)
                        {
                            all_bets_equal = 0;
                        }
                    }
                }

                if (active_players <= 1)
                {
                    game.round_stage = ROUND_SHOWDOWN;
                    break;
                }

                if (all_bets_equal)
                {
                    break;
                }

                do
                {
                    game.current_player = (game.current_player + 1) % game.num_players;
                } while (game.player_status[game.current_player] != PLAYER_ACTIVE);
                log_info("Next player turn: %d", game.current_player);
            }

            game.round_stage++;
            break;
        }

        case ROUND_SHOWDOWN:
            server_end(&game);

            int ready_count = 0;
            int ready[MAX_PLAYERS] = {0};
            while (ready_count < game.num_players)
            {
                for (int i = 0; i < game.num_players; i++)
                {
                    if (game.player_status[i] == PLAYER_LEFT || ready[i])
                        continue;

                    client_packet_t in;
                    ssize_t bytes = recv(game.sockets[i], &in, sizeof(in), MSG_DONTWAIT);
                    if (bytes > 0)
                    {
                        if (in.packet_type == READY)
                        {
                            ready[i] = 1;
                            ready_count++;
                            log_info("Player %d is READY.", i);
                        }
                        else if (in.packet_type == LEAVE)
                        {
                            game.player_status[i] = PLAYER_LEFT;
                            ready[i] = 1;
                            ready_count++;
                            log_info("Player %d has LEFT.", i);
                            close(game.sockets[i]);
                            game.sockets[i] = -1;
                        }
                    }
                }
            }

            int active_players = 0;
            for (int i = 0; i < game.num_players; i++)
            {
                if (game.player_status[i] == PLAYER_ACTIVE)
                {
                    active_players++;
                }
            }

            if (active_players < 2)
            {
                for (int i = 0; i < game.num_players; i++)
                {
                    if (game.player_status[i] == PLAYER_ACTIVE)
                    {
                        server_packet_t halt_pkt;
                        memset(&halt_pkt, 0, sizeof(halt_pkt));
                        halt_pkt.packet_type = HALT;
                        send(game.sockets[i], &halt_pkt, sizeof(halt_pkt), 0);
                        close(game.sockets[i]);
                        game.sockets[i] = -1;
                        game.player_status[i] = PLAYER_LEFT;
                    }
                }
                goto cleanup;
            }

            reset_game_state(&game);
            game.round_stage = ROUND_INIT;
            break;

        default:
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