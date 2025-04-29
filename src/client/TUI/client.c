#include <ncursesw/curses.h>
#include <locale.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "logs.h"
#include "poker_client.h"

typedef struct coordinate
{
    int x, y;
} coordinate_t;

typedef enum anchor
{
    TOP_LEFT,
    TOP_RIGHT,
    TOP_MIDDLE,
    BOTTOM_LEFT,
    BOTTOM_RIGHT,
    BOTTOM_MIDDLE,
    LEFT_MIDDLE,
    RIGHT_MIDDLE,
    MIDDLE,
    MAX_ANCHORS
} anchor_t;

// assuming box is drawn
static coordinate_t anchors[MAX_ANCHORS] = { 0 };
static coordinate_t player_panel_anchors[MAX_PLAYERS] = { 0 };

void set_anchors(WINDOW *window)
{
    int max_x, max_y;
    getmaxyx(window, max_y, max_x);

    anchors[TOP_LEFT] = (coordinate_t){ 1, 1 };
    anchors[TOP_RIGHT] = (coordinate_t){ (max_x - 2), 1 };
    anchors[TOP_MIDDLE] = (coordinate_t){ (max_x - 2) / 2, 1 };
    anchors[BOTTOM_LEFT] = (coordinate_t){ 1, max_y - 2 };
    anchors[BOTTOM_RIGHT] = (coordinate_t){ max_x - 2, max_y - 2 };
    anchors[BOTTOM_MIDDLE] = (coordinate_t){ (max_x - 2) / 2, max_y - 2 };
    anchors[LEFT_MIDDLE] = (coordinate_t){ 1, (max_y - 2) / 2 };
    anchors[RIGHT_MIDDLE] = (coordinate_t){ max_x - 2, (max_y - 2) / 2 };
    anchors[MIDDLE] = (coordinate_t){ (max_x - 2) / 2, (max_y - 2) / 2 };
}

// -------------------- buttons -------------------- //

struct button;

typedef void (*on_button_hover_callback_t)(struct button *);
typedef void (*on_button_unhover_callback_t)(struct button *);
typedef void (*on_button_click_callback_t)(struct button *);

typedef struct button
{
    WINDOW *button_panel;
    WINDOW *parent;
    coordinate_t top_left; 
    coordinate_t bottom_right;
    int is_active;
    int hover;
    on_button_hover_callback_t on_hover;
    on_button_unhover_callback_t on_unhover;
    on_button_click_callback_t on_click;
} button_t;

// initializes the button interface by enabling the mouse tracking
static void button_module_init()
{
    mmask_t mouse_mask = BUTTON1_CLICKED | BUTTON2_CLICKED | BUTTON3_CLICKED | BUTTON4_CLICKED | REPORT_MOUSE_POSITION;
    mousemask(mouse_mask, NULL);

    printf("\033[?1003h\n");
}

static void button_module_fini()
{
    printf("\033[?1003l\n");
}

static void create_button(button_t *button, WINDOW *parent, int width, int height, coordinate_t top_left)
{
    button->top_left = top_left;
    button->bottom_right = top_left;
    button->parent = parent;

    button->bottom_right.x += width + 2;
    button->bottom_right.y += height + 2;

    button->hover = false;
    button->is_active = false;
    button->button_panel = derwin(parent, height + 2, width + 2, top_left.y, top_left.x);
    button->on_hover = NULL;
    button->on_hover = NULL;
    button->on_click = NULL;
}

static void draw_button_panel(button_t *button)
{
    box(button->button_panel, 0, 0);
    wrefresh(button->button_panel);
}

static void write_button_text(button_t *button, char *button_text)
{
    mvwprintw(button->button_panel, 1, 1, "%s", button_text);
    wrefresh(button->button_panel);
}

static void delete_button(button_t *button)
{   
    delwin(button->button_panel);
    button->on_hover = NULL;
    button->on_hover = NULL;
    button->on_click = NULL;
    button->is_active = false;
    button->hover = false;
}

static void enable_button(button_t *button)
{
    button->is_active = true;
}

static void disable_button(button_t *button)
{
    button->is_active = false;
}

