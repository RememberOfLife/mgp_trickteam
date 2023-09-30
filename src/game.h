#pragma once

#include "surena/game.h"

#ifdef __cplusplus
extern "C" {
#endif

//TODO add challenge reject mode, where single challenges can be rejected and rerolled, this is SM where everyone can move and id or pass and then it is immediately replaced with a random challenge of the same value

typedef enum __attribute__((__packed__)) TRICKTEAM_SUIT_E {
    TRICKTEAM_SUIT_NONE = 0,
    TRICKTEAM_SUIT_BLACK,
    TRICKTEAM_SUIT_YELLOW, // cross
    TRICKTEAM_SUIT_TEAL, // circle //TODO rename this and others, this is blue not teal!
    TRICKTEAM_SUIT_PINK, // square
    TRICKTEAM_SUIT_GREEN, // triangle
    TRICKTEAM_SUIT_COUNT,
    TRICKTEAM_SUIT_FIRST_NON_BLACK = TRICKTEAM_SUIT_BLACK + 1,
    TRICKTEAM_SUIT_SIZE_MAX = 0xFF,
} TRICKTEAM_SUIT;

typedef struct trickteam_card_s {
    TRICKTEAM_SUIT suit;
    uint8_t value;
} trickteam_card;

typedef enum __attribute__((__packed__)) TRICKTEAM_CHALLENGE_TYPE_E {
    TRICKTEAM_CHALLENGE_TYPE_NONE = 0,

    // TRICKTEAM_CHALLENGE_TYPE_WIN_SPECIFIC_WITH_BLACK, // e.g. win a specific card with suit and value with a black card
    // TRICKTEAM_CHALLENGE_TYPE_GET_EEQ_N_BLACK, // e.g. get exactly n black cards
    // TRICKTEAM_CHALLENGE_TYPE_GET_ONLY_SPECIFIC_BLACK, // e.g. get only this specific black card and no other black cards
    // TRICKTEAM_CHALLENGE_TYPE_GET_SPECIFIC_BLACK, // e.g. get specific black with value
    // TRICKTEAM_CHALLENGE_TYPE_GET_NO_BLACK, // e.g. get no black ever

    TRICKTEAM_CHALLENGE_TYPE_GET_SPECIFIC_1, // e.g. get specific card with suit and value
    // TRICKTEAM_CHALLENGE_TYPE_GET_SPECIFIC_2, // same with two specific cards
    // TRICKTEAM_CHALLENGE_TYPE_GET_SPECIFIC_3, // same with three cards
    // TRICKTEAM_CHALLENGE_TYPE_GET_ALL_SUITS_FOR_VALUE, // e.g. get all 9's from all suits
    // TRICKTEAM_CHALLENGE_TYPE_GET_ALL_VALUES_FOR_SUIT, // e.g. get all cards for a suit

    // TRICKTEAM_CHALLENGE_TYPE_NONE_OF_SUIT, // e.g. get no cards of specific suit
    // TRICKTEAM_CHALLENGE_TYPE_NONE_OF_SUIT_2, // same but disallowes two suits
    // TRICKTEAM_CHALLENGE_TYPE_NONE_OF_VALUE, // e.g. get no 9's no matter the suit
    // TRICKTEAM_CHALLENGE_TYPE_NONE_OF_VALUE_2, // same but disallows two values
    // TRICKTEAM_CHALLENGE_TYPE_NONE_OF_VALUE_3, // same but disallows three values

    // TRICKTEAM_CHALLENGE_TYPE_GET_MIN_N_OF_VALUE, // e.g. get min n cards of value no matter the suit
    // TRICKTEAM_CHALLENGE_TYPE_GET_EEQ_N_OF_VALUE, // e.g. get exactly n cards of value no matter the suit
    // TRICKTEAM_CHALLENGE_TYPE_GET_MIN_N_OF_SUIT, // e.g. get min n cards of suit no matter the value
    // TRICKTEAM_CHALLENGE_TYPE_GET_EEQ_N_OF_SUIT, // e.g. get exactly n cards of suit no matter the value
    // TRICKTEAM_CHALLENGE_TYPE_GET_EEQ_N_OF_SUIT_2, // e.g. get exactly n cards of two suits each no matter the value
    // TRICKTEAM_CHALLENGE_TYPE_GET_ONE_OF_ALL_SUITS, // e.g. win at least one card of every suit

    // TRICKTEAM_CHALLENGE_TYPE_WIN_TRICK_WITH_VALUE, // e.g. win a trick with card of value no matter the suit
    // TRICKTEAM_CHALLENGE_TYPE_WIN_VALUE_WITH_VALUE, // e.g. win a card of value1 with a card of value2 (if value1==value2 then different suits)
    // TRICKTEAM_CHALLENGE_TYPE_WIN_SPECIFIC_IN_LAST_TRICK, // e.g. win a specific card, in the last trick of the game

    // TRICKTEAM_CHALLENGE_TYPE_NEVER_LEAD_SUIT_2, // e.g. never lead a trick with these two suits
    // TRICKTEAM_CHALLENGE_TYPE_NEVER_LEAD_SUIT_3, // same with three suits

    // TRICKTEAM_CHALLENGE_TYPE_WIN_TRICK_SUM_MIN, // e.g. win a trick where the sum of all values is min n (no black)
    // TRICKTEAM_CHALLENGE_TYPE_WIN_TRICK_SUM_MAX, // e.g. win a trick where the sum of all values is max n (no black)
    // TRICKTEAM_CHALLENGE_TYPE_WIN_TRICK_SUM_EEQ, // e.g. win a trick where the sum of all values is exactly n (no black)
    // TRICKTEAM_CHALLENGE_TYPE_WIN_TRICK_ALL_VALUES_MIN, // e.g. win a trick where all values are min n (no black)
    // TRICKTEAM_CHALLENGE_TYPE_WIN_TRICK_ALL_VALUES_MAX, // e.g. win a trick where all values are max n (no black)

    // TRICKTEAM_CHALLENGE_TYPE_WIN_TRICK_VALUES_EVEN, // e.g. win a trick where all values are even (no black)
    // TRICKTEAM_CHALLENGE_TYPE_WIN_TRICK_VALUES_ODD, // e.g. win a trick where all values are odd (no black)
    // TRICKTEAM_CHALLENGE_TYPE_WIN_TRICK_EEQ_SUIT_CARDS, // e.g. win a trick with same number of cards of suit1 as of suit2
    // TRICKTEAM_CHALLENGE_TYPE_GET_MORE_SUIT_CARDS, // e.g. overall, get more cards of suit1 than of suit2
    // TRICKTEAM_CHALLENGE_TYPE_GET_EEQ_SUIT_CARDS, // e.g. overall, get the same number of cards of suit1 as of suit2

    // TRICKTEAM_CHALLENGE_TYPE_WIN_X_TRICKS, // e.g. win exactly x tricks (where x is anounced before play)
    // TRICKTEAM_CHALLENGE_TYPE_WIN_EEQ_N_TRICKS, // e.g. win exactly n tricks

    // TRICKTEAM_CHALLENGE_TYPE_WIN_LAST_TRICK, // e.g. win at least last trick
    // TRICKTEAM_CHALLENGE_TYPE_WIN_ONLY_LAST_TRICK, // e.g. win only the last trick
    // TRICKTEAM_CHALLENGE_TYPE_WIN_FIRST_TRICK, // e.g. win at least the first trick
    // TRICKTEAM_CHALLENGE_TYPE_WIN_ONLY_FIRST_TRICK, // e.g. win only the first trick
    // TRICKTEAM_CHALLENGE_TYPE_WIN_FIRST_AND_LAST_TRICK, // e.g. win at least the first and last trick
    // TRICKTEAM_CHALLENGE_TYPE_WIN_FIRST_N_TRICKS, // e.g. win at least the first n tricks
    // TRICKTEAM_CHALLENGE_TYPE_NONE_FIRST_N_TRICKS, // e.g. win none of the first n tricks
    // TRICKTEAM_CHALLENGE_TYPE_WIN_TRICK_STREAK_N, // e.g. win at least n tricks in a streak
    // TRICKTEAM_CHALLENGE_TYPE_WIN_EEQ_N_TRICKS_STREAK, // e.g. win exactly n tricks and those in a streak
    // TRICKTEAM_CHALLENGE_TYPE_NEVER_TRICK_STREAK_N, // e.g. never win at least n tricks in a streak

    // TRICKTEAM_CHALLENGE_TYPE_WIN_MORE_TRICKS_THAN_CAPTAIN, // e.g. overall, win more tricks than the captain
    // TRICKTEAM_CHALLENGE_TYPE_WIN_LESS_TRICKS_THAN_CAPTAIN, // same but less
    // TRICKTEAM_CHALLENGE_TYPE_WIN_SAME_TRICKS_AS_CAPTAIN, // same but win exactly the same amount of tricks
    // TRICKTEAM_CHALLENGE_TYPE_WIN_LESS_TRICKS_AS_EVERY_OTHER, // e.g. win less tricks than every other individual player
    // TRICKTEAM_CHALLENGE_TYPE_WIN_MORE_TRICKS_AS_EVERY_OTHER, // e.g. win more tricks than every other individual player
    // TRICKTEAM_CHALLENGE_TYPE_WIN_MORE_TRICKS_THAN_COMBINED_OTHERS, // e.g. win more tricks than all other players combined

    TRICKTEAM_CHALLENGE_TYPE_COUNT,
    TRICKTEAM_CHALLENGE_TYPE_FIRST = TRICKTEAM_CHALLENGE_TYPE_NONE + 1,
    TRICKTEAM_CHALLENGE_TYPE_SIZE_MAX = 0xFFFF,
} TRICKTEAM_CHALLENGE_TYPE;

typedef enum __attribute__((__packed__)) TRICKTEAM_CHALLENGE_STATE_E {
    TRICKTEAM_CHALLENGE_STATE_IDLE = 0,
    TRICKTEAM_CHALLENGE_STATE_PENDING_FAIL,
    TRICKTEAM_CHALLENGE_STATE_PENDING_PASS,
    TRICKTEAM_CHALLENGE_STATE_FAIL,
    TRICKTEAM_CHALLENGE_STATE_PASS,
    TRICKTEAM_CHALLENGE_STATE_COUNT,
    TRICKTEAM_CHALLENGE_STATE_SIZE_MAX = 0xFF,
} TRICKTEAM_CHALLENGE_STATE;

typedef struct trickteam_challenge_s {
    TRICKTEAM_CHALLENGE_TYPE type;
    TRICKTEAM_CHALLENGE_STATE state;

    union {
        struct {
            trickteam_card target;
        } get_specific_1;
    } du;
} trickteam_challenge;

typedef enum __attribute__((__packed__)) TRICKTEAM_COMMUNICATION_E {
    TRICKTEAM_COMMUNICATION_NONE = 0,
    TRICKTEAM_COMMUNICATION_LOWEST,
    TRICKTEAM_COMMUNICATION_SINGULAR,
    TRICKTEAM_COMMUNICATION_HIGHEST,
    TRICKTEAM_COMMUNICATION_COUNT,
    TRICKTEAM_COMMUNICATION_SIZE_MAX = 0xFF,
} TRICKTEAM_COMMUNICATION;

typedef struct trickteam_communication_s {
    TRICKTEAM_COMMUNICATION type;
    trickteam_card card;
} trickteam_communication;

typedef struct trickteam_trick_s {
    uint8_t order_id;
    player_id lead_player;
    TRICKTEAM_SUIT lead_suit;
    trickteam_card* cards;
    player_id winning_player;
    trickteam_card winning_card;
} trickteam_trick;

typedef struct trickteam_player_s {
    player_id id;
    bool captain;
    trickteam_challenge* challenges;
    trickteam_card* cards;
    trickteam_communication* communications;
    bool communication_pass;
    bool hidden_info; // true if the hidden info for this player is known
    // trickteam_trick* tricks; //TODO does anyone really need this?
    //TODO infered infos
    // bool blank[TRICKTEAM_SUIT_COUNT - 1]; // stores inferred info on wether player still has *any* cards of suit
} trickteam_player;

typedef struct trickteam_options_s {
    uint8_t player_count; // 3,4,5
    bool allow_communication; // if enabled allows players to communicate
    bool value_communication; // if enabled allows players to communicate values
    uint8_t communication_limit; // if 0 means not used and every player just gets 1
    // bool allow_distress; //TODO SM
    // bool free_task_selection; //TODO SM
    //TODO X players get all the tasks
    // bool real_time; //TODO
    //TODO tt1 mode where only "get x card" challenges are drafted
    //TODO tt2 faithful mode where only the challenges in their og base game variation are offered for draw
    uint8_t challenge_points;
    trickteam_challenge* challenge_list;
} trickteam_options;

typedef enum __attribute__((__packed__)) TRICKTEAM_STATE_E {
    TRICKTEAM_STATE_NONE = 0,
    TRICKTEAM_STATE_DEALING_CHALLENGES,
    TRICKTEAM_STATE_DEALING_CARDS,
    TRICKTEAM_STATE_CHALLENGE_SELECTING,
    TRICKTEAM_STATE_CHALLENGE_SPECIFYING,
    TRICKTEAM_STATE_COMMUNICATING,
    TRICKTEAM_STATE_TRICK_PLAYING,
    TRICKTEAM_STATE_DONE,
    TRICKTEAM_STATE_COUNT,
    TRICKTEAM_STATE_SIZE_MAX = 0xFF,
} TRICKTEAM_STATE;

typedef struct trickteam_state_s {
    uint8_t challenge_points;
    trickteam_challenge* challenge_pool;
    trickteam_card* deck; // for dealing out cards
    bool deck_hidden_info;
    uint8_t chall_sel_open_passes;
    // trickteam_card* expended_deck; // for tracking discretize state
    trickteam_player* players;
    TRICKTEAM_STATE state;
    trickteam_trick previous_trick;
    trickteam_trick current_trick;
    uint8_t current_trick_id;
    uint8_t max_tricks;
    player_id lead_player;
    uint8_t open_communications;
    bool win;
} trickteam_state;

typedef struct trickteam_internal_methods_s {

    // msb ... lsb
    // G P T Y .. B2 B3 B4
    // i.e. values decreasing from lsb to msb and suits going from black to green as in the enum
    // only ever call this when the hidden info on the players cards is available
    uint64_t (*get_player_sorted_cards_bitfield_igf)(game* self, player_id player);

} trickteam_internal_methods;

extern const game_methods trickteam_standard_gbe;

#ifdef __cplusplus
}
#endif
