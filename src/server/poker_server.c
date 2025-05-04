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

    // Initialize game state
    int seed = (argc >= 2) ? atoi(argv[1]) : (int)time(NULL);
    init_game_state(&game, 1000, seed);

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

    // JOIN stage: accept JOIN packets from all players
    if (game.round_stage == ROUND_JOIN)
    {
        client_packet_t pkt;
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
            // Expect a JOIN packet; consider checking the return value of recv
            if (recv(fd, &pkt, sizeof(pkt), 0) <= 0)
            {
                perror("recv");
                exit(EXIT_FAILURE);
            }
            log_info("[INFO] [Client ~> Server] Received packet: type=JOIN");
        }
        server_join(&game);
    }

    // Main game loop
    while (1)
    {
        print_game_state(&game);
        switch (game.round_stage)
        {
        case ROUND_INIT:
        {
            int ready = server_ready(&game);
            if (ready < 2)
            {
                log_info("Not enough players to start");
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
            // Send updated info packets to all active players
            for (int p = 0; p < MAX_PLAYERS; p++)
            {
                if (game.player_status[p] == PLAYER_LEFT)
                    continue;
                server_packet_t sp;
                build_info_packet(&game, p, &sp);
                info_packet_t *ip = &sp.info;
                log_info("[INFO] [INFO_PACKET] pot_size=%d, player_turn=%d, dealer=%d, bet_size=%d",
                         ip->pot_size, ip->player_turn, ip->dealer, ip->bet_size);
                log_info("[INFO] [INFO_PACKET] Your Cards: %s %s",
                         card_name(ip->player_cards[0]),
                         card_name(ip->player_cards[1]));
                for (int j = 0; j < MAX_PLAYERS; j++)
                {
                    log_info("[INFO] [INFO_PACKET] Player %d: stack=%d, bet=%d, status=%d",
                             j, ip->player_stacks[j], ip->player_bets[j], ip->player_status[j]);
                }
                for (int j = 0; j < MAX_COMMUNITY_CARDS; j++)
                {
                    log_info("[INFO] [INFO_PACKET] Community Card %d: %s",
                             j, card_name(ip->community_cards[j]));
                }
                send(game.sockets[p], &sp, sizeof(sp), 0);
            }

            // Run the betting round; if server_bet returns != -1, jump to showdown
            int lone = server_bet(&game);
            if (lone != -1)
                game.round_stage = ROUND_SHOWDOWN;
            else
            {
                server_community(&game);
                if (game.round_stage < ROUND_RIVER)
                    game.round_stage++;
                else
                    game.round_stage = ROUND_SHOWDOWN;
            }
            break;
        }

        case ROUND_SHOWDOWN:
        {
            int winner = find_winner(&game);
            for (int p = 0; p < MAX_PLAYERS; p++)
            {
                if (game.player_status[p] == PLAYER_LEFT)
                    continue;
                server_packet_t sp;
                build_end_packet(&game, winner, &sp);
                end_packet_t *ep = &sp.end;
                log_info("[INFO] [END_PACKET] pot_size=%d, winner=%d, dealer=%d",
                         ep->pot_size, ep->winner, ep->dealer);
                for (int j = 0; j < MAX_PLAYERS; j++)
                {
                    log_info("[INFO] [END_PACKET] Player %d Final Stack=%d, Cards: %s %s",
                             j, ep->player_stacks[j],
                             card_name(ep->player_cards[j][0]),
                             card_name(ep->player_cards[j][1]));
                }
                send(game.sockets[p], &sp, sizeof(sp), 0);
            }
            reset_game_state(&game);
            game.round_stage = ROUND_INIT;
            break;
        }

        default:
            fprintf(stderr, "Invalid game stage %d\n", game.round_stage);
            goto cleanup;
        }
    }

cleanup:
    log_info("Cleaning up and shutting down server");
    log_fini();
    for (int i = 0; i < NUM_PORTS; i++)
        close(server_fds[i]);
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (game.sockets[i] >= 0)
            close(game.sockets[i]);
    return 0;
}