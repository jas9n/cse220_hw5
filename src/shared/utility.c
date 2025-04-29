#include "poker_client.h"

#include <string.h>

static const char *poker_card_names[] = {
    CARD_STR(TWO, DIAMOND),
    CARD_STR(TWO, CLUB),
    CARD_STR(TWO, HEART),
    CARD_STR(TWO, SPADE),
    CARD_STR(THREE, DIAMOND),
    CARD_STR(THREE, CLUB),
    CARD_STR(THREE, HEART),
    CARD_STR(THREE, SPADE),
    CARD_STR(FOUR, DIAMOND),
    CARD_STR(FOUR, CLUB),
    CARD_STR(FOUR, HEART),
    CARD_STR(FOUR, SPADE),
    CARD_STR(FIVE, DIAMOND),
    CARD_STR(FIVE, CLUB),
    CARD_STR(FIVE, HEART),
    CARD_STR(FIVE, SPADE),
    CARD_STR(SIX, DIAMOND),
    CARD_STR(SIX, CLUB),
    CARD_STR(SIX, HEART),
    CARD_STR(SIX, SPADE),
    CARD_STR(SEVEN, DIAMOND),
    CARD_STR(SEVEN, CLUB),
    CARD_STR(SEVEN, HEART),
    CARD_STR(SEVEN, SPADE),
    CARD_STR(EIGHT, DIAMOND),
    CARD_STR(EIGHT, CLUB),
    CARD_STR(EIGHT, HEART),
    CARD_STR(EIGHT, SPADE),
    CARD_STR(NINE, DIAMOND),
    CARD_STR(NINE, CLUB),
    CARD_STR(NINE, HEART),
    CARD_STR(NINE, SPADE),
    CARD_STR(TEN, DIAMOND),
    CARD_STR(TEN, CLUB),
    CARD_STR(TEN, HEART),
    CARD_STR(TEN, SPADE),
    CARD_STR(JACK, DIAMOND),
    CARD_STR(JACK, CLUB),
    CARD_STR(JACK, HEART),
    CARD_STR(JACK, SPADE),
    CARD_STR(QUEEN, DIAMOND),
    CARD_STR(QUEEN, CLUB),
    CARD_STR(QUEEN, HEART),
    CARD_STR(QUEEN, SPADE),
    CARD_STR(KING, DIAMOND),
    CARD_STR(KING, CLUB),
    CARD_STR(KING, HEART),
    CARD_STR(KING, SPADE),
    CARD_STR(ACE, DIAMOND),
    CARD_STR(ACE, CLUB),
    CARD_STR(ACE, HEART),
    CARD_STR(ACE, SPADE)
};

