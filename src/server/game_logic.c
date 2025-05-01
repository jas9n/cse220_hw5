#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "poker_client.h"
#include "client_action_handler.h"
#include "game_logic.h"

// Feel free to add your own code. I stripped out most of our solution functions but I left some "breadcrumbs" for anyone lost

// for debugging
// void print_game_state(game_state_t *game)
// {
//     (void)game;
// }

void init_deck(card_t deck[DECK_SIZE], int seed)
{ // DO NOT TOUCH THIS FUNCTION
    srand(seed);
    int i = 0;
    for (int r = 0; r < 13; r++)
    {
        for (int s = 0; s < 4; s++)
        {
            deck[i++] = (r << SUITE_BITS) | s;
        }
    }
}

void shuffle_deck(card_t deck[DECK_SIZE])
{ // DO NOT TOUCH THIS FUNCTION
    for (int i = 0; i < DECK_SIZE; i++)
    {
        int j = rand() % DECK_SIZE;
        card_t temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
}

// You dont need to use this if you dont want, but we did.
void init_game_state(game_state_t *game, int starting_stack, int random_seed)
{
    memset(game, 0, sizeof(game_state_t));
    init_deck(game->deck, random_seed);
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        game->player_stacks[i] = starting_stack;
        game->player_status[i] = PLAYER_ACTIVE;
    }
    game->num_players = MAX_PLAYERS;
    game->next_card = 0;
    game->highest_bet = 0;
    game->pot_size = 0;
    game->current_player = 0;
    game->dealer_player = MAX_PLAYERS - 1;
    game->round_stage = ROUND_JOIN;
}

void reset_game_state(game_state_t *game)
{
    shuffle_deck(game->deck);
    // Call this function between hands.
    // You should add your own code, I just wanted to make sure the deck got shuffled.
    game->next_card = 0;
    memset(game->current_bets, 0, sizeof(game->current_bets));
    game->highest_bet = 0;
    game->pot_size = 0;
    game->round_stage = ROUND_INIT;

    // Reset folded players to active for next hand
    for (int i = 0; i < game->num_players; i++)
    {
        if (game->player_status[i] == PLAYER_FOLDED)
        {
            game->player_status[i] = PLAYER_ACTIVE;
        }
    }
}

void server_join(game_state_t *game)
{
    // This function was called to get the join packets from all players
    client_packet_t in;
    for (int i = 0; i < game->num_players; i++)
    {
        ssize_t bytes = recv(game->sockets[i], &in, sizeof(in), 0);
        if (bytes <= 0 || in.packet_type != JOIN)
        {
            // Mark player as left on error
            game->player_status[i] = PLAYER_LEFT;
        }
    }
    game->round_stage = ROUND_INIT;
}

int server_ready(game_state_t *game)
{
    // This function updated the dealer and checked ready/leave status for all players

    // Rotate dealer
    game->dealer_player = (game->dealer_player + 1) % game->num_players;

    // Count active players
    int ready_count = 0;
    for (int i = 0; i < game->num_players; i++)
    {
        if (game->player_status[i] == PLAYER_ACTIVE)
        {
            ready_count++;
        }
    }

    // Reset bets and pot
    memset(game->current_bets, 0, sizeof(game->current_bets));
    game->highest_bet = 0;
    game->pot_size = 0;

    // Shuffle deck and reset draw index
    shuffle_deck(game->deck);
    game->next_card = 0;

    // Advance to dealing stage
    game->round_stage = ROUND_PREFLOP;
    game->current_player = (game->dealer_player + 1) % game->num_players;
    return ready_count;
}

// This was our dealing function with some of the code removed (I left the dealing so we have the same logic)
void server_deal(game_state_t *game)
{
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (game->player_status[i] == PLAYER_ACTIVE)
        {
            game->player_hands[i][0] = game->deck[game->next_card++];
            game->player_hands[i][1] = game->deck[game->next_card++];
        }
    }
}

int server_bet(game_state_t *game)
{
    // This was our function to determine if everyone has called or folded

    int last_winner = -1;
    while (!check_betting_end(game))
    {
        // Send INFO
        server_packet_t info_pkt;
        build_info_packet(game, game->current_player, &info_pkt);
        send(game->sockets[game->current_player], &info_pkt, sizeof(info_pkt), 0);

        // Receive action
        client_packet_t in;
        recv(game->sockets[game->current_player], &in, sizeof(in), 0);
        server_packet_t resp;
        if (handle_client_action(game, game->current_player, &in, &resp) == 0)
        {
            send(game->sockets[game->current_player], &resp, sizeof(resp), 0);
        }
        else
        {
            // Invalid action: send NACK and retry
            send(game->sockets[game->current_player], &resp, sizeof(resp), 0);
            continue;
        }

        // Advance to next active
        int next = (game->current_player + 1) % game->num_players;
        while (game->player_status[next] != PLAYER_ACTIVE)
        {
            next = (next + 1) % game->num_players;
        }
        game->current_player = next;

        // Check for single remaining
        int active_count = 0;
        for (int i = 0; i < game->num_players; i++)
        {
            if (game->player_status[i] == PLAYER_ACTIVE)
            {
                active_count++;
                last_winner = i;
            }
        }
        if (active_count == 1)
        {
            return last_winner;
        }
    }
    return -1;
}

// Returns 1 if all bets are the same among active players
int check_betting_end(game_state_t *game)
{
    for (int i = 0; i < game->num_players; i++)
    {
        if (game->player_status[i] == PLAYER_ACTIVE &&
            game->current_bets[i] != game->highest_bet)
        {
            return 0;
        }
    }
    return 1;
}

void server_community(game_state_t *game)
{
    // This function checked the game state and dealt new community cards if needed;
    switch (game->round_stage)
    {
    case ROUND_PREFLOP:
        // flop: 3 cards
        game->community_cards[0] = game->deck[game->next_card++];
        game->community_cards[1] = game->deck[game->next_card++];
        game->community_cards[2] = game->deck[game->next_card++];
        game->round_stage = ROUND_FLOP;
        break;
    case ROUND_FLOP:
        // turn: 1 card
        game->community_cards[3] = game->deck[game->next_card++];
        game->round_stage = ROUND_TURN;
        break;
    case ROUND_TURN:
        // river: 1 card
        game->community_cards[4] = game->deck[game->next_card++];
        game->round_stage = ROUND_RIVER;
        break;
    default:
        break;
    }
}

void server_end(game_state_t *game)
{
    // This function sends the end packet
    int winner = find_winner(game);
    server_packet_t end_pkt;
    for (int i = 0; i < game->num_players; i++)
    {
        build_end_packet(game, winner, &end_pkt);
        send(game->sockets[i], &end_pkt, sizeof(end_pkt), 0);
    }
}

int evaluate_hand(game_state_t *game, player_id_t pid)
{
    // We wrote a function to compare a "value" for each players hand (to make comparison easier)
    // Feel free to not do this.
    (void)game;
    (void)pid;
    return 0;
}

int find_winner(game_state_t *game)
{
    // We wrote this function that looks at the game state and returns the player id for the best 5 card hand.
    int best = -1, best_val = -1;
    for (int i = 0; i < game->num_players; i++)
    {
        if (game->player_status[i] == PLAYER_ACTIVE)
        {
            int val = evaluate_hand(game, i);
            if (val > best_val)
            {
                best_val = val;
                best = i;
            }
        }
    }
    return best;
}