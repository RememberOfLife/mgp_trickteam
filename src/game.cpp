#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define ROSALIA_SEMVER_STATIC
#define ROSALIA_SEMVER_IMPLEMENTATION
#include "rosalia/semver.h"
#define ROSALIA_VECTOR_STATIC
#define ROSALIA_VECTOR_IMPLEMENTATION
#include "rosalia/vector.h"

#include "surena/game.h"

#include "game.h"

// general purpose helpers for opts, data, bufs

struct export_buffers {
    char* options;
    char* state;
    player_id* players_to_move;
    move_data* concrete_moves;
    float* move_probabilities;
    move_data* actions;
    player_id* results;
    move_data_sync move_out;
    char* move_str;
    char* print;
};

typedef trickteam_options opts_repr;

typedef trickteam_state state_repr;

struct game_data {
    export_buffers bufs;
    opts_repr opts;
    state_repr state;
};

export_buffers& get_bufs(game* self)
{
    return ((game_data*)(self->data1))->bufs;
}

opts_repr& get_opts(game* self)
{
    return ((game_data*)(self->data1))->opts;
}

state_repr& get_repr(game* self)
{
    return ((game_data*)(self->data1))->state;
}

/* use these in your functions for easy access
export_buffers& bufs = get_bufs(self);
opts_repr& opts = get_opts(self);
state_repr& data = get_repr(self);
*/

