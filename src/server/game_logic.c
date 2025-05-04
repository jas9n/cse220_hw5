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
void print_game_state(game_state_t *game)
{
    printf("\n=== Game State ===\n");
    printf("Community cards: ");
    for (int i = 0; i < MAX_COMMUNITY_CARDS; i++)
    {
        if (game->community_cards[i] != NOCARD)
        {
            printf("%s ", card_name(game->community_cards[i]));
        }
    }
    printf("\n");

    for (int i = 0; i < game->num_players; i++)
    {
        if (game->player_status[i] != PLAYER_LEFT)
        {
            printf("Player %d: ", i);
            printf("Cards [%s %s] ",
                   card_name(game->player_hands[i][0]),
                   card_name(game->player_hands[i][1]));
            printf("Stack: %d Bet: %d Status: %d\n",
                   game->player_stacks[i],
                   game->current_bets[i],
                   game->player_status[i]);
        }
    }
    printf("Pot: %d Current bet: %d\n", game->pot_size, game->highest_bet);
    printf("=================\n\n");
}

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
    int card_index = game->next_card;

    // Deal 2 cards to each active player
    for (int i = 0; i < game->num_players; i++)
    {
        if (game->player_status[i] == PLAYER_ACTIVE)
        {
            for (int j = 0; j < HAND_SIZE; j++)
            {
                game->player_hands[i][j] = game->deck[card_index++];
                printf("Dealt %s to player %d\n",
                       card_name(game->player_hands[i][j]), i);
            }
        }
    }

    // Store remaining cards for community cards
    for (int i = 0; i < MAX_COMMUNITY_CARDS; i++)
    {
        game->community_cards[i] = game->deck[card_index++];
    }

    game->next_card = card_index;
}

int server_bet(game_state_t *game)
{
    // This was our function to determine if everyone has called or folded

    while (!check_betting_end(game))
    {
        // Send INFO packet to current player
        server_packet_t info_pkt;
        build_info_packet(game, game->current_player, &info_pkt);
        send(game->sockets[game->current_player], &info_pkt, sizeof(info_pkt), 0);

        // Get action from current player
        client_packet_t in;
        if (recv(game->sockets[game->current_player], &in, sizeof(in), 0) <= 0)
        {
            // Handle disconnect
            game->player_status[game->current_player] = PLAYER_LEFT;
            continue;
        }

        server_packet_t resp;
        if (handle_client_action(game, game->current_player, &in, &resp) == 0)
        {
            send(game->sockets[game->current_player], &resp, sizeof(resp), 0);
        }
        else
        {
            resp.packet_type = NACK;
            send(game->sockets[game->current_player], &resp, sizeof(resp), 0);
            continue;
        }

        // Move to next active player
        int next = (game->current_player + 1) % game->num_players;
        while (game->player_status[next] != PLAYER_ACTIVE)
        {
            next = (next + 1) % game->num_players;
            if (next == game->current_player)
                break;
        }
        game->current_player = next;
    }

    return 0;
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
    switch (game->round_stage)
    {
    case ROUND_FLOP:
        printf("Dealing flop: %s %s %s\n",
               card_name(game->community_cards[0]),
               card_name(game->community_cards[1]),
               card_name(game->community_cards[2]));
        break;
    case ROUND_TURN:
        printf("Dealing turn: %s\n",
               card_name(game->community_cards[3]));
        break;
    case ROUND_RIVER:
        printf("Dealing river: %s\n",
               card_name(game->community_cards[4]));
        break;
    default:
        break;
    }
}

void server_end(game_state_t *game)
{
    int winner = find_winner(game);
    printf("\n=== Game Over ===\n");
    if (winner >= 0)
    {
        printf("Player %d wins with hand: ", winner);
        for (int i = 0; i < HAND_SIZE; i++)
        {
            printf("%s ", card_name(game->player_hands[winner][i]));
        }
        printf("\nCommunity cards: ");
        for (int i = 0; i < MAX_COMMUNITY_CARDS; i++)
        {
            printf("%s ", card_name(game->community_cards[i]));
        }
        printf("\nWinning pot: %d\n", game->pot_size);
        game->player_stacks[winner] += game->pot_size;
    }

    server_packet_t end_pkt;
    build_end_packet(game, winner, &end_pkt);

    for (int i = 0; i < game->num_players; i++)
    {
        if (game->player_status[i] != PLAYER_LEFT)
        {
            send(game->sockets[i], &end_pkt, sizeof(end_pkt), 0);
        }
    }
}