static void process_button_event(button_t *button, MEVENT *event)
{
    int in_box_bound = event->x >= button->top_left.x && event->y >= button->top_left.y 
        && event->x <= button->bottom_right.x && event->y <= button->bottom_right.y;

    // mvwprintw(button->parent, 1, 1, "Bstate - %d", event->bstate);
    if (button->is_active)
    {
        if (button->on_click && (event->bstate & (BUTTON1_CLICKED | BUTTON2_CLICKED | BUTTON3_CLICKED | BUTTON4_CLICKED)) && in_box_bound)
        {
            button->on_click(button);
        }
        else // mouse position event
        {
            if (!button->hover && in_box_bound)
            {
                button->hover = true;
                if (button->on_hover) button->on_hover(button);
            }
            else if (button->hover && !in_box_bound)
            {
                button->hover = false;
                if (button->on_unhover) button->on_unhover(button);
            }
        }
    }
}

// -------------------- Player data panel elements -------------------- //

#define PLAYER_PANEL_WIDTH 23
#define PLAYER_PANEL_HEIGHT 5

static const wchar_t *PLAYER_PANELS[] = {
    L"╔──────────╦──────────╗",
    L"│          │          │",
    L"╚─────┬────┼────┬─────╝",
    L"      │    │    │      ",
    L"      └────┴────┘      "
};

// * assume that window is appropriately sized 
static void set_player_panel_anchors(WINDOW *window, coordinate_t *anchors)
{
    int max_x, max_y;
    getmaxyx(window, max_y, max_x);

    int y = 1;
    for (size_t i = 0; i < MAX_PLAYERS; ++i)
    {
        if (i & 1) 
        {
            anchors[i] = (coordinate_t){ max_x - 2 - PLAYER_PANEL_WIDTH, y };
            y += PLAYER_PANEL_HEIGHT + 1;
        }
        else anchors[i] = (coordinate_t){ 2, y };
    }
}

static WINDOW *create_player_panel(WINDOW *parent, coordinate_t top_left)
{
    WINDOW *player_panel = derwin(parent, PLAYER_PANEL_HEIGHT, PLAYER_PANEL_WIDTH, top_left.y, top_left.x);
    return player_panel;
}

// -------------------- pot size information panel -------------------- //

#define POT_PANEL_WIDTH 19
#define POT_PANEL_HEIGHT 5

static const wchar_t *POT_PANELS[] = {
    L"╔─────╦───────────╗",
    L"│ Pot │           │",
    L"╠─────╬───────────╣",
    L"│ Bet │           │",
    L"╚─────╩───────────╝"
};

static void set_pot_panel_anchor(WINDOW *window, coordinate_t *anchor)
{
    int max_x, max_y;
    getmaxyx(window, max_y, max_x);

    anchor->x = max_x / 2 - POT_PANEL_WIDTH / 2;
    anchor->y = 1;
}

static WINDOW *create_pot_panel(WINDOW *parent, coordinate_t top_left)
{
    WINDOW *pot_panel = derwin(parent, POT_PANEL_HEIGHT, POT_PANEL_WIDTH, top_left.y, top_left.x); 
    return pot_panel;
}

// -------------------- community card panel -------------------- //

#define COMMUNITY_PANEL_WIDTH 26
#define COMMUNITY_PANEL_HEIGHT 3

static const wchar_t *COMMUNITY_PANELS[] = {
    L"┌────┬────┬────┬────┬────┐",
    L"│    │    │    │    │    │",
    L"└────┴────┴────┴────┴────┘"
};

static void set_comm_card_anchor(WINDOW *window, coordinate_t *anchor)
{
    int max_x, max_y;
    getmaxyx(window, max_y, max_x);

    int player_panel_rows = (MAX_PLAYERS + 1) / 2; // round up

    anchor->x = max_x / 2 - COMMUNITY_PANEL_WIDTH / 2;
    // PLAYER_PANEL_HEIGHT * player_panel_rows = # of lines for player panels
    // player_panel_rows - 1 is the amount of line skipped between panesl
    anchor->y = 1 + (PLAYER_PANEL_HEIGHT * player_panel_rows + player_panel_rows - 1) / 2;
}

static WINDOW *create_community_cards_panel(WINDOW *parent, coordinate_t top_left)
{
    WINDOW *community_panel = derwin(parent, COMMUNITY_PANEL_HEIGHT, COMMUNITY_PANEL_WIDTH, top_left.y, top_left.x); 
    return community_panel;
}

// -------------------- main poker screen -------------------- //

#define BET_PROMPT_PANEL_WIDTH 26
#define BET_PROMPT_PANEL_HEIGHT 4

