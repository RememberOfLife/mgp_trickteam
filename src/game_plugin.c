#include <stdint.h>

#include "surena/game.h"

#include "game.h"

uint64_t plugin_get_game_capi_version()
{
    return SURENA_GAME_API_VERSION;
}

void plugin_init_game()
{
    // pass
}

void plugin_get_game_methods(uint32_t* count, const game_methods** methods)
{
    *count = 1;
    if (methods != NULL) {
        *methods = &trickteam_standard_gbe;
    }
}

void plugin_cleanup_game()
{
    // pass
}
