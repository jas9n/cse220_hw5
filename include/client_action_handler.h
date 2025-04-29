#ifndef CLIENT_ACTION_H
#define CLIENT_ACTION_H

#include "poker_client.h"
#include "game_logic.h"

int handle_client_action(game_state_t *game, player_id_t pid, const client_packet_t *in, server_packet_t *out);
void build_info_packet(game_state_t *game, player_id_t pid, server_packet_t *out);
void build_end_packet(game_state_t *game, player_id_t winner, server_packet_t *out);

#endif