static const wchar_t *BET_PROMPT_PANELS[] = {
    L"┌────────────────────────┐",
    L"│ Enter bet amount:      │",
    L"│                        │",   
    L"└────────────────────────┘" 
};

static void set_bet_prompt_anchor(WINDOW *window, coordinate_t *anchor)
{
    int max_y, max_x;
    getmaxyx(window, max_y, max_x);

    int player_panel_rows = (MAX_PLAYERS + 1) / 2; // round up

    int x = max_x / 2 - BET_PROMPT_PANEL_WIDTH / 2;
    // PLAYER_PANEL_HEIGHT * player_panel_rows = # of lines for player panels
    // player_panel_rows - 1 is the amount of line skipped between panesl
    int y = 1 + (BET_PROMPT_PANEL_HEIGHT * player_panel_rows + player_panel_rows - 1) / 2;

    anchor->x = x;
    anchor->y = y;
}

static WINDOW *create_bet_prompt_panel(WINDOW *parent, coordinate_t top_left)
{
    WINDOW *bet_prompt_panel = derwin(parent, BET_PROMPT_PANEL_HEIGHT, BET_PROMPT_PANEL_WIDTH, top_left.y, top_left.x);
    return bet_prompt_panel;
}

// -------------------- main poker screen -------------------- //

#define POKER_BUTTONS 3
#define POKER_BUTTONS_WIDTH 35
#define POKER_BUTTON_0_WIDTH 8
#define POKER_BUTTON_1_WIDTH 9
#define POKER_BUTTON_2_WIDTH 8

static void set_button_anchors(WINDOW *window, coordinate_t *anchors)
{
    int max_x, max_y;
    getmaxyx(window, max_y, max_x);

    int player_panel_rows = (MAX_PLAYERS + 1) / 2; // round up

    int y = 1 + (PLAYER_PANEL_HEIGHT * player_panel_rows + player_panel_rows - 1);
    int mid_x = max_x / 2; 

    int button0_width = POKER_BUTTON_0_WIDTH + 2;
    int button1_width = POKER_BUTTON_0_WIDTH + 2;
    int button2_width = POKER_BUTTON_0_WIDTH + 2;

    int button_0_x = mid_x - (button1_width / 2) - 1 - button0_width - 2;
    int button_1_x = mid_x - (button1_width / 2);
    int button_2_x = mid_x + (button1_width / 2) + 4;

    anchors[0] = (coordinate_t){ button_0_x, y };
    anchors[1] = (coordinate_t){ button_1_x, y };
    anchors[2] = (coordinate_t){ button_2_x, y };
}

static void on_poker_button_hover(button_t *button)
{   
    wattron(button->button_panel, COLOR_PAIR(1));
    draw_button_panel(button);
    wattroff(button->button_panel, COLOR_PAIR(1));
    wrefresh(button->button_panel);
}

static void on_poker_button_unhover(button_t *button)
{
    draw_button_panel(button);
    wrefresh(button->button_panel);
}

typedef struct poker_screen
{
    WINDOW *main_window;
    coordinate_t players_anchors[MAX_PLAYERS];
    WINDOW *player_panels[MAX_PLAYERS];
    coordinate_t community_anchor;
    WINDOW *community_cards_panel;
    coordinate_t pot_anchor;
    WINDOW *pot_panel;
    coordinate_t button_anchors[POKER_BUTTONS];
    button_t buttons[POKER_BUTTONS];
    coordinate_t bet_prompt_anchor;
    WINDOW *bet_prompt_panel;
} poker_screen_t;

