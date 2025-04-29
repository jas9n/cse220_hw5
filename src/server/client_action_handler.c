#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "client_action_handler.h"
#include "game_logic.h"

/**
 * @brief Processes packet from client and generates a server response packet.
 * 
 * If the action is valid, a SERVER_INFO packet will be generated containing the updated game state.
 * If the action is invalid or out of turn, a SERVER_NACK packet is returned with an optional error message.
 * 
 * @param pid The ID of the client/player who sent the packet.
 * @param in Pointer to the client_packet_t received from the client.
 * @param out Pointer to a server_packet_t that will be filled with the response.
 * @return 0 if successful processing, -1 on NACK or error.
 */
int handle_client_action(game_state_t *game, player_id_t pid, const client_packet_t *in, server_packet_t *out) {
    //Optional function, see documentation above. Strongly reccomended.
    (void)game;
    (void)pid;
    (void)in;
    (void)out;
    return -1;
}

void build_info_packet(game_state_t *game, player_id_t pid, server_packet_t *out) {
    //Put state info from "game" (for player pid) into packet "out"
    (void) game;
    (void) pid;
    (void) out;
}

void build_end_packet(game_state_t *game, player_id_t winner, server_packet_t *out) {
    //Put state info from "game" (and calculate winner) into packet "out"
    (void) game;
    (void) winner;
    (void) out;
}
