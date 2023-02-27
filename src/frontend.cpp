#include <cstdint>

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include "nanovg.h"
#include "nanovg_gl.h" //TODO is just this fine or do we need to do more here?
#include "imgui.h"
#include "surena/game.h"
#include "mirabel/event_queue.h"
#include "mirabel/event.h"
#include "mirabel/frontend.h"

#include "frontend_plugin.h"

#include "frontend.hpp"

namespace {

    struct data_repr {
        frontend_display_data* dd;
        NVGcontext* dc;
    };

    data_repr& _get_repr(frontend* self)
    {
        return *((data_repr*)(self->data1));
    }

    error_code create(frontend* self, frontend_display_data* display_data, void* options_struct)
    {
        self->data1 = malloc(sizeof(data_repr));
        data_repr& data = _get_repr(self);
        data.dd = display_data;
        void* gl_ctx = SDL_GL_GetCurrentContext();
        data.dc = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES); //TODO is this fine?
        return ERR_OK;
    }

    error_code destroy(frontend* self)
    {
        {
            data_repr& data = _get_repr(self);
            nvgDeleteGL3(data.dc);
        }
        free(self->data1);
        self->data1 = NULL;
        return ERR_OK;
    }

    error_code runtime_opts_display(frontend* self)
    {
        return ERR_OK;
    }

    error_code process_event(frontend* self, event_any event)
    {
        event_destroy(&event);
        return ERR_OK;
    }

    error_code process_input(frontend* self, SDL_Event event)
    {
        return ERR_OK;
    }

    error_code update(frontend* self)
    {
        return ERR_OK;
    }

    error_code render(frontend* self)
    {
        data_repr& data = _get_repr(self);
        frontend_display_data& dd = *data.dd;
        NVGcontext* dc = data.dc;

        nvgBeginFrame(dc, dd.fbw, dd.fbh, 2);

        nvgSave(dc);
        nvgTranslate(dc, dd.x, dd.y);

        nvgBeginPath(dc);
        nvgRect(dc, -10, -10, dd.w + 20, dd.h + 20);
        nvgFillColor(dc, nvgRGB(201, 144, 73));
        nvgFill(dc);

        nvgRestore(dc);

        nvgEndFrame(dc);

        return ERR_OK;
    }

    error_code is_game_compatible(const game_methods* methods)
    {
        return ERR_OK;
    }

} // namespace

#ifdef __cplusplus
extern "C" {
#endif

const frontend_methods trickteam_fem{
    .frontend_name = "trickteam",
    .version = semver{0, 1, 0},
    .features = frontend_feature_flags{
        .error_strings = false,
        .options = false,
    },

    .internal_methods = NULL,

    .opts_create = NULL,
    .opts_display = NULL,
    .opts_destroy = NULL,

    .get_last_error = NULL,

    .create = create,
    .destroy = destroy,

    .runtime_opts_display = runtime_opts_display,

    .process_event = process_event,
    .process_input = process_input,
    .update = update,

    .render = render,

    .is_game_compatible = is_game_compatible,

};

#ifdef __cplusplus
}
#endif