static void init_poker_screen(poker_screen_t *poker_screen, WINDOW *window)
{
    poker_screen->main_window = window;

    // initialize all anchors
    set_player_panel_anchors(window, poker_screen->players_anchors);
    set_pot_panel_anchor(window, &poker_screen->pot_anchor);
    set_comm_card_anchor(window, &poker_screen->community_anchor);
    set_button_anchors(window, poker_screen->button_anchors);
    set_bet_prompt_anchor(window, &poker_screen->bet_prompt_anchor);

    // create player panels
    for (size_t i = 0; i < MAX_PLAYERS; ++i)
        poker_screen->player_panels[i] = create_player_panel(window, poker_screen->players_anchors[i]);
    
    // create pot panel
    poker_screen->pot_panel = create_pot_panel(window, poker_screen->pot_anchor);

    // create community card panel
    poker_screen->community_cards_panel = create_community_cards_panel(window, poker_screen->community_anchor);    

    // create button panels
    int poker_button_widths[] = { POKER_BUTTON_0_WIDTH, POKER_BUTTON_1_WIDTH, POKER_BUTTON_2_WIDTH };
    for (size_t i = 0; i < POKER_BUTTONS; ++i)
    {
        create_button(&poker_screen->buttons[i], window, poker_button_widths[i], 1, poker_screen->button_anchors[i]);
        poker_screen->buttons[i].on_hover = poker_screen->buttons[i].on_click = poker_screen->buttons[i].on_unhover = NULL;
        
        poker_screen->buttons[i].on_hover = on_poker_button_hover;
        poker_screen->buttons[i].on_unhover = on_poker_button_unhover;
        poker_screen->buttons[i].on_click = NULL;
    }

    // create bet prompt panel
    poker_screen->bet_prompt_panel = create_bet_prompt_panel(window, poker_screen->bet_prompt_anchor);
}

static void draw_poker_border(poker_screen_t *poker_screen)
{
    box(poker_screen->main_window, 0, 0);

    int pid = getpid();
    mvwprintw(poker_screen->main_window, 0, 2, " POKER [%d] ", pid);
}

static void draw_community_card_panel(poker_screen_t *poker_screen)
{   
    for (size_t i = 0; i < COMMUNITY_PANEL_HEIGHT; ++i)
        mvwprintw(poker_screen->community_cards_panel, i, 0, "%ls", COMMUNITY_PANELS[i]);
    wrefresh(poker_screen->community_cards_panel);
}

static void write_community_card(poker_screen_t *poker_screen, size_t card_index, card_t card)
{
    const wchar_t *card_name = fancy_card_name(card);
    mvwprintw(poker_screen->community_cards_panel, 1, card_index * 5 + 2, "%ls", card_name);
    wrefresh(poker_screen->community_cards_panel);
}

static void draw_pot_panel(poker_screen_t *poker_screen)
{
    for (size_t i = 0; i < POT_PANEL_HEIGHT; ++i)
        mvwprintw(poker_screen->pot_panel, i, 0, "%ls", POT_PANELS[i]);
    wrefresh(poker_screen->pot_panel);
}

static void write_pot_value(poker_screen_t *poker_screen, int amount)
{
    char spot[10] = { 0 };
    snprintf(spot, 10, "$%d", amount);
    mvwprintw(poker_screen->pot_panel, 1, 8, "%s", spot);
    wrefresh(poker_screen->pot_panel);
}

static void write_bet_value(poker_screen_t *poker_screen, int amount)
{
    char spot[10] = { 0 };
    snprintf(spot, 10, "$%d", amount);
    mvwprintw(poker_screen->pot_panel, 3, 8, "%s", spot);
    wrefresh(poker_screen->pot_panel);
}

static void draw_player_panel(poker_screen_t *poker_screen, player_id_t player_id)
{
    for (size_t i = 0; i < PLAYER_PANEL_HEIGHT; ++i)
        mvwprintw(poker_screen->player_panels[player_id], i, 0, "%ls", PLAYER_PANELS[i]);
    wrefresh(poker_screen->player_panels[player_id]);
}

static void draw_all_player_panels(poker_screen_t *poker_screen)
{
    for (player_id_t i = 0; i < MAX_PLAYERS; ++i)
        draw_player_panel(poker_screen, i);
}

static void write_player_name(poker_screen_t *poker_screen, player_id_t player_id, char *player_name)
{
    char name[9] = { 0 };
    strncpy(name, player_name, 8); // ensure name length is fixed
    mvwprintw(poker_screen->player_panels[player_id], 1, 2, "%s", name);
    wrefresh(poker_screen->player_panels[player_id]);
}

static void write_player_stack(poker_screen_t *poker_screen, player_id_t player_id, int stack)
{
    char sstack[9] = { 0 };
    snprintf(sstack, 9, "$%d", stack);
    mvwprintw(poker_screen->player_panels[player_id], 1, 13, "%s", sstack);
    wrefresh(poker_screen->player_panels[player_id]);
}

static void write_player_card(poker_screen_t *poker_screen, player_id_t player_id, card_t card0, card_t card1)
{
    mvwprintw(poker_screen->player_panels[player_id], 3, 8, "%ls", fancy_card_name(card0));
    mvwprintw(poker_screen->player_panels[player_id], 3, 13, "%ls", fancy_card_name(card1));
    wrefresh(poker_screen->player_panels[player_id]);
}