// Helper function to sort cards by rank
static void sort_cards(card_t cards[], int n)
{
    for (int i = 0; i < n - 1; i++)
    {
        for (int j = 0; j < n - i - 1; j++)
        {
            if (RANK(cards[j]) > RANK(cards[j + 1]))
            {
                card_t temp = cards[j];
                cards[j] = cards[j + 1];
                cards[j + 1] = temp;
            }
        }
    }
}

int evaluate_hand(game_state_t *game, player_id_t pid)
{
    card_t cards[7]; // 2 hole cards + 5 community cards
    int num_cards = 0;

    // Collect hole cards
    for (int i = 0; i < HAND_SIZE; i++)
    {
        if (game->player_hands[pid][i] != NOCARD)
        {
            cards[num_cards++] = game->player_hands[pid][i];
        }
    }

    // Collect community cards
    for (int i = 0; i < MAX_COMMUNITY_CARDS; i++)
    {
        if (game->community_cards[i] != NOCARD)
        {
            cards[num_cards++] = game->community_cards[i];
        }
    }

    if (num_cards < 5)
        return 0; // Not enough cards for evaluation

    sort_cards(cards, num_cards);

    // Check for straight flush
    for (int i = num_cards - 5; i >= 0; i--)
    {
        int straight = 1;
        int flush = 1;
        int base_suite = SUITE(cards[i]);

        for (int j = 1; j < 5; j++)
        {
            if (RANK(cards[i + j]) != RANK(cards[i + j - 1]) + 1)
                straight = 0;
            if (SUITE(cards[i + j]) != base_suite)
                flush = 0;
        }

        if (straight && flush)
            return 9000000 + RANK(cards[i + 4]);
    }

    // Check for four of a kind
    for (int i = 0; i <= num_cards - 4; i++)
    {
        if (RANK(cards[i]) == RANK(cards[i + 1]) &&
            RANK(cards[i]) == RANK(cards[i + 2]) &&
            RANK(cards[i]) == RANK(cards[i + 3]))
        {
            return 8000000 + RANK(cards[i]);
        }
    }

    // Check for full house
    for (int i = num_cards - 1; i >= 2; i--)
    {
        if (RANK(cards[i]) == RANK(cards[i - 1]) &&
            RANK(cards[i]) == RANK(cards[i - 2]))
        {
            for (int j = 0; j < num_cards - 1; j++)
            {
                if (RANK(cards[j]) == RANK(cards[j + 1]) &&
                    RANK(cards[j]) != RANK(cards[i]))
                {
                    return 7000000 + RANK(cards[i]) * 100 + RANK(cards[j]);
                }
            }
        }
    }

    // Check for flush
    for (int suite = 0; suite < 4; suite++)
    {
        int count = 0;
        int highest = 0;
        for (int i = 0; i < num_cards; i++)
        {
            if (SUITE(cards[i]) == suite)
            {
                count++;
                if (RANK(cards[i]) > highest)
                {
                    highest = RANK(cards[i]);
                }
            }
        }
        if (count >= 5)
            return 6000000 + highest;
    }

    // Check for straight
    for (int i = num_cards - 5; i >= 0; i--)
    {
        if (RANK(cards[i + 4]) == RANK(cards[i + 3]) + 1 &&
            RANK(cards[i + 3]) == RANK(cards[i + 2]) + 1 &&
            RANK(cards[i + 2]) == RANK(cards[i + 1]) + 1 &&
            RANK(cards[i + 1]) == RANK(cards[i]) + 1)
        {
            return 5000000 + RANK(cards[i + 4]);
        }
    }

    // Check for three of a kind
    for (int i = num_cards - 1; i >= 2; i--)
    {
        if (RANK(cards[i]) == RANK(cards[i - 1]) &&
            RANK(cards[i]) == RANK(cards[i - 2]))
        {
            return 4000000 + RANK(cards[i]);
        }
    }

    // Check for two pair
    for (int i = num_cards - 1; i >= 1; i--)
    {
        if (RANK(cards[i]) == RANK(cards[i - 1]))
        {
            for (int j = i - 2; j >= 1; j--)
            {
                if (RANK(cards[j]) == RANK(cards[j - 1]))
                {
                    return 3000000 + RANK(cards[i]) * 100 + RANK(cards[j]);
                }
            }
        }
    }

    // Check for one pair
    for (int i = num_cards - 1; i >= 1; i--)
    {
        if (RANK(cards[i]) == RANK(cards[i - 1]))
        {
            return 2000000 + RANK(cards[i]);
        }
    }

    // High card
    return 1000000 + RANK(cards[num_cards - 1]);
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

// Implement function to check if game should end
int check_game_end(game_state_t *game)
{
    int active_players = 0;
    for (int i = 0; i < game->num_players; i++)
    {
        if (game->player_status[i] == PLAYER_ACTIVE)
        {
            active_players++;
        }
    }
    return active_players < 2;
}