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
int handle_client_action(game_state_t *game, player_id_t pid, const client_packet_t *in, server_packet_t *out)
{
    // Optional function, see documentation above. Strongly reccomended.
    // Ensure it's this player's turn
    if (!game || !in || !out || pid >= MAX_PLAYERS || pid < 0)
    {
        if (out)
            out->packet_type = NACK;
        return -1;
    }

    if (pid != game->current_player)
    {
        out->packet_type = NACK;
        return -1;
    }

    memset(out, 0, sizeof(server_packet_t));

    switch (in->packet_type)
    {
    case CALL:
    {
        int to_call = game->highest_bet - game->current_bets[pid];
        if (to_call < 0)
            to_call = 0;
        if (game->player_stacks[pid] < to_call)
        {
            out->packet_type = NACK;
            return -1;
        }
        game->player_stacks[pid] -= to_call;
        game->current_bets[pid] += to_call;
        game->pot_size += to_call;
        out->packet_type = ACK;
        break;
    }
    case CHECK:
    {
        if (game->current_bets[pid] != game->highest_bet)
        {
            out->packet_type = NACK;
            return -1;
        }
        out->packet_type = ACK;
        break;
    }
    case RAISE:
    {
        int raise_amt = in->params[0];
        int to_call = game->highest_bet - game->current_bets[pid] + raise_amt;
        if (game->player_stacks[pid] < to_call)
        {
            out->packet_type = NACK;
            return -1;
        }
        game->player_stacks[pid] -= to_call;
        game->current_bets[pid] += to_call;
        game->highest_bet += raise_amt;
        game->pot_size += to_call;
        out->packet_type = ACK;
        break;
    }
    case FOLD:
    {
        game->player_status[pid] = PLAYER_FOLDED;
        out->packet_type = ACK;
        break;
    }
    default:
        out->packet_type = NACK;
        return -1;
    }
    return 0;
}

void build_info_packet(game_state_t *game, player_id_t pid, server_packet_t *out)
{
    // Put state info from "game" (for player pid) into packet "out"
    memset(out, 0, sizeof(server_packet_t));
    out->packet_type = INFO;
    info_packet_t *info = &out->info;

    // Hole cards
    for (int i = 0; i < HAND_SIZE; i++)
    {
        info->player_cards[i] = game->player_hands[pid][i];
    }

    // Community cards
    int num_comm = 0;
    switch (game->round_stage)
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
        num_comm = 0;
        break;
    }
    for (int i = 0; i < MAX_COMMUNITY_CARDS; i++)
    {
        if (i < num_comm)
        {
            info->community_cards[i] = game->community_cards[i];
        }
        else
        {
            info->community_cards[i] = NOCARD;
        }
    }

    // Stacks, bets, statuses
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        info->player_stacks[i] = game->player_stacks[i];
        info->player_bets[i] = game->current_bets[i];
        info->player_status[i] = game->player_status[i];
    }

    info->pot_size = game->pot_size;
    info->dealer = game->dealer_player;
    info->player_turn = game->current_player;
    info->bet_size = game->highest_bet;
}

void build_end_packet(game_state_t *game, player_id_t winner, server_packet_t *out)
{
    // Put state info from "game" (and calculate winner) into packet "out"
    memset(out, 0, sizeof(server_packet_t));
    out->packet_type = END;
    end_packet_t *endp = &out->end;

    // Final cards & statuses for all players
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        for (int j = 0; j < HAND_SIZE; j++)
        {
            endp->player_cards[i][j] = game->player_hands[i][j];
        }
        endp->player_stacks[i] = game->player_stacks[i];
        endp->player_status[i] = game->player_status[i];
    }

    // Community cards
    for (int i = 0; i < MAX_COMMUNITY_CARDS; i++)
    {
        endp->community_cards[i] = game->community_cards[i];
    }

    endp->pot_size = game->pot_size;
    endp->dealer = game->dealer_player;
    endp->winner = winner;
}