static void write_player_dealer(poker_screen_t *poker_screen, player_id_t player_id)
{
    mvwprintw(poker_screen->player_panels[player_id], 3, 2, "[D]");
    wrefresh(poker_screen->player_panels[player_id]);
}

static void write_player_turn(poker_screen_t *poker_screen, player_id_t player_id)
{
    mvwprintw(poker_screen->player_panels[player_id], 3, 18, "[*]");
    wrefresh(poker_screen->player_panels[player_id]);
}

static void write_player_fold(poker_screen_t *poker_screen, player_id_t player_id)
{
    mvwprintw(poker_screen->player_panels[player_id], 3, 18, "[F]");
    wrefresh(poker_screen->player_panels[player_id]);
}

static void write_player_winner(poker_screen_t *poker_screen, player_id_t player_id)
{
    mvwprintw(poker_screen->player_panels[player_id], 3, 18, "[W]");
    wrefresh(poker_screen->player_panels[player_id]);
}

static void process_all_buttons(poker_screen_t *poker_screen, MEVENT *event)
{
    for (size_t i = 0; i < POKER_BUTTONS; ++i)
    {
        process_button_event(&poker_screen->buttons[i], event);
    }
}

static void draw_bet_prompt(poker_screen_t *poker_screen)
{
    for (size_t i = 0; i < BET_PROMPT_PANEL_HEIGHT; ++i)
        mvwprintw(poker_screen->bet_prompt_panel, i, 0, "%ls", BET_PROMPT_PANELS[i]);
    wrefresh(poker_screen->bet_prompt_panel);
}

// glob
static poker_screen_t poker_screen;

// draw the base elements of the TUI
static void draw_base_poker_screen()
{
    clear();

    // set modes
    raw();
    noecho();
    curs_set(0);

    draw_poker_border(&poker_screen);
    draw_all_player_panels(&poker_screen);

    draw_pot_panel(&poker_screen);
    draw_community_card_panel(&poker_screen);

    refresh();
}

static server_packet_t serv_pkt;
player_id_t id;

// -------------------- wait for ready/leave (caused by end packet) -------------------- //

static void send_ready(button_t *button)
{
    int ret = ready();
    // this should move on to the next state (we should never return here)
    if (ret == 0) recv_packet(&serv_pkt); 
    log_err("sending READY packet failed.");
    // otherwise, continue the loop
}

static void send_leave(button_t *button)
{
    int ret = leave();
    if (ret == 0)
    {
        flushinp();
        disconnect_to_serv();
        log_info("TUI fini.");
        button_module_fini();
        endwin();
        log_fini();
        exit(0);
    }
    // otherwise, continue the loop
}

static void send_check(button_t *button)
{
    int ret = check();
    if (ret == 0) recv_packet(&serv_pkt);
    log_err("sending CHECK packet failed.");
}

static int get_raise_amount(int check_enabled)
{
    // disable all buttons temporarily
    disable_button(&poker_screen.buttons[0]);
    disable_button(&poker_screen.buttons[1]);
    disable_button(&poker_screen.buttons[2]);

    // disable mouse input temporarily
    mousemask(0, NULL);

    // enable echo
    echo();
    curs_set(1);

    char read_input[23] = { 0 };
    while (true)
    {
        draw_bet_prompt(&poker_screen);
        wmove(poker_screen.main_window, poker_screen.bet_prompt_anchor.y + 2, poker_screen.bet_prompt_anchor.x + 2);

        memset(read_input, 0, sizeof(read_input));
        int ch = 0;
        for (size_t i = 0; i < 22; ++i)
        {
            ch = getch();
            if (ch == KEY_BACKSPACE) // handle backspace
            {
                if (i != 0)
                {
                    int y, x;
                    printw(" ");
                    getyx(poker_screen.main_window, y, x);
                    wmove(poker_screen.main_window, y, x-1);
                    i--;
                    read_input[i] = 0;
                    i--;
                }
                else
                {
                    --i;
                    wmove(poker_screen.main_window, poker_screen.bet_prompt_anchor.y + 2, poker_screen.bet_prompt_anchor.x + 2);
                }
                continue;
            }
            else if (ch == '\n') break;
            read_input[i] = tolower(ch);
        }

        int amount;

        if (strcmp(read_input, "fold") == 0)
        {
            button_module_init();
            noecho();
            curs_set(0);
            return -1; 
        }
        else if (check_enabled && strcmp(read_input, "check") == 0)
        {
            button_module_init();
            noecho();
            curs_set(0);
            return -2;
        }
        else if (!check_enabled && strcmp(read_input, "call") == 0)
        {
            button_module_init();
            noecho();
            curs_set(0);
            return -2;
        }
        else if ((amount = atoi(read_input)) > 0)
        {
            button_module_init();
            noecho();
            curs_set(0);
            return amount;
        }
    }

    return 0; // should never be called
}

