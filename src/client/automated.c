/**
 * Supported commands
 *  - ready
 *  - leave
 *  - raise AMOUNT
 *  - raise allin
 *  - call
 *  - check
 *  - fold
 * 
 * if EOF is reached in stdin, then the client will fold if it recieves an INFO packet until
 * the next END packet. At the next END packet, the client will leave the table then. 
 */

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "logs.h"
#include "poker_client.h"

player_id_t id;
server_packet_t serv_pkt;
char *line = NULL;
size_t buffer_len = 0;
size_t line_len = 0;

int done_reading = 0;

typedef void(*command_t)(int argc, char *argv[]);

#define TOTAL_COMMANDS 6

static void ready_command(int argc, char *argv[])
{
    int required_argc = 0;
    if (argc != required_argc + 1)
    {
        log_err("Wrong number of args (given: %d, required: %d) for CLI command '%s'", argc - 1, required_argc, argv[0]);
        return;
    }

    int ret = ready();
    // this should move on to the next state (we should never return here)
    if (ret == 0) recv_packet(&serv_pkt); 
    // otherwise, continue the loop
}

static void leave_command(int argc, char *argv[])
{
    int required_argc = 0;
    if (argc != required_argc + 1)
    {
        log_err("Wrong number of args (given: %d, required: %d) for CLI command '%s'", argc - 1, required_argc, argv[0]);
        return;
    }

    int ret = leave();
    if (ret == 0)
    {
        disconnect_to_serv();
        log_fini();
        exit(0);
    }
    // otherwise, continue the loop
}

static void raise_command(int argc, char *argv[])
{
    int required_argc = 1;
    if (argc != required_argc + 1)
    {
        log_err("Wrong number of args (given: %d, required: %d) for CLI command '%s'", argc - 1, required_argc, argv[0]);
        return;
    }

    if (strcmp(argv[1], "allin") == 0)
    {
        int ret = bet_raise(serv_pkt.info.player_stacks[id]);
        if (ret == 0) recv_packet(&serv_pkt);
        return; // otherwise, continue the loop

    }

    int amount = atoi(argv[1]);
    if (amount != 0)
    {
        int ret = bet_raise(amount);
        if (ret == 0) recv_packet(&serv_pkt);
    }
    // otherwise, continue the loop
}

static void call_command(int argc, char *argv[])
{
    int required_argc = 0;
    if (argc != required_argc + 1)
    {
        log_err("Wrong number of args (given: %d, required: %d) for CLI command '%s'", argc - 1, required_argc, argv[0]);
        return;
    }

    int ret = call();
    if (ret == 0) recv_packet(&serv_pkt);
    // otherwise, continue the loop
}

static void check_command(int argc, char *argv[])
{
    int required_argc = 0;
    if (argc != required_argc + 1)
    {
        log_err("Wrong number of args (given: %d, required: %d) for CLI command '%s'", argc - 1, required_argc, argv[0]);
        return;
    }

    int ret = check();
    if (ret == 0) recv_packet(&serv_pkt);
    // otherwise, continue the loop
}

static void fold_command(int argc, char *argv[])
{
    int required_argc = 0;
    if (argc != required_argc + 1)
    {
        log_err("Wrong number of args (given: %d, required: %d) for CLI command '%s'", argc - 1, required_argc, argv[0]);
        return;
    }

    int ret = fold();
    if (ret == 0) recv_packet(&serv_pkt);
    // otherwise, continue the loop
}

static const char *command_names[TOTAL_COMMANDS] = {
    "ready",
    "leave",
    "raise",
    "call",
    "check",
    "fold"
};

static command_t command_list[TOTAL_COMMANDS] = {
    ready_command,
    leave_command,
    raise_command,
    call_command,
    check_command,
    fold_command
};

// commands

static size_t count_words(char *line)
{
    size_t word_count = 0;

    int prev_not_whitespace = 0;
    while (*line)
    {
        if (!isspace(*line)) prev_not_whitespace = 1;
        else if (prev_not_whitespace && isspace(*line))
        {
            ++word_count;
            prev_not_whitespace = 0;
        }
        ++line;
    }

    if (prev_not_whitespace) ++word_count; 

    return word_count;
}

#define STRTOK_WHITESPACE_DELIM " \t"

static void invoke_cli_line()
{
    size_t argc = count_words(line);

    // if line is empty or only contains spaces, return int representing empty line
    if (argc == 0) return;

    char **args = malloc(argc * sizeof(void*));
    // * use strtok to iterate through the words

    args[0] = strtok(line, STRTOK_WHITESPACE_DELIM);

    for (size_t i = 1; i < argc; ++i)
    {
        args[i] = strtok(NULL, STRTOK_WHITESPACE_DELIM);
    }

    int found_command = 0;
    // start searching for command
    for (size_t i = 0; i < TOTAL_COMMANDS; ++i)
    {
        if (strcmp(args[0], command_names[i]) == 0)
        {
            command_t command_handler = command_list[i];
            // no possible chance of overflow since that would require approx 2 * 10^9 arguments
            command_handler((int) argc, args);
            found_command = 1;
            break;
        }
    }

    if (!found_command) log_err("Unrecognized command: %s\n", args[0]);

    free(args);
}

