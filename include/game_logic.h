#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include "poker_client.h"  // for card_t, player_id_t
#include "macros.h"        // for constants like MAX_PLAYERS

#define MAX_COMMUNITY_CARDS 5
#define HAND_SIZE 2

typedef enum {
    PLAYER_FOLDED = 0,
    PLAYER_ACTIVE = 1,
    PLAYER_ALLIN  = 2,
    PLAYER_LEFT = 3
} player_status_t;

typedef enum {
    ROUND_JOIN = 0,
    ROUND_INIT = 1,
    ROUND_PREFLOP = 2,
    ROUND_FLOP = 3,
    ROUND_TURN = 4,
    ROUND_RIVER = 5,
    ROUND_SHOWDOWN = 6
} round_stage_t;

typedef struct {
    card_t player_hands[MAX_PLAYERS][HAND_SIZE];   // each player’s 2 cards
    card_t community_cards[MAX_COMMUNITY_CARDS];   // shared cards on table
    card_t deck[DECK_SIZE];                        // main deck
    int next_card;                                 // index of the next card to be drawn
    int player_stacks[MAX_PLAYERS];                // how many chips each player has
    int current_bets[MAX_PLAYERS];                 // amount bet this round
    int highest_bet;                               // highest bet to call to
    player_status_t player_status[MAX_PLAYERS];    // FOLDED, ACTIVE, etc
    int pot_size;                                  // total chips in pot
    int current_player;                            // index of current turn
    int dealer_player;                             // index of dealer TODO
    round_stage_t round_stage;                     // init/preflop/flop/turn/river/showdown
    int num_players;                               // total players in game
    int sockets[MAX_PLAYERS];                      // sockets for each player
} game_state_t;

void init_game_state(game_state_t *game, int starting_stack, int random_seed);
void reset_game_state(game_state_t *game);
void print_game_state(game_state_t *game); // for debugging
void init_deck(card_t deck[DECK_SIZE], int seed); 
void shuffle_deck(card_t deck[DECK_SIZE]);
int check_betting_end(game_state_t *game);
int find_winner(game_state_t *game);
int evaluate_hand(game_state_t *game, player_id_t pid);

void server_join(game_state_t *game);
int server_ready(game_state_t *game);
void server_deal(game_state_t *game);
int server_bet(game_state_t *game);
void server_community(game_state_t *game);
void server_end(game_state_t *game);

#endif