static void send_call(button_t *button)
{
    int ret = call();
    if (ret == 0) recv_packet(&serv_pkt);
    log_err("sending CALL packet failed.");
}

static void send_fold(button_t *button)
{
    int ret = fold();
    if (ret == 0) recv_packet(&serv_pkt);
    log_err("sending FOLD packet failed.");
}

static void poker_game_screen(info_packet_t *pkt);

static void send_raise(button_t *button)
{
    // attempt to get a valid bet amount
    int amount = get_raise_amount(serv_pkt.info.bet_size == 0);

    if (amount == -1)
    {
        send_fold(button);
    } 
    else if (amount == -2 && serv_pkt.info.bet_size == 0)
    {
        send_check(button);
    }
    else if (amount == -2 && serv_pkt.info.bet_size != 0)
    {
        send_call(button);
    }
    
    int ret = bet_raise(amount);
    if (ret == 0) recv_packet(&serv_pkt);

    log_err("sending RAISE packet failed.");
    poker_game_screen(&serv_pkt.info); // try again
}

static void draw_end_info(end_packet_t* pkt)
{
    draw_base_poker_screen();

    char *player_names[] = { 
        "Player 0", "Player 1", "Player 2", 
        "Player 3", "Player 4", "Player 5" 
    };

    // write pot and bet amount
    write_pot_value(&poker_screen, pkt->pot_size);

    write_player_winner(&poker_screen, pkt->winner);

    // set player info
    for (player_id_t player_id = 0; player_id < MAX_PLAYERS; ++player_id)
    {
        if (pkt->player_status[player_id] != 2)
        {
            write_player_name(&poker_screen, player_id, player_names[player_id]);
            write_player_stack(&poker_screen, player_id, pkt->player_stacks[player_id]);
            write_player_card(&poker_screen, player_id, pkt->player_cards[player_id][0], pkt->player_cards[player_id][1]);
        }
        else if (pkt->player_status[player_id] == 0)
        {
            write_player_fold(&poker_screen, player_id);
            write_player_card(&poker_screen, player_id, pkt->player_cards[player_id][0], pkt->player_cards[player_id][1]);
        }
    }

    // draw community cards
    for (size_t i = 0; i < 5; ++i)
        write_community_card(&poker_screen, i, pkt->community_cards[i]);
    
}

static void ready_leave_screen(end_packet_t *pkt)
{
    char *ready_leave_buttons[] = { " READY  ", "  LEAVE " }; 
    // on first call 
    if (!pkt)
    {
        draw_base_poker_screen();
    }
    else
    {
        draw_end_info(pkt);
    }

    // disable all button temporarily
    disable_button(&poker_screen.buttons[0]);
    disable_button(&poker_screen.buttons[1]);
    disable_button(&poker_screen.buttons[2]);

    draw_button_panel(&poker_screen.buttons[0]);
    write_button_text(&poker_screen.buttons[0], ready_leave_buttons[0]);
    poker_screen.buttons[0].on_click = send_ready;

    draw_button_panel(&poker_screen.buttons[2]);
    write_button_text(&poker_screen.buttons[2], ready_leave_buttons[1]);
    poker_screen.buttons[2].on_click = send_leave;

    // flush input away
    flushinp();

    enable_button(&poker_screen.buttons[0]);
    enable_button(&poker_screen.buttons[2]);

    int ch = 0;
    MEVENT event;
    while (true)
    {
        ch = getch();
        if (ch == KEY_MOUSE)
        {
            getmouse(&event);
            process_all_buttons(&poker_screen, &event);
        }
    }
}

