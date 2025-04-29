#ifndef POKER_MACROS_H
#define POKER_MACROS_H

/**
 * lower two bits - suite of the card
 * upper bits - the rank of the card
 *
 * how to create a cards:
 *  TWO | DIAMOND (two of diamonds)
 *  ACE | SPADE (ace of spades)
 * etc
 *
 * NOTE: cards are automatically ordered, i.e. ace of spaces > ace of diamonds >
 * two of hearts
 * 
 * if you want to make this more cursed, you can do
 *  TWO OF SPADE
 * because OF is a macro for |
 */

#define OF |

#define SUITE_BITS  2
#define DIAMOND     0
#define CLUB        1
#define HEART       2
#define SPADE       3

#define TWO         (0 << SUITE_BITS)
#define THREE       (1 << SUITE_BITS)
#define FOUR        (2 << SUITE_BITS)
#define FIVE        (3 << SUITE_BITS)
#define SIX         (4 << SUITE_BITS)
#define SEVEN       (5 << SUITE_BITS)
#define EIGHT       (6 << SUITE_BITS)
#define NINE        (7 << SUITE_BITS)
#define TEN         (8 << SUITE_BITS)
#define JACK        (9 << SUITE_BITS)
#define QUEEN       (10 << SUITE_BITS)
#define KING        (11 << SUITE_BITS)
#define ACE         (12 << SUITE_BITS)

#define SUITE(card) (card & ((1 << SUITE_BITS) - 1))
#define RANK(card)  (card >> SUITE_BITS)

// for an nonexistent card...
#define NOCARD (-1)

/**
 * retrieve the string representation
 *
 * POKER_STR(n) returns the string representation of a rank or a suite
 *      POKER_STR(DIAMOND) = "d"
 *      POKER_STR(ACE) = "A"
 *      POKER_STR(TEN) = "T"
 * CARD_STR(rank, suite) returns the string representation of a card
 *      CARD_STR(ACE, DIAMOND) = "Ad"
 *      CARD_STR(THREE, SPADE) = "3s"
 */

#define S_          ""

#define S_DIAMOND   "d"
#define S_CLUB      "c"
#define S_HEART     "h"
#define S_SPADE     "s"

#define S_TWO       "2"
#define S_THREE     "3"
#define S_FOUR      "4"
#define S_FIVE      "5"
#define S_SIX       "6"
#define S_SEVEN     "7"
#define S_EIGHT     "8"
#define S_NINE      "9"
#define S_TEN       "T"
#define S_JACK      "J"
#define S_QUEEN     "Q"
#define S_KING      "K"
#define S_ACE       "A"

#define S_NOCARD    ""

#define POKER_STR(n) S_##n
#define CARD_STR(rank, suite) S_##rank S_##suite

#define SF_TWO       L"2"
#define SF_THREE     L"3"
#define SF_FOUR      L"4"
#define SF_FIVE      L"5"
#define SF_SIX       L"6"
#define SF_SEVEN     L"7"
#define SF_EIGHT     L"8"
#define SF_NINE      L"9"
#define SF_TEN       L"T"
#define SF_JACK      L"J"
#define SF_QUEEN     L"Q"
#define SF_KING      L"K"
#define SF_ACE       L"A"

#define SF_DIAMOND   L"♦"
#define SF_CLUB      L"♣"
#define SF_HEART     L"♥"
#define SF_SPADE     L"♠"

#define SF_NOCARD    L""

#define FANCY_CARD_STR(rank, suite) SF_##rank SF_##suite

#define DECK_SIZE 52

#endif