#ifdef __cplusplus
extern "C" {
#endif

// impl internal declarations
static bool card_eq_igf(trickteam_card left, trickteam_card right);
static void player_create_igf(trickteam_player* player, player_id id);
static void player_destroy_igf(trickteam_player* player);
static void trick_create_igf(trickteam_trick* trick, uint8_t order_id, player_id lead_player);
static void trick_destroy_igf(trickteam_trick* trick);

static TRICKTEAM_CHALLENGE_STATE check_challenge_igf(game* self, trickteam_player* player, trickteam_challenge* challenge, trickteam_trick done_trick);
static bool challenge_needs_specification_igf(trickteam_challenge challenge, bool selecting_not_dealing);
static uint32_t challenge_specification_get_concrete_moves_igf(move_data* moves, trickteam_challenge challenge);
static uint32_t challenge_specification_get_concrete_move_probabilities_igf(float* probabilities, trickteam_challenge challenge);
static bool challenge_specification_is_legal_igf(move_data move, trickteam_challenge challenge);
static void challenge_specification_make_move_igf(move_data move, trickteam_challenge* challenge);
static error_code challenge_specification_get_move_data_igf(const char* str, move_data* move, trickteam_challenge challenge);
static size_t challenge_specification_get_move_str_igf(char* str, move_data move, trickteam_challenge challenge);
// these two are exactly for opts and state and print and NOT for move gen etc dealing..
static trickteam_challenge sscanf_challenge_igf(const char* str, const char* str_end); // for scanning from opts and state
static size_t sprintf_challenge_igf(char* str, trickteam_challenge challenge); // for exporting opts, state and print

struct challenge_info {
    char str_id[32];
    uint32_t weigth; // 1 is default
    uint8_t challenge_rating[3]; // for 3,4,5 players
};

static challenge_info get_challenge_info(TRICKTEAM_CHALLENGE_TYPE type); // sadly array designator are not a cpp thing

static uint64_t get_player_sorted_cards_bitfield_igf(game* self, player_id player);

static char comm_2_char[TRICKTEAM_COMMUNICATION_COUNT] = {'~', 'v', '*', '^'}; //TODO maybe use - * + instead?
static char suit_2_char[TRICKTEAM_SUIT_COUNT] = {'?', 'B', 'Y', 'T', 'P', 'G'};
static char val_2_char[10] = {'?', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

static TRICKTEAM_COMMUNICATION char_2_comm(char c);
static TRICKTEAM_SUIT char_2_suit(char c);
static uint8_t char_2_val(char c);

// need internal function pointer struct here
static const trickteam_internal_methods trickteam_gbe_internal_methods{
    .get_player_sorted_cards_bitfield_igf = get_player_sorted_cards_bitfield_igf,
};

// declare and form game
#define SURENA_GDD_BENAME trickteam_standard_gbe
#define SURENA_GDD_GNAME "TrickTeam"
#define SURENA_GDD_VNAME "Standard"
#define SURENA_GDD_INAME "mirabel_supplemental"
#define SURENA_GDD_VERSION ((semver){0, 1, 0})
#define SURENA_GDD_INTERNALS &trickteam_gbe_internal_methods
#define SURENA_GDD_FF_OPTIONS
#define SURENA_GDD_FF_RANDOM_MOVES
#define SURENA_GDD_FF_HIDDEN_INFORMATION
#define SURENA_GDD_FF_SIMULTANEOUS_MOVES
#define SURENA_GDD_FF_PRINT
#include "surena/game_decldef.h"

// implementation

// static const char* get_last_error(game* self)
// {
//     return (char*)self->data2;
// }

static error_code create_gf(game* self, game_init* init_info)
{
    self->data1 = malloc(sizeof(game_data));
    if (self->data1 == NULL) {
        return ERR_OUT_OF_MEMORY;
    }
    self->data2 = NULL;

    opts_repr& opts = get_opts(self);
    opts.player_count = 3;
    opts.allow_communication = true;
    opts.value_communication = true;
    opts.communication_limit = 0;
    opts.challenge_points = 8;
    opts.challenge_list = NULL;
    if (init_info->source_type == GAME_INIT_SOURCE_TYPE_STANDARD && init_info->source.standard.opts != NULL) {
        /*
        trickteam options string format EBNF:
        <options> = <player_count>, " ", <allow_coms>, <val_coms>, <com_limit>, " ", <challenge_info>;

        <player_count>: = "3" | "4" | "5";
        <allow_coms> = "C" | "-";
        <val_coms> = "V" | "-";
        <com_limit> = "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" | "-";
        <challenge_info> = ( "P", <challenge_points> ) | ( "L", <challenge_list> )
        <challenge_points> = <uint8_t>
        <challenge_list> = <challenge>, { ",", <challenge> }
        <challenge> = "<", <challenge_str_id>, "/", <challenge_specification>, ">"
        //TODO attention: if we allow non fully specified challenges in the challenge list, we need an extra specification mode for cycling through them with player rand!
        */
        const char* wstr = init_info->source.standard.opts;

        char player_count = '\0';
        char allow_coms = '\0';
        char value_coms = '\0';
        char com_limit = '\0';
        int ec = sscanf(wstr, "%c %c%c%c", &player_count, &allow_coms, &value_coms, &com_limit);
        if (ec != 4) {
            free(self->data1);
            self->data1 = NULL;
            return ERR_INVALID_INPUT;
        }
        wstr += 5;

        switch (player_count) {
            case '3':
            case '4':
            case '5': {
                opts.player_count = player_count - '0';
            } break;
            default: {
                free(self->data1);
                self->data1 = NULL;
                return ERR_INVALID_INPUT;
            } break;
        }

        if (allow_coms != 'C' && allow_coms != '-') {
            free(self->data1);
            self->data1 = NULL;
            return ERR_INVALID_INPUT;
        }
        opts.allow_communication = (allow_coms == 'C');

        if (value_coms != 'V' && value_coms != '-') {
            free(self->data1);
            self->data1 = NULL;
            return ERR_INVALID_INPUT;
        }
        opts.value_communication = (value_coms == 'V');

        if (com_limit != '-' && (com_limit < '1' || com_limit > '9')) {
            free(self->data1);
            self->data1 = NULL;
            return ERR_INVALID_INPUT;
        }
        opts.value_communication = (com_limit == '-' ? 0 : com_limit - '0');

        // challenges info
        char challenge_info;
        ec = sscanf(wstr, " %c", &challenge_info);
        if (ec != 1) {
            free(self->data1);
            self->data1 = NULL;
            return ERR_INVALID_INPUT;
        }
        wstr += 2;
        if (challenge_info == 'P') {
            ec = sscanf(wstr, "%hhu", &opts.challenge_points);
            if (ec != 1) {
                free(self->data1);
                self->data1 = NULL;
                return ERR_INVALID_INPUT;
            }
        } else if (challenge_info == 'L') {
            //TODO read list of challenges
        } else {
            free(self->data1);
            self->data1 = NULL;
            return ERR_INVALID_INPUT;
        }
    }
    //TODO sanity check things here if wanted

    {
        // init state
        state_repr& data = get_repr(self);
        data.challenge_pool = NULL;
        data.deck = NULL;
        VEC_CREATE(&data.players, opts.player_count);
        VEC_PUSH_N(&data.players, opts.player_count);
        for (size_t player_idx = 0; player_idx < VEC_LEN(&data.players); player_idx++) {
            player_create_igf(&data.players[player_idx], player_idx + 1);
        }
        trick_create_igf(&data.previous_trick, 0, 0); //TODO make sure everyone else also obeys the fact that these are always at least initialized
        trick_create_igf(&data.current_trick, 0, 0);
    }

    {
        export_buffers& bufs = get_bufs(self);
        bufs.options = (char*)malloc(6 * sizeof(char));
        bufs.state = (char*)malloc(1000 * sizeof(char));
        bufs.players_to_move = (player_id*)malloc(opts.player_count * sizeof(player_id));
        bufs.concrete_moves = (move_data*)malloc(40 * sizeof(move_data));
        bufs.move_probabilities = (float*)malloc(40 * sizeof(float));
        bufs.actions = (move_data*)malloc(10 * sizeof(move_data));
        bufs.results = (player_id*)malloc(opts.player_count * sizeof(player_id));
        bufs.move_str = (char*)malloc(10 * sizeof(char));
        bufs.print = (char*)malloc(1000 * sizeof(char));
        if (bufs.state == NULL ||
            bufs.players_to_move == NULL ||
            bufs.concrete_moves == NULL ||
            bufs.move_probabilities == NULL ||
            bufs.actions == NULL ||
            bufs.results == NULL ||
            bufs.move_str == NULL ||
            bufs.print == NULL) {
            destroy_gf(self);
            return ERR_OUT_OF_MEMORY;
        }
    }
    const char* initial_state = NULL;
    if (init_info->source_type == GAME_INIT_SOURCE_TYPE_STANDARD) {
        initial_state = init_info->source.standard.state;
    }
    return import_state_gf(self, initial_state);
}

static error_code destroy_gf(game* self)
{
    {
        // destroy state_repr
        opts_repr& opts = get_opts(self);
        VEC_DESTROY(&opts.challenge_list);
        state_repr& data = get_repr(self);
        VEC_DESTROY(&data.challenge_pool);
        VEC_DESTROY(&data.deck);
        for (size_t player_idx = 0; player_idx < VEC_LEN(&data.players); player_idx++) {
            player_destroy_igf(&data.players[player_idx]);
        }
        {
            //TODO do not destroy if tricks are moved!
            trick_destroy_igf(&data.previous_trick);
            trick_destroy_igf(&data.current_trick);
        }
    }
    {
        export_buffers& bufs = get_bufs(self);
        free(bufs.options);
        free(bufs.state);
        free(bufs.players_to_move);
        free(bufs.concrete_moves);
        free(bufs.results);
        free(bufs.move_str);
        free(bufs.print);
    }
    free(self->data2);
    self->data2 = NULL;
    return ERR_OK;
}

static error_code clone_gf(game* self, game* clone_target)
{
    //TODO this should be a disableable feature >:(
    return ERR_FEATURE_UNSUPPORTED;
}

static error_code copy_from_gf(game* self, game* other)
{
    //TODO this should be a disableable feature >:(
    return ERR_FEATURE_UNSUPPORTED;
}

static error_code compare_gf(game* self, game* other, bool* ret_equal)
{
    //TODO this should be a disableable feature >:(
    return ERR_FEATURE_UNSUPPORTED;
}

static error_code export_options_gf(game* self, player_id player, size_t* ret_size, const char** ret_str)
{
    opts_repr& opts = get_opts(self);
    export_buffers& bufs = get_bufs(self);
    char* outbuf = bufs.options;
    {
        outbuf += sprintf(
            outbuf,
            "%c %c%c%c",
            '0' + opts.player_count,
            opts.allow_communication == true ? 'C' : '-',
            opts.value_communication == true ? 'V' : '-',
            opts.communication_limit > 0 ? '0' + opts.communication_limit : '-'
        );
        //TODO more export regarding challenges
    }
    *ret_size = outbuf - bufs.options;
    *ret_str = bufs.options;
    return ERR_OK;
}

static error_code player_count_gf(game* self, uint8_t* ret_count)
{
    opts_repr& opts = get_opts(self);
    *ret_count = opts.player_count;
    return ERR_OK;
}

static error_code export_state_gf(game* self, player_id player, size_t* ret_size, const char** ret_str)
{
    opts_repr& opts = get_opts(self);
    state_repr& data = get_repr(self);
    export_buffers& bufs = get_bufs(self);
    char* outbuf = bufs.state;
    {
        outbuf += sprintf(outbuf, ""); //TODO
    }
    *ret_size = outbuf - bufs.state;
    *ret_str = bufs.state;
    return ERR_OK;
}

static error_code import_state_gf(game* self, const char* str)
{
    opts_repr& opts = get_opts(self);
    state_repr& data = get_repr(self);
    data.challenge_points = opts.challenge_points;
    VEC_DESTROY(&data.challenge_pool);
    VEC_CLONE(&data.challenge_pool, &opts.challenge_list);
    VEC_DESTROY(&data.deck);
    VEC_CREATE(&data.deck, 40);
    for (TRICKTEAM_SUIT suit = TRICKTEAM_SUIT_BLACK; suit < TRICKTEAM_SUIT_COUNT; suit = (TRICKTEAM_SUIT)((int)suit + 1)) {
        uint8_t suit_max_value = (suit == TRICKTEAM_SUIT_BLACK ? 4 : 9);
        for (uint8_t value = 1; value <= suit_max_value; value++) {
            trickteam_card add_card = (trickteam_card){
                .suit = suit,
                .value = value,
            };
            VEC_PUSH(&data.deck, add_card);
        }
    }
    data.deck_hidden_info = true;
    data.chall_sel_open_passes = 0;
    VEC_SETLEN(&data.players, opts.player_count);
    for (uint8_t player_idx = 0; player_idx < VEC_LEN(&data.players); player_idx++) {
        player_destroy_igf(&data.players[player_idx]);
        player_create_igf(&data.players[player_idx], player_idx + 1);
    }
    data.state = data.challenge_points > 0 ? TRICKTEAM_STATE_DEALING_CHALLENGES : TRICKTEAM_STATE_DEALING_CARDS;
    trick_destroy_igf(&data.previous_trick);
    trick_create_igf(&data.previous_trick, 0, 0);
    trick_destroy_igf(&data.current_trick);
    trick_create_igf(&data.current_trick, 0, 0); // captains id will be assigned during card dealing
    data.current_trick_id = 0;
    data.max_tricks = (uint8_t[]){13, 10, 8}[opts.player_count - 3]; // for 3/4/5 players
    data.lead_player = PLAYER_RAND;
    data.open_communications = (opts.communication_limit > 0 ? opts.communication_limit : opts.player_count);
    data.win = false;
    if (str == NULL) {
        return ERR_OK;
    }
    /*
    trickteam state string format EBNF:
    //TODO this is all old and wrong
    <state> = <player_count>, " ", <allow_coms>, <val_coms>, <com_limit>;

    <player_count>: = "3" | "4" | "5";
    <allow_coms> = "C" | "-";
    <val_coms> = "V" | "-";
    <com_limit> = "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" | "-";

    ---
    state stores 


    TRICKTEAM_STATE_DEALING_CHALLENGES,
    TRICKTEAM_STATE_DEALING_CARDS,
    TRICKTEAM_STATE_CHALLENGE_SELECTING,
    TRICKTEAM_STATE_CHALLENGE_SPECIFYING,
    TRICKTEAM_STATE_COMMUNICATING,
    TRICKTEAM_STATE_TRICK_PLAYING,
    TRICKTEAM_STATE_DONE,
DCH
DCD
CSE
CSP
COM
TRP
DON

    */
    //TODO
    return ERR_OK;
}

static error_code players_to_move_gf(game* self, uint8_t* ret_count, const player_id** ret_players)
{
    opts_repr& opts = get_opts(self);
    state_repr& data = get_repr(self);
    export_buffers& bufs = get_bufs(self);
    player_id* outbuf = bufs.players_to_move;
    uint32_t count = 0;
    switch (data.state) {
        case TRICKTEAM_STATE_NONE: {
            assert(0);
        } break;
        case TRICKTEAM_STATE_DEALING_CHALLENGES: {
            outbuf[count++] = PLAYER_RAND;
        } break;
        case TRICKTEAM_STATE_DEALING_CARDS: {
            outbuf[count++] = PLAYER_RAND;
        } break;
        case TRICKTEAM_STATE_CHALLENGE_SELECTING: {
            outbuf[count++] = data.lead_player;
        } break;
        case TRICKTEAM_STATE_CHALLENGE_SPECIFYING: {
            outbuf[count++] = data.lead_player;
        } break;
        case TRICKTEAM_STATE_COMMUNICATING: {
            for (size_t i = 0; i < VEC_LEN(&data.players); i++) {
                if (data.players[i].communication_pass == false) {
                    outbuf[count++] = data.players[i].id;
                }
            }
        } break;
        case TRICKTEAM_STATE_TRICK_PLAYING: {
            outbuf[count++] = data.lead_player;
        } break;
        case TRICKTEAM_STATE_DONE: {
            // pass, game is over, nobody to move
        } break;
        case TRICKTEAM_STATE_COUNT:
        case TRICKTEAM_STATE_SIZE_MAX: {
            assert(0);
        } break;
    }
    *ret_count = count;
    *ret_players = bufs.players_to_move;
    return ERR_OK;
}

static error_code get_concrete_moves_gf(game* self, player_id player, uint32_t* ret_count, const move_data** ret_moves)
{
    opts_repr& opts = get_opts(self);
    state_repr& data = get_repr(self);
    export_buffers& bufs = get_bufs(self);
    move_data* outbuf = bufs.concrete_moves;
    uint32_t count = 0;
    switch (data.state) {
        case TRICKTEAM_STATE_NONE: {
            assert(0);
        } break;
        case TRICKTEAM_STATE_DEALING_CHALLENGES: {
            // deal random challenge, according to the amount of challenge points left to fill
            for (TRICKTEAM_CHALLENGE_TYPE chall_type = TRICKTEAM_CHALLENGE_TYPE_FIRST; chall_type < TRICKTEAM_CHALLENGE_TYPE_COUNT; chall_type = (TRICKTEAM_CHALLENGE_TYPE)((uint32_t)chall_type + 1)) {
                if (data.challenge_points >= get_challenge_info(chall_type).challenge_rating[opts.player_count - 3]) {
                    outbuf[count++] = game_e_create_move_small((uint32_t)chall_type);
                }
            }
        } break;
        case TRICKTEAM_STATE_DEALING_CARDS: {
            if (data.deck_hidden_info == false) {
                return ERR_MISSING_HIDDEN_STATE;
            }
            for (size_t i = 0; i < VEC_LEN(&data.deck); i++) {
                trickteam_card card = data.deck[i];
                outbuf[count++] = game_e_create_move_small(
                    ((move_code)card.suit << 8) |
                    (move_code)card.value
                );
            }
        } break;
        case TRICKTEAM_STATE_CHALLENGE_SELECTING: {
            for (size_t chall_idx = 0; chall_idx < VEC_LEN(&data.challenge_pool); chall_idx++) {
                outbuf[count++] = game_e_create_move_small(chall_idx + 1);
            }
            if (data.chall_sel_open_passes > 0) {
                // passing selecting is done by 0
                outbuf[count++] = game_e_create_move_small(0);
            }
        } break;
        case TRICKTEAM_STATE_CHALLENGE_SPECIFYING: {
            // when specifying
            // for player rand dealing challenges, its the last one in the challenge deck
            // for a real player, its the last in their personal deck
            count += challenge_specification_get_concrete_moves_igf(outbuf, player == PLAYER_RAND ? VEC_LAST(&data.challenge_pool) : VEC_LAST(&data.players[player - 1].challenges));
        } break;
        case TRICKTEAM_STATE_COMMUNICATING: {
            if (data.players[player - 1].hidden_info == false) {
                return ERR_MISSING_HIDDEN_STATE;
            }
            uint8_t highest_vals[TRICKTEAM_SUIT_COUNT - 1] = {0, 0, 0, 0, 0};
            uint8_t lowest_vals[TRICKTEAM_SUIT_COUNT - 1] = {5, 10, 10, 10, 10};
            bool allowed_suits[TRICKTEAM_SUIT_COUNT - 1] = {0, 0, 0, 0, 0};
            for (size_t i = 0; i < VEC_LEN(&data.players[player - 1].cards); i++) {
                trickteam_card card = data.players[player - 1].cards[i];
                int suit_idx = (int)card.suit - 1;
                allowed_suits[suit_idx] |= true;
                if (card.value > highest_vals[suit_idx]) {
                    highest_vals[suit_idx] = card.value;
                }
                if (card.value < lowest_vals[suit_idx]) {
                    lowest_vals[suit_idx] = card.value;
                }
            }
            allowed_suits[TRICKTEAM_SUIT_BLACK - 1] = false; // black can never be communicated
            for (int i = 0; i < TRICKTEAM_SUIT_COUNT - 1; i++) {
                if (allowed_suits[i] == true) {
                    if (highest_vals[i] == lowest_vals[i]) {
                        TRICKTEAM_COMMUNICATION com = opts.value_communication == true ? TRICKTEAM_COMMUNICATION_SINGULAR : TRICKTEAM_COMMUNICATION_NONE;
                        outbuf[count++] = game_e_create_move_small(
                            ((move_code)TRICKTEAM_COMMUNICATION_SINGULAR << 16) |
                            (((move_code)i + 1) << 8) |
                            (move_code)highest_vals[i]
                        );
                    } else {
                        if (highest_vals[i] < 10) {
                            TRICKTEAM_COMMUNICATION com = opts.value_communication == true ? TRICKTEAM_COMMUNICATION_HIGHEST : TRICKTEAM_COMMUNICATION_NONE;
                            outbuf[count++] = game_e_create_move_small(
                                ((move_code)com << 16) |
                                (((move_code)i + 1) << 8) |
                                (move_code)highest_vals[i]
                            );
                        }
                        if (lowest_vals[i] > 0) {
                            TRICKTEAM_COMMUNICATION com = opts.value_communication == true ? TRICKTEAM_COMMUNICATION_LOWEST : TRICKTEAM_COMMUNICATION_NONE;
                            outbuf[count++] = game_e_create_move_small(
                                ((move_code)com << 16) |
                                (((move_code)i + 1) << 8) |
                                (move_code)lowest_vals[i]
                            );
                        }
                    }
                }
            }
            // passing communication is done by communicating a card with no suit and no value
            outbuf[count++] = game_e_create_move_small((move_code)TRICKTEAM_COMMUNICATION_NONE << 16);
            // note: since all this is state dependent this is a unique move here, even though it is 0, move none is 0xFF...FF
        } break;
        case TRICKTEAM_STATE_TRICK_PLAYING: {
            if (data.players[player - 1].hidden_info == false) {
                return ERR_MISSING_HIDDEN_STATE;
            }
            bool can_follow_suit = false;
            if (VEC_LEN(&data.current_trick.cards) > 0) {
                for (size_t i = 0; i < VEC_LEN(&data.players[player - 1].cards); i++) {
                    trickteam_card card = data.players[player - 1].cards[i];
                    if (card.suit == data.current_trick.lead_suit) {
                        can_follow_suit = true;
                        outbuf[count++] = game_e_create_move_small(
                            ((move_code)card.suit << 8) |
                            (move_code)card.value
                        );
                    }
                }
            }
            if (can_follow_suit == true) {
                break;
            }
            for (size_t i = 0; i < VEC_LEN(&data.players[player - 1].cards); i++) {
                trickteam_card card = data.players[player - 1].cards[i];
                outbuf[count++] = game_e_create_move_small(
                    ((move_code)card.suit << 8) |
                    (move_code)card.value
                );
            }
        } break;
        case TRICKTEAM_STATE_DONE:
        case TRICKTEAM_STATE_COUNT:
        case TRICKTEAM_STATE_SIZE_MAX: {
            assert(0);
        } break;
    }
    *ret_count = count;
    *ret_moves = bufs.concrete_moves;
    return ERR_OK;
}

static error_code get_concrete_move_probabilities_gf(game* self, uint32_t* ret_count, const float** ret_move_probabilities)
{
    opts_repr& opts = get_opts(self);
    state_repr& data = get_repr(self);
    export_buffers& bufs = get_bufs(self);
    float* outbuf = bufs.move_probabilities;
    uint32_t count = 0;
    switch (data.state) {
        case TRICKTEAM_STATE_NONE: {
            assert(0);
        } break;
        case TRICKTEAM_STATE_DEALING_CHALLENGES: {
            uint32_t total_weight = 0;
            // only consider challenges which are valid for the amount of challenge points left
            for (TRICKTEAM_CHALLENGE_TYPE chall_type = TRICKTEAM_CHALLENGE_TYPE_FIRST; chall_type < TRICKTEAM_CHALLENGE_TYPE_COUNT; chall_type = (TRICKTEAM_CHALLENGE_TYPE)((uint32_t)chall_type + 1)) {
                challenge_info chall_info = get_challenge_info(chall_type);
                if (data.challenge_points >= chall_info.challenge_rating[opts.player_count - 3]) {
                    total_weight += chall_info.weigth;
                }
            }
            // second round, actually deal out the challenge probabilities, now with total weight
            for (TRICKTEAM_CHALLENGE_TYPE chall_type = TRICKTEAM_CHALLENGE_TYPE_FIRST; chall_type < TRICKTEAM_CHALLENGE_TYPE_COUNT; chall_type = (TRICKTEAM_CHALLENGE_TYPE)((uint32_t)chall_type + 1)) {
                challenge_info chall_info = get_challenge_info(chall_type);
                if (data.challenge_points >= chall_info.challenge_rating[opts.player_count - 3]) {
                    outbuf[count++] = (float)chall_info.weigth / (float)total_weight;
                }
            }
        } break;
        case TRICKTEAM_STATE_DEALING_CARDS: {
            float card_chance = (float)1 / (float)VEC_LEN(&data.deck);
            for (size_t i = 0; i < VEC_LEN(&data.deck); i++) {
                outbuf[count++] = card_chance;
            }
        } break;
        case TRICKTEAM_STATE_CHALLENGE_SELECTING: {
            assert(0);
        } break;
        case TRICKTEAM_STATE_CHALLENGE_SPECIFYING: {
            count += challenge_specification_get_concrete_move_probabilities_igf(outbuf, VEC_LAST(&data.challenge_pool));
        } break;
        case TRICKTEAM_STATE_COMMUNICATING:
        case TRICKTEAM_STATE_TRICK_PLAYING:
        case TRICKTEAM_STATE_DONE:
        case TRICKTEAM_STATE_COUNT:
        case TRICKTEAM_STATE_SIZE_MAX: {
            assert(0);
        } break;
    }
    *ret_count = count;
    *ret_move_probabilities = bufs.move_probabilities;
    return ERR_OK;
}

static error_code get_random_move_gf(game* self, uint64_t seed, move_data_sync** ret_move)
{
    opts_repr& opts = get_opts(self);
    state_repr& data = get_repr(self);
    export_buffers& bufs = get_bufs(self);
    move_data_sync* outbuf = &bufs.move_out;
    switch (data.state) {
        case TRICKTEAM_STATE_NONE: {
            assert(0);
        } break;
        case TRICKTEAM_STATE_DEALING_CHALLENGES: {
            *outbuf = game_e_get_random_move_sync(self, seed);
        } break;
        case TRICKTEAM_STATE_DEALING_CARDS: {
            *outbuf = game_e_get_random_move_sync(self, seed);
        } break;
        case TRICKTEAM_STATE_CHALLENGE_SELECTING:
        case TRICKTEAM_STATE_CHALLENGE_SPECIFYING: {
            *outbuf = game_e_get_random_move_sync(self, seed);
        } break;
        case TRICKTEAM_STATE_COMMUNICATING:
        case TRICKTEAM_STATE_TRICK_PLAYING:
        case TRICKTEAM_STATE_DONE:
        case TRICKTEAM_STATE_COUNT:
        case TRICKTEAM_STATE_SIZE_MAX: {
            assert(0);
        } break;
    }
    *ret_move = outbuf;
    return ERR_OK;
}

static error_code get_actions_gf(game* self, player_id player, uint32_t* ret_count, const move_data** ret_moves)
{
    return ERR_FEATURE_UNSUPPORTED; //REMOVE
    opts_repr& opts = get_opts(self);
    state_repr& data = get_repr(self);
    export_buffers& bufs = get_bufs(self);
    move_data* outbuf = bufs.actions;
    uint32_t count = 0;
    switch (data.state) {
        case TRICKTEAM_STATE_NONE: {
            assert(0);
        } break;
        case TRICKTEAM_STATE_DEALING_CHALLENGES: {
            //TODO
        } break;
        case TRICKTEAM_STATE_DEALING_CARDS: {
            //TODO
        } break;
        case TRICKTEAM_STATE_CHALLENGE_SELECTING: {
            //TODO
        } break;
        case TRICKTEAM_STATE_CHALLENGE_SPECIFYING: {
            //TODO
        } break;
        case TRICKTEAM_STATE_COMMUNICATING: {
            //TODO
        } break;
        case TRICKTEAM_STATE_TRICK_PLAYING: {
            //TODO
        } break;
        case TRICKTEAM_STATE_DONE:
        case TRICKTEAM_STATE_COUNT:
        case TRICKTEAM_STATE_SIZE_MAX: {
            assert(0);
        } break;
    }
    *ret_count = count;
    *ret_moves = bufs.actions;
    return ERR_OK;
}

static error_code is_legal_move_gf(game* self, player_id player, move_data_sync move)
{
    opts_repr& opts = get_opts(self);
    state_repr& data = get_repr(self);
    switch (data.state) {
        case TRICKTEAM_STATE_NONE: {
            assert(0);
        } break;
        case TRICKTEAM_STATE_DEALING_CHALLENGES: {
            //TODO
        } break;
        case TRICKTEAM_STATE_DEALING_CARDS: {
            //TODO
        } break;
        case TRICKTEAM_STATE_CHALLENGE_SELECTING: {
            //TODO
        } break;
        case TRICKTEAM_STATE_CHALLENGE_SPECIFYING: {
            //TODO
        } break;
        case TRICKTEAM_STATE_COMMUNICATING: {
            //TODO
        } break;
        case TRICKTEAM_STATE_TRICK_PLAYING: {
            //TODO
        } break;
        case TRICKTEAM_STATE_DONE:
        case TRICKTEAM_STATE_COUNT:
        case TRICKTEAM_STATE_SIZE_MAX: {
            assert(0);
        } break;
    }
    return ERR_OK;
}

static error_code move_to_action_gf(game* self, player_id player, move_data_sync move, player_id target_player, move_data_sync** ret_action)
{
    opts_repr& opts = get_opts(self);
    state_repr& data = get_repr(self);
    export_buffers& bufs = get_bufs(self);
    move_data_sync* outbuf = &bufs.move_out;
    move_code mcode = move.md.cl.code;
    switch (data.state) {
        case TRICKTEAM_STATE_NONE: {
            assert(0);
        } break;
        case TRICKTEAM_STATE_DEALING_CHALLENGES: {
            *outbuf = game_e_move_sync_copy(move);
        } break;
        case TRICKTEAM_STATE_DEALING_CARDS: {
            trickteam_card move_card = (trickteam_card){
                .suit = (TRICKTEAM_SUIT)((mcode >> 8) & 0xFF),
                .value = (uint8_t)(mcode & 0xFF),
            };
            if (target_player == data.lead_player || card_eq_igf(move_card, (trickteam_card){.suit = TRICKTEAM_SUIT_BLACK, .value = 4}) == true) {
                *outbuf = game_e_move_sync_copy(move);
            } else {
                *outbuf = game_e_create_move_sync_small(self, ((move_code)TRICKTEAM_SUIT_NONE << 8) | (move_code)0);
            }
        } break;
        case TRICKTEAM_STATE_CHALLENGE_SELECTING: {
            *outbuf = game_e_move_sync_copy(move);
        } break;
        case TRICKTEAM_STATE_CHALLENGE_SPECIFYING: {
            *outbuf = game_e_move_sync_copy(move);
        } break;
        case TRICKTEAM_STATE_COMMUNICATING: {
            *outbuf = game_e_move_sync_copy(move);
        } break;
        case TRICKTEAM_STATE_TRICK_PLAYING: {
            *outbuf = game_e_move_sync_copy(move);
        } break;
        case TRICKTEAM_STATE_DONE:
        case TRICKTEAM_STATE_COUNT:
        case TRICKTEAM_STATE_SIZE_MAX: {
            assert(0);
        } break;
    }
    *ret_action = outbuf;
    return ERR_OK;
}

static error_code make_move_gf(game* self, player_id player, move_data_sync move)
{
    opts_repr& opts = get_opts(self);
    state_repr& data = get_repr(self);
    move_code mcode = move.md.cl.code;
    switch (data.state) {
        case TRICKTEAM_STATE_NONE: {
            assert(0);
        } break;
        case TRICKTEAM_STATE_DEALING_CHALLENGES: {
            // move is u32 chall_type
            uint32_t chall_type = mcode;
            // get challenge
            trickteam_challenge chall = (trickteam_challenge){
                .type = (TRICKTEAM_CHALLENGE_TYPE)chall_type,
                .state = TRICKTEAM_CHALLENGE_STATE_IDLE,
            };
            // add challenge to pool
            VEC_PUSH(&data.challenge_pool, chall);
            // decrease required challenge points left
            data.challenge_points -= get_challenge_info(chall.type).challenge_rating[opts.player_count - 3];
            // if challenge requires specification, change state
            if (challenge_needs_specification_igf(chall, false)) {
                data.state = TRICKTEAM_STATE_CHALLENGE_SPECIFYING;
                break;
            }
            if (data.challenge_points == 0) {
                // when finished with dealing challenges, set data.chall_sel_open_passes to the correct number
                data.chall_sel_open_passes = (VEC_LEN(&data.players) >= VEC_LEN(&data.challenge_pool) ? VEC_LEN(&data.players) - VEC_LEN(&data.challenge_pool) : 0);
                data.state = TRICKTEAM_STATE_DEALING_CARDS; // go to dealing cards once all challenges are dealt
            }
        } break;
        case TRICKTEAM_STATE_DEALING_CARDS: {
            trickteam_card move_card = (trickteam_card){
                .suit = (TRICKTEAM_SUIT)((mcode >> 8) & 0xFF),
                .value = (uint8_t)(mcode & 0xFF),
            };
            size_t dealing_to_player_idx = data.lead_player - 1;
            if (move_card.suit == TRICKTEAM_SUIT_NONE) {
                data.deck_hidden_info = false;
                data.players[dealing_to_player_idx].hidden_info = false;
            }
            // if deck info is known remove card
            if (data.deck_hidden_info == true) {
                for (size_t card_idx = 0; card_idx < VEC_LEN(&data.deck); card_idx++) {
                    if (card_eq_igf(data.deck[card_idx], move_card) == true) {
                        VEC_REMOVE_SWAP(&data.deck, card_idx);
                        break;
                    }
                }
            } else {
                VEC_POP(&data.deck);
            }
            // add to deck of player
            VEC_PUSH(&data.players[dealing_to_player_idx].cards, move_card);
            // if this is the (BLACK,4) note the starting player for later in the first, "current" trick, and set player as captain
            if (card_eq_igf(move_card, (trickteam_card){.suit = TRICKTEAM_SUIT_BLACK, .value = 4}) == true) {
                data.players[dealing_to_player_idx].captain = true;
                data.current_trick.lead_player = dealing_to_player_idx + 1;
            }
            // go forward for lead player
            data.lead_player = (data.lead_player % opts.player_count) + 1;
            // if this was the last card in the deck proceed to selecting challenges
            if (VEC_LEN(&data.deck) == 0) {
                trick_create_igf(&data.current_trick, data.current_trick_id, data.current_trick.lead_player);
                data.lead_player = data.current_trick.lead_player;
                data.state = TRICKTEAM_STATE_CHALLENGE_SELECTING;
            }
        } break;
        case TRICKTEAM_STATE_CHALLENGE_SELECTING: {
            size_t chall_num = mcode;
            // is a valid chall_num, 0 is skip
            if (chall_num > 0) {
                // move is idx of challenge player wants
                trickteam_challenge chall = data.challenge_pool[chall_num - 1];
                VEC_REMOVE(&data.challenge_pool, chall_num - 1);
                // move challenge to the player
                VEC_PUSH(&data.players[player - 1].challenges, chall);
                // if the challenge requires specification switch state
                if (challenge_needs_specification_igf(chall, true)) {
                    data.state = TRICKTEAM_STATE_CHALLENGE_SPECIFYING;
                    break;
                }
            } else {
                // player is passing on picking a challenge
                if (data.chall_sel_open_passes > 0) {
                    data.chall_sel_open_passes--;
                }
            }
            // go to selecting for the next player
            data.lead_player = (data.lead_player % opts.player_count) + 1;
            // if no more challenges left, play for captain
            if (VEC_LEN(&data.challenge_pool) == 0) {
                data.state = TRICKTEAM_STATE_COMMUNICATING;
                data.lead_player = data.current_trick.lead_player;
            }
        } break;
        case TRICKTEAM_STATE_CHALLENGE_SPECIFYING: {
            // move is challenge specific specification, specify the challenge
            challenge_specification_make_move_igf(move.md, player == PLAYER_RAND ? &VEC_LAST(&data.challenge_pool) : &VEC_LAST(&data.players[player - 1].challenges));
            // challenge might need more specification
            if (challenge_needs_specification_igf(player == PLAYER_RAND ? VEC_LAST(&data.challenge_pool) : VEC_LAST(&data.players[player - 1].challenges), player != PLAYER_RAND)) {
                break;
            }
            // are we still dealing out challenges to the pool / selecting challenges from the pool?
            bool still_dealing_or_selecting = (data.lead_player == PLAYER_RAND ? data.challenge_points > 0 : VEC_LEN(&data.challenge_pool) > 0);
            if (still_dealing_or_selecting == true && data.lead_player == PLAYER_RAND) {
                // player rand continues to deal challenges
                data.state = TRICKTEAM_STATE_DEALING_CHALLENGES;
            } else if (still_dealing_or_selecting == true) {
                // next player continues with challenge selecting
                data.state = TRICKTEAM_STATE_CHALLENGE_SELECTING;
                data.lead_player = (data.lead_player % opts.player_count) + 1;
            } else {
                // if not dealing / selecting anymore, captain is lead player to select / play next
                data.state = (data.lead_player == PLAYER_RAND ? TRICKTEAM_STATE_DEALING_CARDS : TRICKTEAM_STATE_COMMUNICATING);
                data.lead_player = (data.lead_player == PLAYER_RAND ? 1 : data.current_trick.lead_player);
            }
        } break;
        case TRICKTEAM_STATE_COMMUNICATING: {
            // move is card name and includes info on the communication type
            trickteam_communication com = (trickteam_communication){
                .type = (TRICKTEAM_COMMUNICATION)((mcode >> 16) & 0xFF),
                .card = (trickteam_card){
                    .suit = (TRICKTEAM_SUIT)((mcode >> 8) & 0xFF),
                    .value = (uint8_t)(mcode & 0xFF),
                },
            };
            // add to communications if it is one
            if (com.type != TRICKTEAM_COMMUNICATION_NONE) {
                VEC_PUSH(&data.players[player - 1].communications, com);
                data.open_communications -= 1;
            }
            // pass player if communication pass or player can only communicate once
            if (com.type == TRICKTEAM_COMMUNICATION_NONE || opts.communication_limit == 0) {
                data.players[player - 1].communication_pass = true;
            }
            // determine if this is the end of the communication phase
            bool all_players_passed = true;
            for (size_t i = 0; i < VEC_LEN(&data.players); i++) {
                if (data.players[i].communication_pass == false) {
                    all_players_passed &= false;
                }
            }
            // this was the last open communication (no matter if limit or not) or all players passed
            if (data.open_communications == 0 || all_players_passed == true) {
                for (size_t i = 0; i < VEC_LEN(&data.players); i++) {
                    data.players[i].communication_pass = false;
                }
                data.open_communications = (opts.communication_limit > 0 ? opts.communication_limit : opts.player_count);
                data.state = TRICKTEAM_STATE_TRICK_PLAYING;
            }
        } break;
        case TRICKTEAM_STATE_TRICK_PLAYING: {
            // move is card with suit and value
            trickteam_card move_card = (trickteam_card){
                .suit = (TRICKTEAM_SUIT)((mcode >> 8) & 0xFF),
                .value = (uint8_t)(mcode & 0xFF),
            };
            // if this is the first card in the trick set lead suit
            if (VEC_LEN(&data.current_trick.cards) == 0) {
                data.current_trick.lead_suit = move_card.suit;
            }
            // add the card from move to trick
            VEC_PUSH(&data.current_trick.cards, move_card);
            // if hidden info is known for player remove the card
            if (data.players[player - 1].hidden_info == true) {
                for (size_t card_idx = 0; card_idx < VEC_LEN(&data.players[player - 1].cards); card_idx++) {
                    trickteam_card test_card = data.players[player - 1].cards[card_idx];
                    if (card_eq_igf(test_card, move_card) == true) {
                        VEC_REMOVE_SWAP(&data.players[player - 1].cards, card_idx);
                        break;
                    }
                }
            } else {
                // otherwise just remove 1 unknown card
                VEC_POP(&data.players[player - 1].cards);
            }
            // first card in the trick is always winning
            // played card is winning if stronger than currently winning card
            // or is winning if winningcard is not black but new card is
            bool card_is_winning = VEC_LEN(&data.current_trick.cards) == 0 ||
                                   (move_card.suit == data.current_trick.winning_card.suit && move_card.value > data.current_trick.winning_card.value) ||
                                   (data.current_trick.winning_card.suit != TRICKTEAM_SUIT_BLACK && move_card.suit == TRICKTEAM_SUIT_BLACK);
            if (card_is_winning == true) {
                data.current_trick.winning_card = move_card;
                data.current_trick.winning_player = player;
            }
            // go forward for lead player
            data.lead_player = (data.lead_player % opts.player_count) + 1;
            // if trick is done
            if (VEC_LEN(&data.current_trick.cards) == opts.player_count) {
                // adjust ptm
                data.lead_player = data.current_trick.winning_player;
                // for every player for each of their challenges run check challenge and record if win
                bool challenge_win = true;
                bool challenge_fail = false;
                for (size_t player_idx = 0; player_idx < VEC_LEN(&data.players); player_idx++) {
                    for (size_t challenge_idx = 0; challenge_idx < VEC_LEN(&data.players[player_idx].challenges); challenge_idx++) {
                        TRICKTEAM_CHALLENGE_STATE challenge_state = check_challenge_igf(self, &data.players[player_idx], &data.players[player_idx].challenges[challenge_idx], data.current_trick); // run challenges for every player for this trick //TODO want to do this after every card that is played?
                        if (challenge_state == TRICKTEAM_CHALLENGE_STATE_FAIL) {
                            challenge_fail = true;
                        }
                        if (challenge_state != TRICKTEAM_CHALLENGE_STATE_PASS) {
                            challenge_win = false;
                        }
                    }
                }
                {
                    trick_destroy_igf(&data.previous_trick); //TODO don't destroy if is moved instead, then this is just a copy
                }
                // replace the previous trick with the new one
                data.previous_trick = data.current_trick;
                {
                    // move it to the winners tricks //TODO want this?
                    // VEC_PUSH(data.players[data.current_trick.winning_player-1].tricks, data.current_trick);
                }
                // increment trick order id
                data.current_trick_id++;
                // create new trick and delete all communications and move state to communicating
                trick_create_igf(&data.current_trick, data.current_trick_id, data.lead_player);
                for (size_t player_idx = 0; player_idx < VEC_LEN(&data.players); player_idx++) {
                    VEC_DESTROY(&data.players[player_idx].communications);
                }
                data.state = TRICKTEAM_STATE_COMMUNICATING;
                // if this was the last trick or early win, trigger end game
                if (data.current_trick_id == data.max_tricks || challenge_win == true || challenge_fail == true) {
                    data.win = challenge_win;
                    data.state = TRICKTEAM_STATE_DONE;
                }
            }
        } break;
        case TRICKTEAM_STATE_DONE:
        case TRICKTEAM_STATE_COUNT:
        case TRICKTEAM_STATE_SIZE_MAX: {
            assert(0);
        } break;
    }
    return ERR_OK;
}

static error_code get_results_gf(game* self, uint8_t* ret_count, const player_id** ret_players)
{
    opts_repr& opts = get_opts(self);
    state_repr& data = get_repr(self);
    export_buffers& bufs = get_bufs(self);
    player_id* outbuf = bufs.results;
    if (data.win == false) {
        *ret_count = 0;
        return ERR_OK;
    }
    *ret_count = opts.player_count;
    for (uint8_t i = 0; i < opts.player_count; i++) {
        outbuf[i] = i + 1;
    }
    *ret_players = outbuf;
    return ERR_OK;
}

static error_code redact_keep_state_gf(game* self, uint8_t count, const player_id* players)
{
    //TODO
    return ERR_OK;
}

static error_code get_move_data_gf(game* self, player_id player, const char* str, move_data_sync** ret_move)
{
    opts_repr& opts = get_opts(self);
    state_repr& data = get_repr(self);
    export_buffers& bufs = get_bufs(self);
    move_data_sync* outbuf = &bufs.move_out;
    switch (data.state) {
        case TRICKTEAM_STATE_NONE: {
            assert(0);
        } break;
        case TRICKTEAM_STATE_DEALING_CHALLENGES: {
            bool found_challenge = false;
            // match str with str_id in challenge_info
            for (TRICKTEAM_CHALLENGE_TYPE chall_type = TRICKTEAM_CHALLENGE_TYPE_FIRST; chall_type < TRICKTEAM_CHALLENGE_TYPE_COUNT; chall_type = (TRICKTEAM_CHALLENGE_TYPE)((uint32_t)chall_type + 1)) {
                if (strcmp(get_challenge_info(chall_type).str_id, str) == 0) {
                    *outbuf = game_e_create_move_sync_small(self, (uint32_t)chall_type);
                    found_challenge = true;
                    break;
                }
            }
            if (found_challenge == false) {
                return ERR_INVALID_INPUT; // if no match found this is not a valid challenge
            }
        } break;
        case TRICKTEAM_STATE_DEALING_CARDS: {
            if (strlen(str) != 2) {
                return ERR_INVALID_INPUT;
            }
            char suit;
            char value;
            int ec = sscanf(str, "%c%c", &suit, &value);
            if (ec != 2) {
                return ERR_INVALID_INPUT;
            }
            trickteam_card card = (trickteam_card){
                .suit = char_2_suit(suit),
                .value = char_2_val(value),
            };
            if ((card.suit == TRICKTEAM_SUIT_NONE && suit != suit_2_char[TRICKTEAM_SUIT_NONE]) || (card.value == 0 && value != val_2_char[0])) {
                return ERR_INVALID_INPUT;
            }
            *outbuf = game_e_create_move_sync_small(self, ((move_code)card.suit << 8) | (move_code)card.value);
        } break;
        case TRICKTEAM_STATE_CHALLENGE_SELECTING: {
            size_t chall_num;
            int ec = sscanf(str, "%zu", &chall_num);
            if (ec != 1) {
                return ERR_INVALID_INPUT;
            }
            *outbuf = game_e_create_move_sync_small(self, (move_code)chall_num);
        } break;
        case TRICKTEAM_STATE_CHALLENGE_SPECIFYING: {
            move_data speci_move;
            error_code ec = challenge_specification_get_move_data_igf(str, &speci_move, player == PLAYER_RAND ? VEC_LAST(&data.challenge_pool) : VEC_LAST(&data.players[player - 1].challenges));
            if (ec != ERR_OK) {
                return ERR_INVALID_INPUT;
            }
            *outbuf = game_e_create_move_sync_small(self, speci_move.cl.code);
        } break;
        case TRICKTEAM_STATE_COMMUNICATING: {
            if (strcmp(str, "pass") == 0) {
                *outbuf = game_e_create_move_sync_small(self, ((move_code)TRICKTEAM_COMMUNICATION_NONE << 16) | ((move_code)TRICKTEAM_SUIT_NONE << 8) | (move_code)0);
                break;
            }
            if (strlen(str) != 3) {
                return ERR_INVALID_INPUT;
            }
            char comm;
            char suit;
            char value;
            int ec = sscanf(str, "%c%c%c", &comm, &suit, &value);
            if (ec != 3) {
                return ERR_INVALID_INPUT;
            }
            trickteam_communication comm_v = (trickteam_communication){
                .type = char_2_comm(comm),
                .card = (trickteam_card){
                    .suit = char_2_suit(suit),
                    .value = char_2_val(value),
                },
            };
            if ((comm_v.type == TRICKTEAM_COMMUNICATION_NONE && comm != comm_2_char[TRICKTEAM_COMMUNICATION_NONE]) || comm_v.card.suit == TRICKTEAM_SUIT_NONE || comm_v.card.value == 0) {
                return ERR_INVALID_INPUT;
            }
            *outbuf = game_e_create_move_sync_small(self, ((move_code)comm_v.type << 16) | ((move_code)comm_v.card.suit << 8) | (move_code)comm_v.card.value);
        } break;
        case TRICKTEAM_STATE_TRICK_PLAYING: {
            if (strlen(str) != 2) {
                return ERR_INVALID_INPUT;
            }
            char suit;
            char value;
            int ec = sscanf(str, "%c%c", &suit, &value);
            if (ec != 2) {
                return ERR_INVALID_INPUT;
            }
            trickteam_card card = (trickteam_card){
                .suit = char_2_suit(suit),
                .value = char_2_val(value),
            };
            if (card.suit == TRICKTEAM_SUIT_NONE || card.value == 0) {
                return ERR_INVALID_INPUT;
            }
            *outbuf = game_e_create_move_sync_small(self, ((move_code)card.suit << 8) | (move_code)card.value);
        } break;
        case TRICKTEAM_STATE_DONE:
        case TRICKTEAM_STATE_COUNT:
        case TRICKTEAM_STATE_SIZE_MAX: {
            assert(0);
        } break;
    }
    *ret_move = outbuf;
    return ERR_OK;
}

static error_code get_move_str_gf(game* self, player_id player, move_data_sync move, size_t* ret_size, const char** ret_str)
{
    opts_repr& opts = get_opts(self);
    state_repr& data = get_repr(self);
    export_buffers& bufs = get_bufs(self);
    char* outbuf = bufs.move_str;
    move_code mcode = move.md.cl.code;
    switch (data.state) {
        case TRICKTEAM_STATE_NONE: {
            assert(0);
        } break;
        case TRICKTEAM_STATE_DEALING_CHALLENGES: {
            // move is chall type, just print the id for dealing
            TRICKTEAM_CHALLENGE_TYPE chall_type = (TRICKTEAM_CHALLENGE_TYPE)mcode;
            outbuf += sprintf(outbuf, "%s", get_challenge_info(chall_type).str_id);
        } break;
        case TRICKTEAM_STATE_DEALING_CARDS: {
            trickteam_card move_card = (trickteam_card){
                .suit = (TRICKTEAM_SUIT)((mcode >> 8) & 0xFF),
                .value = (uint8_t)(mcode & 0xFF),
            };
            outbuf += sprintf(outbuf, "%c%c", suit_2_char[move_card.suit], val_2_char[move_card.value]);
        } break;
        case TRICKTEAM_STATE_CHALLENGE_SELECTING: {
            size_t chall_num = mcode;
            if (chall_num > 0) {
                outbuf += sprintf(outbuf, "%zu", chall_num);
            } else {
                outbuf += sprintf(outbuf, "pass");
            }
        } break;
        case TRICKTEAM_STATE_CHALLENGE_SPECIFYING: {
            outbuf += challenge_specification_get_move_str_igf(outbuf, move.md, player == PLAYER_RAND ? VEC_LAST(&data.challenge_pool) : VEC_LAST(&data.players[player - 1].challenges));
        } break;
        case TRICKTEAM_STATE_COMMUNICATING: {
            trickteam_communication com = (trickteam_communication){
                .type = (TRICKTEAM_COMMUNICATION)((mcode >> 16) & 0xFF),
                .card = (trickteam_card){
                    .suit = (TRICKTEAM_SUIT)((mcode >> 8) & 0xFF),
                    .value = (uint8_t)(mcode & 0xFF),
                },
            };
            if (com.type == TRICKTEAM_COMMUNICATION_NONE && com.card.suit == TRICKTEAM_SUIT_NONE && com.card.value == 0) {
                outbuf += sprintf(outbuf, "pass");
            } else {
                outbuf += sprintf(outbuf, "%c%c%c", comm_2_char[com.type], suit_2_char[com.card.suit], val_2_char[com.card.value]);
            }
        } break;
        case TRICKTEAM_STATE_TRICK_PLAYING: {
            trickteam_card move_card = (trickteam_card){
                .suit = (TRICKTEAM_SUIT)((mcode >> 8) & 0xFF),
                .value = (uint8_t)(mcode & 0xFF),
            };
            outbuf += sprintf(outbuf, "%c%c", suit_2_char[move_card.suit], val_2_char[move_card.value]);
        } break;
        case TRICKTEAM_STATE_DONE:
        case TRICKTEAM_STATE_COUNT:
        case TRICKTEAM_STATE_SIZE_MAX: {
            assert(0);
        } break;
    }
    *ret_size = outbuf - bufs.move_str;
    *ret_str = bufs.move_str;
    return ERR_OK;
}

static error_code print_gf(game* self, player_id player, size_t* ret_size, const char** ret_str)
{
    opts_repr& opts = get_opts(self);
    state_repr& data = get_repr(self);
    export_buffers& bufs = get_bufs(self);
    char* outbuf = bufs.print;
    {
        // state display
        outbuf += sprintf(outbuf, "state: ");
        switch (data.state) {
            case TRICKTEAM_STATE_NONE: {
                assert(0);
            } break;
            case TRICKTEAM_STATE_DEALING_CHALLENGES: {
                outbuf += sprintf(outbuf, "dealing challenges (%hhu points left)", data.challenge_points);
            } break;
            case TRICKTEAM_STATE_DEALING_CARDS: {
                outbuf += sprintf(outbuf, "dealing cards (to P%hhu)", data.lead_player);
            } break;
            case TRICKTEAM_STATE_CHALLENGE_SELECTING: {
                outbuf += sprintf(outbuf, "challenge selecting (to P%hhu) (%hhu skips left)", data.lead_player, data.chall_sel_open_passes);
            } break;
            case TRICKTEAM_STATE_CHALLENGE_SPECIFYING: {
                outbuf += sprintf(outbuf, "challenge specifying (as P%hhu)", data.lead_player);
            } break;
            case TRICKTEAM_STATE_COMMUNICATING: {
                outbuf += sprintf(outbuf, "communicating (P%hhu leads)", data.current_trick.lead_player);
                if (opts.communication_limit > 0) {
                    outbuf += sprintf(outbuf, " (%hhu open)", data.open_communications);
                }
            } break;
            case TRICKTEAM_STATE_TRICK_PLAYING: {
                outbuf += sprintf(outbuf, "trick playing (P%hhu)", data.lead_player);
            } break;
            case TRICKTEAM_STATE_DONE: {
                outbuf += sprintf(outbuf, "done (%s)", data.win == true ? "win" : "fail");
            } break;
            case TRICKTEAM_STATE_COUNT:
            case TRICKTEAM_STATE_SIZE_MAX: {
                assert(0);
            } break;
        }
        outbuf += sprintf(outbuf, "\n");

        // challenges to be selected
        if (VEC_LEN(&data.challenge_pool) > 0) {
            outbuf += sprintf(outbuf, "challenge pool (%zu):", VEC_LEN(&data.challenge_pool));
            for (size_t challenge_idx = 0; challenge_idx < VEC_LEN(&data.challenge_pool); challenge_idx++) {
                trickteam_challenge chal = data.challenge_pool[challenge_idx];
                outbuf += sprintf(outbuf, " %zu<", challenge_idx + 1);
                outbuf += sprintf_challenge_igf(outbuf, chal);
                outbuf += sprintf(outbuf, ">");
            }
            outbuf += sprintf(outbuf, "\n");
        }

        // deck to be shuffled
        if (data.state == TRICKTEAM_STATE_DEALING_CARDS) {
            outbuf += sprintf(outbuf, "deck (%zu):", VEC_LEN(&data.deck));
            if (data.deck_hidden_info == true) {
                for (size_t card_idx = 0; card_idx < VEC_LEN(&data.deck); card_idx++) {
                    trickteam_card card = data.deck[card_idx];
                    outbuf += sprintf(outbuf, " %c%c", suit_2_char[card.suit], val_2_char[card.value]);
                }
            }
            outbuf += sprintf(outbuf, "\n");
        }

        // only show trick info if shuffling is done
        if (VEC_LEN(&data.deck) == 0) {
            // info about last trick
            if (data.current_trick.order_id > 0) {
                outbuf += sprintf(outbuf, "prev trick: P%hhu >", data.previous_trick.lead_player);
                for (size_t card_idx = 0; card_idx < VEC_LEN(&data.previous_trick.cards); card_idx++) {
                    trickteam_card card = data.previous_trick.cards[card_idx];
                    outbuf += sprintf(outbuf, " %c%c", suit_2_char[card.suit], val_2_char[card.value]);
                }
                outbuf += sprintf(outbuf, " > P%hhu\n", data.previous_trick.winning_player);
            }
            // info about current trick
            if (data.current_trick.order_id < data.max_tricks) {
                outbuf += sprintf(outbuf, "curr trick (%hhu/%hhu): P%hhu >", (uint8_t)(data.current_trick_id + 1), data.max_tricks, data.current_trick.lead_player); //HACK the first cast somehow required for clang
                for (size_t card_idx = 0; card_idx < VEC_LEN(&data.current_trick.cards); card_idx++) {
                    trickteam_card card = data.current_trick.cards[card_idx];
                    outbuf += sprintf(outbuf, " %c%c", suit_2_char[card.suit], val_2_char[card.value]);
                }
                outbuf += sprintf(outbuf, "\n");
            }
        }

        //TODO

        // list players and things we know about them
        for (size_t player_idx = 0; player_idx < VEC_LEN(&data.players); player_idx++) {
            outbuf += sprintf(
                outbuf,
                "P%hhu %c:\n",
                data.players[player_idx].id,
                data.players[player_idx].captain == true ? 'C' : '-'
            );
            // challenges
            outbuf += sprintf(outbuf, "    chals (%zu):", VEC_LEN(&data.players[player_idx].challenges));
            for (size_t challenge_idx = 0; challenge_idx < VEC_LEN(&data.players[player_idx].challenges); challenge_idx++) {
                trickteam_challenge chal = data.players[player_idx].challenges[challenge_idx];
                outbuf += sprintf(outbuf, " <");
                outbuf += sprintf_challenge_igf(outbuf, chal);
                outbuf += sprintf(outbuf, ">");
            }
            outbuf += sprintf(outbuf, "\n");
            // cards
            outbuf += sprintf(outbuf, "    cards (%zu):", VEC_LEN(&data.players[player_idx].cards));
            for (size_t card_idx = 0; card_idx < VEC_LEN(&data.players[player_idx].cards); card_idx++) {
                trickteam_card card = data.players[player_idx].cards[card_idx];
                outbuf += sprintf(outbuf, " %c%c", suit_2_char[card.suit], val_2_char[card.value]);
            }
            outbuf += sprintf(outbuf, "\n");
            // cards, sorted
            uint64_t card_bitfield = get_player_sorted_cards_bitfield_igf(self, data.players[player_idx].id);
            outbuf += sprintf(outbuf, "      sorted:");
            for (size_t offset = 0; offset < 40; offset++) {
                if (((card_bitfield >> offset) & 0b1) == false) {
                    continue;
                }
                trickteam_card card;
                if (offset < 4) {
                    card.suit = TRICKTEAM_SUIT_BLACK;
                    card.value = 4 - offset;
                } else {
                    card.suit = (TRICKTEAM_SUIT)((uint64_t)TRICKTEAM_SUIT_BLACK + 1 + ((offset - 4) / 9));
                    card.value = 9 - ((offset - 4) % 9);
                }
                outbuf += sprintf(outbuf, " %c%c", suit_2_char[card.suit], val_2_char[card.value]);
            }
            outbuf += sprintf(outbuf, "\n");
            // communications
            outbuf += sprintf(outbuf, "    comms (%zu):", VEC_LEN(&data.players[player_idx].communications));
            for (size_t comm_idx = 0; comm_idx < VEC_LEN(&data.players[player_idx].communications); comm_idx++) {
                trickteam_communication comm = data.players[player_idx].communications[comm_idx];
                outbuf += sprintf(outbuf, " %c%c%c", comm_2_char[comm.type], suit_2_char[comm.card.suit], val_2_char[comm.card.value]);
            }
            outbuf += sprintf(outbuf, "\n");
        }
    }
    *ret_size = outbuf - bufs.print;
    *ret_str = bufs.print;
    return ERR_OK;
}

//=====
// game internal methods

static bool card_eq_igf(trickteam_card left, trickteam_card right)
{
    return left.suit == right.suit && left.value == right.value;
}

static void player_create_igf(trickteam_player* player, player_id id)
{
    *player = (trickteam_player){
        .id = id,
        .captain = false,
        .challenges = NULL,
        .cards = NULL,
        .communications = NULL,
        .communication_pass = false,
        .hidden_info = true,
    };
    VEC_CREATE(&player->cards, 14);
}

static void player_destroy_igf(trickteam_player* player)
{
    VEC_DESTROY(&player->challenges);
    VEC_DESTROY(&player->cards);
    VEC_DESTROY(&player->communications);
}

static void trick_create_igf(trickteam_trick* trick, uint8_t order_id, player_id lead_player)
{
    *trick = (trickteam_trick){
        .order_id = order_id,
        .lead_player = lead_player,
        .lead_suit = TRICKTEAM_SUIT_NONE,
        .cards = NULL,
        .winning_player = PLAYER_NONE,
        .winning_card = (trickteam_card){
            .suit = TRICKTEAM_SUIT_NONE,
            .value = 0,
        },
    };
    VEC_CREATE(&trick->cards, 5);
}

static void trick_destroy_igf(trickteam_trick* trick)
{
    VEC_DESTROY(&trick->cards);
}

static TRICKTEAM_CHALLENGE_STATE check_challenge_igf(game* self, trickteam_player* player, trickteam_challenge* challenge, trickteam_trick done_trick)
{
    opts_repr& opts = get_opts(self);
    state_repr& data = get_repr(self);
    switch (challenge->type) {
        case TRICKTEAM_CHALLENGE_TYPE_NONE: {
            // pass
        } break;
        case TRICKTEAM_CHALLENGE_TYPE_GET_SPECIFIC_1: {
            bool card_in_trick = false;
            for (uint8_t i = 0; i < VEC_LEN(&done_trick.cards); i++) {
                if (card_eq_igf(done_trick.cards[i], challenge->du.get_specific_1.target)) {
                    break;
                }
            }
            if (card_in_trick == true) {
                challenge->state = (done_trick.winning_player == player->id ? TRICKTEAM_CHALLENGE_STATE_PASS : TRICKTEAM_CHALLENGE_STATE_FAIL);
            }
        } break;
        //TODO more types
        case TRICKTEAM_CHALLENGE_TYPE_COUNT:
        case TRICKTEAM_CHALLENGE_TYPE_SIZE_MAX:
        default: {
            assert(0);
        } break;
    }
    return challenge->state;
}

static bool challenge_needs_specification_igf(trickteam_challenge challenge, bool selecting_not_dealing)
{
    switch (challenge.type) {
        case TRICKTEAM_CHALLENGE_TYPE_NONE: {
            return false;
        } break;
        case TRICKTEAM_CHALLENGE_TYPE_GET_SPECIFIC_1: {
            trickteam_card card = challenge.du.get_specific_1.target;
            return (card.suit == TRICKTEAM_SUIT_NONE || card.value == 0);
        } break;
        //TODO more types
        case TRICKTEAM_CHALLENGE_TYPE_COUNT:
        case TRICKTEAM_CHALLENGE_TYPE_SIZE_MAX:
        default: {
            assert(0);
        } break;
    }
    return false;
}

static uint32_t challenge_specification_get_concrete_moves_igf(move_data* moves, trickteam_challenge challenge)
{
    //TODO this needs access to all existing challenges, same for is_legal, so the generated cards are not contradicting!
    //TODO or just use the replace mode for now, or just regenerate the whole game
    uint32_t count = 0;
    switch (challenge.type) {
        case TRICKTEAM_CHALLENGE_TYPE_NONE: {
            // pass
        } break;
        case TRICKTEAM_CHALLENGE_TYPE_GET_SPECIFIC_1: {
            //TODO adapt for non conflicting challenges
            for (TRICKTEAM_SUIT suit = TRICKTEAM_SUIT_FIRST_NON_BLACK; suit < TRICKTEAM_SUIT_COUNT; suit = (TRICKTEAM_SUIT)((int)suit + 1)) {
                for (uint8_t value = 1; value <= 9; value++) {
                    moves[count++] = game_e_create_move_small(((move_code)suit << 8) | (move_code)value);
                }
            }
        } break;
        //TODO more types
        case TRICKTEAM_CHALLENGE_TYPE_COUNT:
        case TRICKTEAM_CHALLENGE_TYPE_SIZE_MAX:
        default: {
            assert(0);
        } break;
    }
    return count;
}

static uint32_t challenge_specification_get_concrete_move_probabilities_igf(float* probabilities, trickteam_challenge challenge)
{
    uint32_t count = 0;
    switch (challenge.type) {
        case TRICKTEAM_CHALLENGE_TYPE_NONE: {
            // pass
        } break;
        case TRICKTEAM_CHALLENGE_TYPE_GET_SPECIFIC_1: {
            //TODO adapt for non conflicting challenges
            for (uint32_t i = 0; i < 36; i++) {
                probabilities[count++] = (float)1 / (float)36;
            }
        } break;
        //TODO more types
        case TRICKTEAM_CHALLENGE_TYPE_COUNT:
        case TRICKTEAM_CHALLENGE_TYPE_SIZE_MAX:
        default: {
            assert(0);
        } break;
    }
    return count;
}

static bool challenge_specification_is_legal_igf(move_data move, trickteam_challenge challenge)
{
    //TODO
    switch (challenge.type) {
        case TRICKTEAM_CHALLENGE_TYPE_NONE: {
            // pass
        } break;
        case TRICKTEAM_CHALLENGE_TYPE_GET_SPECIFIC_1: {
            //TODO
        } break;
        //TODO more types
        case TRICKTEAM_CHALLENGE_TYPE_COUNT:
        case TRICKTEAM_CHALLENGE_TYPE_SIZE_MAX:
        default: {
            assert(0);
        } break;
    }
    return false;
}

static void challenge_specification_make_move_igf(move_data move, trickteam_challenge* challenge)
{
    move_code mcode = move.cl.code;
    switch (challenge->type) {
        case TRICKTEAM_CHALLENGE_TYPE_NONE: {
            // pass
        } break;
        case TRICKTEAM_CHALLENGE_TYPE_GET_SPECIFIC_1: {
            trickteam_card move_card = (trickteam_card){
                .suit = (TRICKTEAM_SUIT)((mcode >> 8) & 0xFF),
                .value = (uint8_t)(mcode & 0xFF),
            };
            challenge->du.get_specific_1.target = move_card;
        } break;
        //TODO more types
        case TRICKTEAM_CHALLENGE_TYPE_COUNT:
        case TRICKTEAM_CHALLENGE_TYPE_SIZE_MAX:
        default: {
            assert(0);
        } break;
    }
}

static error_code challenge_specification_get_move_data_igf(const char* str, move_data* move, trickteam_challenge challenge)
{
    switch (challenge.type) {
        case TRICKTEAM_CHALLENGE_TYPE_NONE: {
            assert(0);
            return ERR_INVALID_INPUT;
        } break;
        case TRICKTEAM_CHALLENGE_TYPE_GET_SPECIFIC_1: {
            //TODO use utility to parse card
            if (strlen(str) != 2) {
                return ERR_INVALID_INPUT;
            }
            char suit;
            char value;
            int ec = sscanf(str, "%c%c", &suit, &value);
            if (ec != 2) {
                return ERR_INVALID_INPUT;
            }
            trickteam_card card = (trickteam_card){
                .suit = char_2_suit(suit),
                .value = char_2_val(value),
            };
            if (card.suit == TRICKTEAM_SUIT_NONE || card.value == 0) {
                return ERR_INVALID_INPUT;
            }
            *move = game_e_create_move_small(((move_code)card.suit << 8) | (move_code)card.value);
        } break;
        //TODO more types
        case TRICKTEAM_CHALLENGE_TYPE_COUNT:
        case TRICKTEAM_CHALLENGE_TYPE_SIZE_MAX:
        default: {
            assert(0);
            return ERR_INVALID_INPUT;
        } break;
    }
    return ERR_OK;
}

static size_t challenge_specification_get_move_str_igf(char* str, move_data move, trickteam_challenge challenge)
{
    char* wstr = str;
    move_code mcode = move.cl.code;
    switch (challenge.type) {
        case TRICKTEAM_CHALLENGE_TYPE_NONE: {
            // pass
        } break;
        case TRICKTEAM_CHALLENGE_TYPE_GET_SPECIFIC_1: {
            //TODO use utility to print card
            trickteam_card move_card = (trickteam_card){
                .suit = (TRICKTEAM_SUIT)((mcode >> 8) & 0xFF),
                .value = (uint8_t)(mcode & 0xFF),
            };
            wstr += sprintf(wstr, "%c%c", suit_2_char[move_card.suit], val_2_char[move_card.value]);
        } break;
        //TODO more types
        case TRICKTEAM_CHALLENGE_TYPE_COUNT:
        case TRICKTEAM_CHALLENGE_TYPE_SIZE_MAX:
        default: {
            assert(0);
        } break;
    }
    return wstr - str;
}

static trickteam_challenge sscanf_challenge_igf(const char* str, const char* str_end)
{
    trickteam_challenge challenge = (trickteam_challenge){.type = TRICKTEAM_CHALLENGE_TYPE_NONE};
    //TODO scan the type of challenge, the first few chars of the str
    switch (challenge.type) {
        case TRICKTEAM_CHALLENGE_TYPE_NONE: {
            // pass
        } break;
        case TRICKTEAM_CHALLENGE_TYPE_GET_SPECIFIC_1: {
            //TODO
        } break;
        //TODO more types
        case TRICKTEAM_CHALLENGE_TYPE_COUNT:
        case TRICKTEAM_CHALLENGE_TYPE_SIZE_MAX:
        default: {
            assert(0);
        } break;
    }
    return challenge;
}

// automatically prints specification if available, even if partial
static size_t sprintf_challenge_igf(char* str, trickteam_challenge challenge)
{
    char* wstr = str;
    switch (challenge.type) {
        case TRICKTEAM_CHALLENGE_TYPE_NONE: {
            // pass
        } break;
        case TRICKTEAM_CHALLENGE_TYPE_GET_SPECIFIC_1: {
            trickteam_card card = challenge.du.get_specific_1.target;
            wstr += sprintf(wstr, "%s", get_challenge_info(TRICKTEAM_CHALLENGE_TYPE_GET_SPECIFIC_1).str_id);
            if (card.suit != TRICKTEAM_SUIT_NONE && card.value != 0) {
                wstr += sprintf(wstr, "/%c%c", suit_2_char[card.suit], val_2_char[card.value]);
            }
        } break;
        //TODO more types
        case TRICKTEAM_CHALLENGE_TYPE_COUNT:
        case TRICKTEAM_CHALLENGE_TYPE_SIZE_MAX:
        default: {
            assert(0);
        } break;
    }
    return wstr - str;
}

static challenge_info get_challenge_info(TRICKTEAM_CHALLENGE_TYPE type)
{
    switch (type) {
        case TRICKTEAM_CHALLENGE_TYPE_NONE: {
            assert(0);
        } break;
        case TRICKTEAM_CHALLENGE_TYPE_GET_SPECIFIC_1:
            return {"GS1", 4, {1, 1, 1}};
        case TRICKTEAM_CHALLENGE_TYPE_COUNT:
        case TRICKTEAM_CHALLENGE_TYPE_SIZE_MAX:
        default: {
            assert(0);
        } break;
    }
    assert(0);
    return {"NONE", 0, {0xFF, 0xFF, 0xFF}};
}

static uint64_t get_player_sorted_cards_bitfield_igf(game* self, player_id player)
{
    state_repr& data = get_repr(self);
    uint64_t card_bits = 0;
    for (uint8_t i = 0; i < VEC_LEN(&data.players[player - 1].cards); i++) {
        trickteam_card card = data.players[player - 1].cards[i];
        size_t suit_offset = (size_t[]){0, 4, 4 + 9 * 1, 4 + 9 * 2, 4 + 9 * 3}[card.suit - 1];
        size_t card_max = (size_t[]){4, 9, 9, 9, 9}[card.suit - 1];
        size_t offset = suit_offset + (card_max - card.value);
        card_bits |= ((uint64_t)1 << offset);
    }
    return card_bits;
}

static TRICKTEAM_COMMUNICATION char_2_comm(char c)
{
    switch (c) {
        default:
        case '~': {
            return TRICKTEAM_COMMUNICATION_NONE;
        } break;
        case 'v': {
            return TRICKTEAM_COMMUNICATION_LOWEST;
        } break;
        case '*': {
            return TRICKTEAM_COMMUNICATION_SINGULAR;
        } break;
        case '^': {
            return TRICKTEAM_COMMUNICATION_HIGHEST;
        } break;
    }
}

static TRICKTEAM_SUIT char_2_suit(char c)
{
    switch (c) {
        default:
        case '?': {
            return TRICKTEAM_SUIT_NONE;
        } break;
        case 'B': {
            return TRICKTEAM_SUIT_BLACK;
        } break;
        case 'Y': {
            return TRICKTEAM_SUIT_YELLOW;
        } break;
        case 'T': {
            return TRICKTEAM_SUIT_TEAL;
        } break;
        case 'P': {
            return TRICKTEAM_SUIT_PINK;
        } break;
        case 'G': {
            return TRICKTEAM_SUIT_GREEN;
        } break;
    }
}

static uint8_t char_2_val(char c)
{
    switch (c) {
        default:
        case '?': {
            return 0;
        } break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': {
            return c - '0';
        } break;
    }
}

#ifdef __cplusplus
}
#endif