static void draw_poker_info(info_packet_t *pkt)
{
    draw_base_poker_screen();

    char *player_names[] = { 
        "Player 0", "Player 1", "Player 2", 
        "Player 3", "Player 4", "Player 5" 
    };

    // write pot and bet amount
    write_pot_value(&poker_screen, pkt->pot_size);
    write_bet_value(&poker_screen, pkt->bet_size);

    // set player info
    for (player_id_t player_id = 0; player_id < MAX_PLAYERS; ++player_id)
    {
        if (pkt->player_status[player_id] != 2)
        {
            write_player_name(&poker_screen, player_id, player_names[player_id]);
            write_player_stack(&poker_screen, player_id, pkt->player_stacks[player_id]);
            if (pkt->player_status[player_id] == 0)
            {
                write_player_fold(&poker_screen, player_id);
            }
        }
    }

    // set player card
    write_player_card(&poker_screen, id, pkt->player_cards[0], pkt->player_cards[1]);

    // set player dealer 
    write_player_dealer(&poker_screen, pkt->dealer);

    // set player turn
    write_player_turn(&poker_screen, pkt->player_turn);

    // draw community cards
    for (size_t i = 0; i < 5; ++i)
        write_community_card(&poker_screen, i, pkt->community_cards[i]);
}

static void poker_game_screen(info_packet_t *pkt)
{
    // disable all button temporarily
    disable_button(&poker_screen.buttons[0]);
    disable_button(&poker_screen.buttons[1]);
    disable_button(&poker_screen.buttons[2]);

    draw_poker_info(pkt);
    if (is_players_turn(id))
    {
        char *button_names[3] = { " CHECK  ", "   BET   ", "  FOLD  " };

        // set up buttons
        if (pkt->bet_size != 0) 
        {
            button_names[0] = "  CALL  ";
            button_names[1] = "  RAISE  ";
        }

        draw_button_panel(&poker_screen.buttons[0]);
        write_button_text(&poker_screen.buttons[0], button_names[0]);
        poker_screen.buttons[0].on_click = pkt->bet_size == 0 ? send_check : send_call;

        draw_button_panel(&poker_screen.buttons[1]);
        write_button_text(&poker_screen.buttons[1], button_names[1]);
        poker_screen.buttons[1].on_click = send_raise;

        draw_button_panel(&poker_screen.buttons[2]);
        write_button_text(&poker_screen.buttons[2], button_names[2]);
        poker_screen.buttons[2].on_click = send_fold;

        flushinp();

        enable_button(&poker_screen.buttons[0]);
        enable_button(&poker_screen.buttons[1]);
        enable_button(&poker_screen.buttons[2]);

        int ch = 0;
        MEVENT event;
        while (true)
        {
            ch = getch();
            if (ch == KEY_MOUSE)
            {
                getmouse(&event);
                process_all_buttons(&poker_screen, &event);
            }
        }        
    }
    else
    {
        recv_packet(&serv_pkt);
    }
}

static void on_halt()
{
    flushinp();
    disconnect_to_serv();
    log_info("TUI fini.");
    button_module_fini();
    endwin();
    log_fini();
    exit(0);
}

// -------------------- main -------------------- //

// there is a tiny chance that this can stack overflow if the game last long enough 
// its probably fine though :D

// expecting client to be called as ./PROG_NAME player_num
int main(int argc, char *argv[])
{
    int ret;

    log_init("client");
    setlocale(LC_ALL, "");

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

    // attempt to connect to the server
    ret = connect_to_serv(id);
    if (ret == -1) // connection failed 
    {   
        log_err("Failed to connect to server as player %d. Exiting...", id);
        exit(1);        
    }   

    set_on_end_packet_handler(ready_leave_screen);
    set_on_info_packet_handler(poker_game_screen);
    set_on_halt_packet_handler(on_halt);

    WINDOW *main_window = initscr();
    log_info("TUI init.");

    int max_y, max_x;
    getmaxyx(main_window, max_y, max_x);
    if (max_y < 24 || max_x < 80)
    {
        mvprintw(1, 1, "Please make the terminal at least 24 rows by 80 columns large. Press any key to exit...");
        getch();
        disconnect_to_serv();
        log_info("TUI fini.");
        endwin();
        log_fini();
        return 1; 
    }

    keypad(main_window, true);
    button_module_init();
    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);

    init_poker_screen(&poker_screen, main_window);

    ready_leave_screen(NULL);

    flushinp();
    disconnect_to_serv();
    log_info("TUI fini.");
    button_module_fini();
    endwin();
    log_fini();
    return 0; 
}