static const wchar_t *fancy_poker_card_names[] = {
    FANCY_CARD_STR(TWO, DIAMOND),
    FANCY_CARD_STR(TWO, CLUB),
    FANCY_CARD_STR(TWO, HEART),
    FANCY_CARD_STR(TWO, SPADE),
    FANCY_CARD_STR(THREE, DIAMOND),
    FANCY_CARD_STR(THREE, CLUB),
    FANCY_CARD_STR(THREE, HEART),
    FANCY_CARD_STR(THREE, SPADE),
    FANCY_CARD_STR(FOUR, DIAMOND),
    FANCY_CARD_STR(FOUR, CLUB),
    FANCY_CARD_STR(FOUR, HEART),
    FANCY_CARD_STR(FOUR, SPADE),
    FANCY_CARD_STR(FIVE, DIAMOND),
    FANCY_CARD_STR(FIVE, CLUB),
    FANCY_CARD_STR(FIVE, HEART),
    FANCY_CARD_STR(FIVE, SPADE),
    FANCY_CARD_STR(SIX, DIAMOND),
    FANCY_CARD_STR(SIX, CLUB),
    FANCY_CARD_STR(SIX, HEART),
    FANCY_CARD_STR(SIX, SPADE),
    FANCY_CARD_STR(SEVEN, DIAMOND),
    FANCY_CARD_STR(SEVEN, CLUB),
    FANCY_CARD_STR(SEVEN, HEART),
    FANCY_CARD_STR(SEVEN, SPADE),
    FANCY_CARD_STR(EIGHT, DIAMOND),
    FANCY_CARD_STR(EIGHT, CLUB),
    FANCY_CARD_STR(EIGHT, HEART),
    FANCY_CARD_STR(EIGHT, SPADE),
    FANCY_CARD_STR(NINE, DIAMOND),
    FANCY_CARD_STR(NINE, CLUB),
    FANCY_CARD_STR(NINE, HEART),
    FANCY_CARD_STR(NINE, SPADE),
    FANCY_CARD_STR(TEN, DIAMOND),
    FANCY_CARD_STR(TEN, CLUB),
    FANCY_CARD_STR(TEN, HEART),
    FANCY_CARD_STR(TEN, SPADE),
    FANCY_CARD_STR(JACK, DIAMOND),
    FANCY_CARD_STR(JACK, CLUB),
    FANCY_CARD_STR(JACK, HEART),
    FANCY_CARD_STR(JACK, SPADE),
    FANCY_CARD_STR(QUEEN, DIAMOND),
    FANCY_CARD_STR(QUEEN, CLUB),
    FANCY_CARD_STR(QUEEN, HEART),
    FANCY_CARD_STR(QUEEN, SPADE),
    FANCY_CARD_STR(KING, DIAMOND),
    FANCY_CARD_STR(KING, CLUB),
    FANCY_CARD_STR(KING, HEART),
    FANCY_CARD_STR(KING, SPADE),
    FANCY_CARD_STR(ACE, DIAMOND),
    FANCY_CARD_STR(ACE, CLUB),
    FANCY_CARD_STR(ACE, HEART),
    FANCY_CARD_STR(ACE, SPADE)
};

static int get_rank(char rank)
{
    switch (rank)
    {
    case '2':
        return TWO;
    case '3':
        return THREE;
    case '4':
        return FOUR;
    case '5':
        return FIVE;
    case '6':
        return SIX;
    case '7':
        return SEVEN;
    case '8':
        return EIGHT;
    case '9':
        return NINE;
    case 'T':
        return TEN;
    case 'J':
        return JACK;
    case 'Q':
        return QUEEN;
    case 'K':
        return KING;
    case 'A':
        return ACE;
    default:
        return -1;
    }
}

static int get_suite(char suite)
{
    switch (suite)
    {
    case 'd':
        return DIAMOND;
    case 'c':
        return CLUB;
    case 'h':
        return HEART;
    case 's':
        return SPADE;
    default:
        return -1;
    }
}

/**
 * @brief converts from the string representation of a card to the integer representation
 * 
 * @param card the string representing the cards
 * @return the corresponding integer value (formed from SUITE | RANK from macro.h) on success.
 *         on failure, return NOCARD
 */
card_t card_id(char *card)
{
    if (strlen(card) != 2) return NOCARD;
    int card_rank = get_rank(card[0]);
    int suite_rank = get_suite(card[1]);
    if (card_rank == -1 || suite_rank == -1) return NOCARD;
    return card_rank | suite_rank;
}

/**
 * @brief converts from a card id to a printable name
 * 
 * @param card the card id to get the corresponding card name
 * @return the null terminated string of the name of the card
 */
const char *card_name(card_t card)
{
    if (card == NOCARD) return S_NOCARD;
    return poker_card_names[card];
}

/**
 * @brief converts from a card id to a fancy printable name using unicode
 * 
 * @param card the card id to get the corresponding card name
 * @return the null terminated string of the name of the card
 */
const wchar_t *fancy_card_name(card_t card)
{
    if (card == NOCARD) return SF_NOCARD;
    return fancy_poker_card_names[card];
}