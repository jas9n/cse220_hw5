#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "poker_client.h"
#include "utility.h"
#include "logs.h"

#define SERVER_IP   "127.0.0.1"
#define BASE_PORT 2201
#define NUM_PORTS 6
#define BUFFER_SIZE 1024

// Static vars
static int client_fd = -1;
static info_packet_handler_t info_handler = NULL;
static end_packet_handler_t end_handler = NULL;
static on_halt_packet_handler_t halt_handler = NULL;
static server_packet_t last_server_packet;
static int halt_received = 0;

static const char *CLIENT_PACKET_TYPE_NAMES[] = {
    "JOIN",
    "LEAVE",
    "READY",
    "RAISE",
    "CALL",
    "CHECK",
    "FOLD"
};

static const char *SERVER_PACKET_TYPE_NAMES[] = {
    "ACK",
    "NACK",
    "INFO",
    "END",
    "HALT"
};

// ---------------------------- Logging Functions ---------------------------- //

void log_info_packet(const info_packet_t *info) {
    if (!info) return;

    log_info("[INFO_PACKET] pot_size=%d, player_turn=%d, dealer=%d, bet_size=%d", 
             info->pot_size, info->player_turn, info->dealer, info->bet_size);

    log_info("[INFO_PACKET] Your Cards: %s %s", 
             card_name(info->player_cards[0]), 
             card_name(info->player_cards[1]));

    for (int i = 0; i < 5; i++) {
        if (info->community_cards[i] != NOCARD) {
            log_info("[INFO_PACKET] Community Card %d: %s", i, card_name(info->community_cards[i]));
        }
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        log_info("[INFO_PACKET] Player %d: stack=%d, bet=%d, status=%d", 
                 i, info->player_stacks[i], info->player_bets[i], info->player_status[i]);
    }
}

void log_end_packet(const end_packet_t *end) {
    if (!end) return;

    log_info("[END_PACKET] pot_size=%d, winner=%d, dealer=%d", 
             end->pot_size, end->winner, end->dealer);

    for (int i = 0; i < 5; i++) {
        if (end->community_cards[i] != NOCARD) {
            log_info("[END_PACKET] Community Card %d: %s", i, card_name(end->community_cards[i]));
        }
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        log_info("[END_PACKET] Player %d Final Stack=%d, Cards: %s %s", 
                 i, end->player_stacks[i],
                 card_name(end->player_cards[i][0]),
                 card_name(end->player_cards[i][1]));
    }
}

// ---------------------------- Networking Functions ---------------------------- //

#define NANOSEC_IN_SEC 1000000000ul
#define MAX_CONNECTION_ATTEMPT_TIME 7500000000ul

int connect_to_serv(player_id_t player_id) {
    struct sockaddr_in serv_addr;

    int port = BASE_PORT + player_id;

    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        log_err("socket failed in connect_to_serv");
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        log_err("inet_pton failed in connect_to_serv");
        close(client_fd);
        client_fd = -1;
        return -1;
    }

    int connection_success = 0;
    int attempt_num = 0;
    struct timespec tm;
    for (size_t timer = 100000000; timer < MAX_CONNECTION_ATTEMPT_TIME; timer *= 2)
    {
        if (connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) >= 0) 
        {
            connection_success = 1;
            break;
        }

        fprintf(stderr, "Failed to connect (Attempt #%d)\n", attempt_num++);
        tm.tv_sec = timer / NANOSEC_IN_SEC;
        tm.tv_nsec = timer % NANOSEC_IN_SEC;
        nanosleep(&tm, NULL);
    }
    

    if (!connection_success) {
        log_err("connect failed in connect_to_serv");
        close(client_fd);
        client_fd = -1;
        return -1;
    }

    log_info("[Client] Successfully connected to server at %s:%d", SERVER_IP, port);

    client_packet_t pkt = { 0 };
    pkt.packet_type = JOIN;

    log_info("[Client ~> Server] Sending packet: type=%s", CLIENT_PACKET_TYPE_NAMES[pkt.packet_type]);

    if (send(client_fd, &pkt, sizeof(client_packet_t), 0) <= 0) {
        log_err("send failed in join.");
        return -1;
    }

    return 0;
}