static char *remove_newline()
{
    if (line[line_len - 1] == '\n')
    {
        line[line_len - 1] = '\0';
    }
    return line;
}

// handlers 

static void on_halt()
{
    disconnect_to_serv();
    log_fini();
    exit(0);
}

static void show_info_pkt(info_packet_t *pkt)
{
    printf("\n");
    printf("PLAYERS %d TURN:\n", pkt->player_turn);
    printf("DEALER: PLAYER %d\n", pkt->dealer);
    printf("POT SIZE: %d\n", pkt->pot_size);
    printf("BET SIZE: %d\n", pkt->bet_size);
    printf("YOUR CARDS: %s %s\n", card_name(pkt->player_cards[0]), card_name(pkt->player_cards[1]));
    if (pkt->community_cards[0] != NOCARD)
    {
        printf("COMMUNITY CARDS: %s %s %s %s %s\n", 
            card_name(pkt->community_cards[0]),
            card_name(pkt->community_cards[1]),
            card_name(pkt->community_cards[2]),
            card_name(pkt->community_cards[3]),
            card_name(pkt->community_cards[4])
        );
    }

    for (player_id_t player_id = 0; player_id < MAX_PLAYERS; ++player_id)
    {
        if (pkt->player_status[player_id] == 1)
        {
            printf("\tPLAYER %d [ STACK = %d | BET = %d ]\n", player_id, pkt->player_stacks[player_id], pkt->player_bets[player_id]);
        }
        else if (pkt->player_status[player_id] == 0)
        {
            printf("\tPLAYER %d [ STACK = %d | FOLDED ]\n", player_id, pkt->player_stacks[player_id]);
        }
    }
}

static void on_info(info_packet_t *pkt)
{
    show_info_pkt(pkt);
    if (is_players_turn(id))
    {

        while (1)
        {
            if (done_reading)
            {
                char *fold_args[] = { "fold" };
                fold_command(1, fold_args);
            }

            printf("> ");
            fflush(stdout);
            if ((line_len = getline(&line, &buffer_len, stdin)) != -1) invoke_cli_line(remove_newline());
            else
            {
                log_info("No more lines of input. Leaving when available.");
                done_reading = 1;
            }
        }
    }    
    else
    {
        recv_packet(&serv_pkt);
    }
}

static void show_end_pkt(end_packet_t *pkt)
{
    printf("\n");
    printf("WINNER: PLAYER %d\n", pkt->winner);
    printf("DEALER: PLAYER %d\n", pkt->dealer);
    printf("POT SIZE: %d\n", pkt->pot_size);

    if (pkt->community_cards[0] != NOCARD)
    {
        printf("COMMUNITY CARDS: %s %s %s %s %s\n", 
            card_name(pkt->community_cards[0]),
            card_name(pkt->community_cards[1]),
            card_name(pkt->community_cards[2]),
            card_name(pkt->community_cards[3]),
            card_name(pkt->community_cards[4])
        );
    }

    for (player_id_t player_id = 0; player_id < MAX_PLAYERS; ++player_id)
    {
        if (pkt->player_status[player_id] == 1)
        {
            printf("\tPLAYER %d [ STACK = %d | CARDS = %s %s ]\n", 
                player_id, pkt->player_stacks[player_id], 
                card_name(pkt->player_cards[player_id][0]), 
                card_name(pkt->player_cards[player_id][1])
            );
        }
        else if (pkt->player_status[player_id] == 0)
        {
            printf("\tPLAYER %d [ STACK = %d | CARDS = %s %s | FOLDED ]\n", 
                player_id, pkt->player_stacks[player_id], 
                card_name(pkt->player_cards[player_id][0]), 
                card_name(pkt->player_cards[player_id][1])
            );
        }
    }
}

static void on_end(end_packet_t *pkt)
{
    if (pkt) show_end_pkt(pkt);

    while (1)
    {
        if (done_reading)
        {
            char *leave_args[] = { "leave" };
            leave_command(1, leave_args);
        }

        printf("> ");
        fflush(stdout);
        if ((line_len = getline(&line, &buffer_len, stdin)) != -1) invoke_cli_line(remove_newline());
        else
        {
            log_info("No more lines of input. Exiting...");
            done_reading = 1;
        }
    }
}

// main

int main(int argc, char *argv[])
{
    int ret;

    if (argc != 2) 
    {
        fprintf(stderr, "incorrect number of args. expecting 1, got %d.\n", argc - 1);
        return 1;
    }

    if (sscanf(argv[1], " %d ", &id) != 1)
    {
        fprintf(stderr, "required arg is not integer.\n");
        return 1;
    }

    if (id < 0 || id >= MAX_PLAYERS)
    {
        fprintf(stderr, "required arg is not in range.\n");
        return 1;
    }

    log_player_init(id);

    // attempt to connect to the server
    ret = connect_to_serv(id);
    if (ret == -1) // connection failed 
    {   
        log_err("Failed to connect to server as player %d. Exiting...", id);
        exit(1);        
    }   

    set_on_info_packet_handler(on_info);
    set_on_end_packet_handler(on_end);
    set_on_halt_packet_handler(on_halt);

    on_end(NULL);

    disconnect_to_serv();
    log_fini();

    return 0; 
}