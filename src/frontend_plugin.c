#include <stdint.h>

#include "mirabel/frontend.h"

#include "frontend_plugin.h"

uint64_t plugin_get_frontend_capi_version()
{
    return MIRABEL_FRONTEND_API_VERSION;
}

void plugin_init_frontend()
{
    // pass
}

void plugin_get_frontend_methods(uint32_t* count, const frontend_methods** methods)
{
    *count = 1;
    if (methods != NULL) {
        *methods = &trickteam_fem;
    }
}

void plugin_cleanup_frontend()
{
    // pass
}
