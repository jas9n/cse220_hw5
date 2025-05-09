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

    // socket
    for (int i = 0; i < NUM_PORTS; i++)
    {
        server_fds[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fds[i] < 0)
        {
            perror("socket");
            exit(EXIT_FAILURE);
        }
        setsockopt(server_fds[i], SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        // bind
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(BASE_PORT + i);
        if (bind(server_fds[i], (struct sockaddr *)&server_addr, addrlen) < 0)
        {
            perror("bind");
            exit(EXIT_FAILURE);
        }

        // listen
        if (listen(server_fds[i], MAX_PLAYERS) < 0)
        {
            perror("listen");
            exit(EXIT_FAILURE);
        }

        // accept
        int fd = accept(server_fds[i], NULL, NULL);
        if (fd < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        game.sockets[i] = fd;
        game.player_status[i] = PLAYER_ACTIVE;
    }

    // Use server_join to handle connections and READY/LEAVE packets
    server_join(&game);

    int active_players = server_ready(&game);
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
        {
            // Rotate dealer, reset bets/pot, shuffle deck, and verify active players
            active_players = server_ready(&game);
            if (active_players < 2)
            {
                log_info("Not enough active players. Shutting down.");
                goto cleanup;
            }
            server_deal(&game);
            game.round_stage = ROUND_PREFLOP;
            break;
        }

        case ROUND_PREFLOP:
        {
            // Send INFO packets to all active players
            for (int i = 0; i < game.num_players; i++)
            {
                if (game.player_status[i] == PLAYER_ACTIVE)
                {
                    server_packet_t info_pkt;
                    build_info_packet(&game, i, &info_pkt);
                    send(game.sockets[i], &info_pkt, sizeof(info_pkt), 0);
                    log_info("Sent INFO packet to player %d.", i);
                }
            }

            // Handle betting round
            while (1)
            {
                // Send INFO packet to the current player
                server_packet_t info_pkt;
                build_info_packet(&game, game.current_player, &info_pkt);
                send(game.sockets[game.current_player], &info_pkt, sizeof(info_pkt), 0);

                // Receive action from the current player
                client_packet_t in;
                ssize_t bytes = recv(game.sockets[game.current_player], &in, sizeof(in), 0);
                if (bytes <= 0)
                {
                    // Treat disconnection as a fold
                    log_info("Player %d disconnected. Marked as LEFT.", game.current_player);
                    game.player_status[game.current_player] = PLAYER_LEFT;
                    close(game.sockets[game.current_player]);
                    game.sockets[game.current_player] = -1;
                    continue;
                }

                // Process the player's action
                server_packet_t resp;
                if (handle_client_action(&game, game.current_player, &in, &resp) == 0)
                {
                    send(game.sockets[game.current_player], &resp, sizeof(resp), 0);
                }
                else
                {
                    resp.packet_type = NACK;
                    send(game.sockets[game.current_player], &resp, sizeof(resp), 0);
                    continue; // Invalid action, don't move to the next player
                }

                // Check if the betting round is over
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
                    break; // Betting round is over
                }

                // Move to the next active player
                do
                {
                    game.current_player = (game.current_player + 1) % game.num_players;
                } while (game.player_status[game.current_player] != PLAYER_ACTIVE);
            }

            // Advance to the next round stage
            game.round_stage = ROUND_FLOP;
            break;
        }

        case ROUND_FLOP:
        {
            // Deal 3 community cards
            deal_community_cards(&game, 3);

            // Send INFO packets to all active players
            for (int i = 0; i < game.num_players; i++)
            {
                if (game.player_status[i] == PLAYER_ACTIVE)
                {
                    server_packet_t info_pkt;
                    build_info_packet(&game, i, &info_pkt);
                    send(game.sockets[i], &info_pkt, sizeof(info_pkt), 0);
                    log_info("Sent INFO packet to player %d.", i);
                }
            }

            // Handle betting round
            while (1)
            {
                // Send INFO packet to the current player
                server_packet_t info_pkt;
                build_info_packet(&game, game.current_player, &info_pkt);
                send(game.sockets[game.current_player], &info_pkt, sizeof(info_pkt), 0);

                // Receive action from the current player
                client_packet_t in;
                ssize_t bytes = recv(game.sockets[game.current_player], &in, sizeof(in), 0);
                if (bytes <= 0)
                {
                    // Treat disconnection as a fold
                    log_info("Player %d disconnected. Marked as LEFT.", game.current_player);
                    game.player_status[game.current_player] = PLAYER_LEFT;
                    close(game.sockets[game.current_player]);
                    game.sockets[game.current_player] = -1;
                    continue;
                }

                // Process the player's action
                server_packet_t resp;
                if (handle_client_action(&game, game.current_player, &in, &resp) == 0)
                {
                    send(game.sockets[game.current_player], &resp, sizeof(resp), 0);
                }
                else
                {
                    resp.packet_type = NACK;
                    send(game.sockets[game.current_player], &resp, sizeof(resp), 0);
                    continue; // Invalid action, don't move to the next player
                }

                // Check if the betting round is over
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
                    break; // Betting round is over
                }

                // Move to the next active player
                do
                {
                    game.current_player = (game.current_player + 1) % game.num_players;
                } while (game.player_status[game.current_player] != PLAYER_ACTIVE);
            }

            // Advance to the next round stage
            game.round_stage = ROUND_TURN;
            break;
        }

        case ROUND_TURN:
        {
            // Deal 1 community card
            deal_community_cards(&game, 1);

            // Send INFO packets to all active players
            for (int i = 0; i < game.num_players; i++)
            {
                if (game.player_status[i] == PLAYER_ACTIVE)
                {
                    server_packet_t info_pkt;
                    build_info_packet(&game, i, &info_pkt);
                    send(game.sockets[i], &info_pkt, sizeof(info_pkt), 0);
                    log_info("Sent INFO packet to player %d.", i);
                }
            }

            // Handle betting round
            while (1)
            {
                // Send INFO packet to the current player
                server_packet_t info_pkt;
                build_info_packet(&game, game.current_player, &info_pkt);
                send(game.sockets[game.current_player], &info_pkt, sizeof(info_pkt), 0);

                // Receive action from the current player
                client_packet_t in;
                ssize_t bytes = recv(game.sockets[game.current_player], &in, sizeof(in), 0);
                if (bytes <= 0)
                {
                    // Treat disconnection as a fold
                    log_info("Player %d disconnected. Marked as LEFT.", game.current_player);
                    game.player_status[game.current_player] = PLAYER_LEFT;
                    close(game.sockets[game.current_player]);
                    game.sockets[game.current_player] = -1;
                    continue;
                }

                // Process the player's action
                server_packet_t resp;
                if (handle_client_action(&game, game.current_player, &in, &resp) == 0)
                {
                    send(game.sockets[game.current_player], &resp, sizeof(resp), 0);
                }
                else
                {
                    resp.packet_type = NACK;
                    send(game.sockets[game.current_player], &resp, sizeof(resp), 0);
                    continue; // Invalid action, don't move to the next player
                }

                // Check if the betting round is over
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
                    break; // Betting round is over
                }

                // Move to the next active player
                do
                {
                    game.current_player = (game.current_player + 1) % game.num_players;
                } while (game.player_status[game.current_player] != PLAYER_ACTIVE);
            }

            // Advance to the next round stage
            game.round_stage = ROUND_RIVER;
            break;
        }

        case ROUND_RIVER:
        {
            // Deal 1 community card
            deal_community_cards(&game, 1);

            // Send INFO packets to all active players
            for (int i = 0; i < game.num_players; i++)
            {
                if (game.player_status[i] == PLAYER_ACTIVE)
                {
                    server_packet_t info_pkt;
                    build_info_packet(&game, i, &info_pkt);
                    send(game.sockets[i], &info_pkt, sizeof(info_pkt), 0);
                    log_info("Sent INFO packet to player %d.", i);
                }
            }

            // Handle betting round
            while (1)
            {
                // Send INFO packet to the current player
                server_packet_t info_pkt;
                build_info_packet(&game, game.current_player, &info_pkt);
                send(game.sockets[game.current_player], &info_pkt, sizeof(info_pkt), 0);

                // Receive action from the current player
                client_packet_t in;
                ssize_t bytes = recv(game.sockets[game.current_player], &in, sizeof(in), 0);
                if (bytes <= 0)
                {
                    // Treat disconnection as a fold
                    log_info("Player %d disconnected. Marked as LEFT.", game.current_player);
                    game.player_status[game.current_player] = PLAYER_LEFT;
                    close(game.sockets[game.current_player]);
                    game.sockets[game.current_player] = -1;
                    continue;
                }

                // Process the player's action
                server_packet_t resp;
                if (handle_client_action(&game, game.current_player, &in, &resp) == 0)
                {
                    send(game.sockets[game.current_player], &resp, sizeof(resp), 0);
                }
                else
                {
                    resp.packet_type = NACK;
                    send(game.sockets[game.current_player], &resp, sizeof(resp), 0);
                    continue; // Invalid action, don't move to the next player
                }

                // Check if the betting round is over
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
                    break; // Betting round is over
                }

                // Move to the next active player
                do
                {
                    game.current_player = (game.current_player + 1) % game.num_players;
                } while (game.player_status[game.current_player] != PLAYER_ACTIVE);
            }

            // Advance to the next round stage
            game.round_stage = ROUND_SHOWDOWN;
            break;
        }

        case ROUND_SHOWDOWN:
        {
            // Call server_end to determine the winner and send END packets
            server_end(&game);

            // Wait for READY or LEAVE packets
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

                            // Disconnect the socket for the player who left
                            close(game.sockets[i]);
                            game.sockets[i] = -1;
                            log_info("Closed socket for player %d.", i);
                        }
                    }
                }
            }

            // Check if there are enough players to continue
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
                // If only one player is READY, send them a HALT packet and close the connection
                for (int i = 0; i < game.num_players; i++)
                {
                    if (game.player_status[i] == PLAYER_ACTIVE)
                    {
                        server_packet_t halt_pkt;
                        memset(&halt_pkt, 0, sizeof(halt_pkt));
                        halt_pkt.packet_type = HALT;

                        send(game.sockets[i], &halt_pkt, sizeof(halt_pkt), 0);
                        log_info("Sent HALT packet to player %d.", i);

                        // Close the connection for the remaining player
                        close(game.sockets[i]);
                        game.sockets[i] = -1;
                        game.player_status[i] = PLAYER_LEFT;
                        log_info("Closed connection for player %d.", i);
                    }
                }

                log_info("Not enough players to continue. Shutting down.");
                goto cleanup; // HALT state
            }

            // Reset the game state and go to DEALING
            reset_game_state(&game);
            game.round_stage = ROUND_INIT;
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