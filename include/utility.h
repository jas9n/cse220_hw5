#ifndef UTILITY_H
#define UTILITY_H

#include "poker_client.h"  // for card_t

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

#endif