int disconnect_to_serv() {
    if (client_fd >= 0) {
        close(client_fd);
        client_fd = -1;
        return 0;
    }
    return -1;
}

int send_packet(client_packet_t *pkt) {
    if (!pkt || client_fd < 0) return -1;

    if (pkt->packet_type == RAISE)
        log_info("[Client ~> Server] Sending packet: type=%s, param[0]=%d", CLIENT_PACKET_TYPE_NAMES[pkt->packet_type], pkt->params[0]);
    else
        log_info("[Client ~> Server] Sending packet: type=%s", CLIENT_PACKET_TYPE_NAMES[pkt->packet_type]);

    if (send(client_fd, pkt, sizeof(client_packet_t), 0) <= 0) {
        log_err("send failed in send_packet");
        return -1;
    }

    if (pkt->packet_type == READY || pkt->packet_type == LEAVE) {
        return 0;
    }

    server_packet_t response;
    if (recv(client_fd, &response, sizeof(server_packet_t), 0) <= 0) {
        log_err("recv failed after sending packet");
        return -1;
    }

    log_info("[Server ~> Client] Received response packet: type=%s", SERVER_PACKET_TYPE_NAMES[response.packet_type]);

    return (response.packet_type == ACK) ? 0 : -1;
}

int recv_packet(server_packet_t *pkt) {
    if (!pkt || client_fd < 0) return -1;

    if (recv(client_fd, pkt, sizeof(server_packet_t), 0) <= 0) {
        log_err("recv failed in recv_packet");
        return -1;
    }

    memcpy(&last_server_packet, pkt, sizeof(server_packet_t));

    switch (pkt->packet_type) {
        case INFO:
            log_info_packet(&(pkt->info));
            if (info_handler) {
                info_handler(&(pkt->info));
            }
            break;
        case END:
            log_end_packet(&(pkt->end));
            if (end_handler) {
                end_handler(&(pkt->end));
            }
            break;
        case HALT:
            halt_received = 1;
            log_info("[Server ~> Client] Received HALT");
            if (halt_handler) {
                halt_handler();
            }
            break;
        case ACK:
            log_info("[Server ~> Client] Received ACK");
            break;
        case NACK:
            log_info("[Server ~> Client] Received NACK");
            break;
        default:
            log_info("[Server ~> Client] Received unknown packet type: %d", pkt->packet_type);
            break;
    }

    return 0;
}

// ---------------------------- Info Packet Handler ---------------------------- //

void set_on_info_packet_handler(info_packet_handler_t handler) {
    info_handler = handler;
}

void set_on_end_packet_handler(end_packet_handler_t handler) {
    end_handler = handler;
}

void set_on_halt_packet_handler(on_halt_packet_handler_t handler) {
    halt_handler = handler;
}

// ------------------------- Poker move functions --------------------------- //

int ready() {
    client_packet_t pkt = { .packet_type = READY };
    return send_packet(&pkt);
}

int check() {
    client_packet_t pkt = { .packet_type = CHECK };
    return send_packet(&pkt);
}

int bet_raise(int new_bet) {
    client_packet_t pkt = { .packet_type = RAISE };
    pkt.params[0] = new_bet;
    return send_packet(&pkt);
}

int call() {
    client_packet_t pkt = { .packet_type = CALL };
    return send_packet(&pkt);
}

int fold() {
    client_packet_t pkt = { .packet_type = FOLD };
    return send_packet(&pkt);
}

int leave() {
    client_packet_t pkt = { .packet_type = LEAVE };
    return send_packet(&pkt);
}

// --------------------------- Utility functions ------------------------------- //

int is_players_turn(player_id_t player_id) {
    if (last_server_packet.packet_type != INFO) {
        return 0;
    }
    return (last_server_packet.info.player_turn == player_id);
}

int has_recv_halt() {
    return halt_received;
}