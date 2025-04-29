#ifndef POKER_CLIENT_H
#define POKER_CLIENT_H

#include "macros.h"
#include "wchar.h"

#define MAX_PLAYERS 6
#define MAX_CLIENT_PACKET_PARAMS 1

// ---------------------------- utility functions ---------------------------- //

typedef int card_t;
typedef int player_id_t;

/**
 * @brief converts from the string representation of a card to the integer representation
 * 
 * @param card the string representing the cards
 * @return the corresponding integer value (formed from SUITE | RANK from macro.h) on success.
 *         on failure, return NOCARD
 */
card_t card_id(char *card);

/**
 * @brief converts from a card id to a printable name
 * 
 * @param card the card id to get the corresponding card name
 * @return the null terminated string of the name of the card
 */
const char *card_name(card_t card);

/**
 * @brief converts from a card id to a fancy printable name using unicode
 * 
 * @param card the card id to get the corresponding card name
 * @return the null terminated string of the name of the card
 */
const wchar_t *fancy_card_name(card_t card);

// ---------------------------- Underlying networking functions ---------------------------- //

/**
 * @brief connect to the the server as a player
 * 
 * @param player_id the player to connect to server as
 * @return 0 on success, -1 otherwise
 */
int connect_to_serv(player_id_t player_id);

/**
 * @brief gracefully disconnect from the server
 *  
 * @return 0 on success, -1 otherwise
 */
int disconnect_to_serv();

/**
 * @brief the packet types to send to the server from the client
 */
typedef enum client_packet_type
{  
    JOIN,       // join the server
    LEAVE,      // leave the server
    READY,      // say ready for the round
    RAISE,      // raise the bet
    CALL,       // call the bet 
    CHECK,      // check
    FOLD        // fold hand
} client_packet_type_t;

typedef struct client_packet
{
    client_packet_type_t packet_type;
    int params[MAX_CLIENT_PACKET_PARAMS];
} client_packet_t;

/**
 * @brief sends a packet to the connected server, then waits for a response
 * 
 * @param pkt the packet contents to send to the server
 * @return 0 on success (ACK response), -1 on failure
 */
int send_packet(client_packet_t *pkt);

/**
 * @brief the packet types that was sent by the server to the client
 */
typedef enum server_packet_type
{
    ACK,        // acknowledge valid packet
    NACK,       // error with packet
    INFO,       // updated game information 
    END,        // game end along with  
    HALT        // halt to end connection
} server_packet_type_t;

/**
 * @brief information about the packet that is send to the client during a hand
 */
typedef struct
{
    card_t player_cards[2];
    card_t community_cards[5];
    int player_stacks[MAX_PLAYERS]; 
    int pot_size; 
    player_id_t dealer; //ID of player who deals the card and is the LAST person to bet in a round
    player_id_t player_turn; //ID of player who is to respond
    int bet_size; //bet that must be called
    int player_bets[MAX_PLAYERS]; //current max bet from each player
    int player_status[MAX_PLAYERS]; //1 for in hand, 0 for folded, 2 for left
} info_packet_t;

/**
 * @brief information about the packet that is send to the client after a hand
 */
typedef struct
{
    card_t player_cards[MAX_PLAYERS][2];
    card_t community_cards[5];
    int player_stacks[MAX_PLAYERS]; //Updated to add pot size to winner
    int pot_size;
    player_id_t dealer; //old dealer (from the finished hand)
    player_id_t winner; //ignore chopped pots
    int player_status[MAX_PLAYERS]; //1 for in hand, 0 for folded, 2 for left
} end_packet_t;

/**
 * @brief information about the packet recieved by the client 
 */
typedef struct server_packet
{
    server_packet_type_t packet_type;
    union
    {
        info_packet_t info;
        end_packet_t end;
    };
} server_packet_t;

/**
 * @brief waits for a packet from the server.
 * 
 * @param pkt the memory to store the packet information
 * @return 0 if packet recieved, -1 on failure
 */
int recv_packet(server_packet_t *pkt);

// ---------------------------- poker operations ---------------------------- //

typedef void(*info_packet_handler_t)(info_packet_t*);
typedef void(*end_packet_handler_t)(end_packet_t*);
typedef void(*on_halt_packet_handler_t)();

/**
 * @brief set the handler that is called whenver an info packet is received 
 * 
 * @note e.g. this can be used to print game state information, or update the screen
 * @param handler the new handler
 */
void set_on_info_packet_handler(info_packet_handler_t handler);

/**
 * @brief set the handler that is called whenver an end packet is received 
 * 
 * @note e.g. this can be used to print game state information, or update the screen
 * @param handler the new handler
 */
void set_on_end_packet_handler(end_packet_handler_t handler);

/**
 * @brief set the handler that is called whenever an HALT packet is received
 * 
 * @param handler the new handler
 */
void set_on_halt_packet_handler(end_packet_handler_t handler);

/**
 * @brief the player states they are ready
 * 
 * this should only be sent if the server state is in INIT or END
 * 
 * @return 0 if successful, -1 if failure
 */
int ready();

/**
 * @brief the player checks by sending a check packet to the server
 * 
 * this should only be sent if the server state is in DEAL, FLOP, TURN, RIVER
 * 
 * @return 0 if successful, -1 if failure
 */
int check();

/**
 * @brief the player raises the bet
 * 
 * this should only be sent if the server state is in DEAL, FLOP, TURN, RIVER
 * 
 * @return 0 if successful, -1 if failure
 */
int bet_raise(int new_bet);

/**
 * @brief the player calls the bet
 * 
 * this should only be sent if the server state is in DEAL, FLOP, TURN, RIVER
 * 
 * @return 0 if successful, -1 if failure
 */
int call();

/**
* @brief the player folds
 * 
 * this should only be sent if the server state is in DEAL, FLOP, TURN, RIVER
 * 
 * @return 0 if successful, -1 if failure
 */
int fold();

/**
 * @brief the player leaves the table
 * 
 * this should only be send if the server state is in END
 * 
 * @return 0 if successful, -1 if failure
 */
int leave();

/**
 * @brief check if it is the players turn based on the most recent info packet
 * 
 * @return 1 if it is player_id's turn and 0 otherwise
 */
int is_players_turn(player_id_t player_id);

/**
 * @brief checks if an halt packet has been recieved
 * 
 * @return 1 if the hand has ended. 0 otherwise.
 */
int has_recv_halt();

#endif