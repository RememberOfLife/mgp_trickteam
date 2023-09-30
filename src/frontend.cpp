#include <cmath>
#include <cstdint>

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include "nanovg.h"
#include "nanovg_gl.h" //TODO is just this fine or do we need to do more here?
#include "imgui.h"
#include "mirabel/event_queue.h"
#include "mirabel/event.h"
#include "mirabel/frontend.h"
#include "mirabel/game.h"

#include "game.h"

#include "frontend.h"

namespace {

    /////
    // helpers fwddec

    // colors
    NVGcolor color_black = nvgRGB(25, 25, 25);
    NVGcolor color_white = nvgRGB(236, 236, 236);
    NVGcolor color_wood_light = nvgRGB(240, 217, 181);
    NVGcolor color_wood_norm = nvgRGB(201, 144, 73);
    NVGcolor color_wood_dark = nvgRGB(161, 119, 67);

    NVGcolor color_card_stock = nvgRGB(244, 228, 210);

    NVGcolor color_suit_yellow = nvgRGB(255, 193, 7);
    NVGcolor color_suit_teal = nvgRGB(13, 202, 240);
    NVGcolor color_suit_pink = nvgRGB(214, 51, 132);
    NVGcolor color_suit_green = nvgRGB(25, 135, 84);

    //TODO
    void draw_symbol(NVGcontext* dc, float x, float y, float size, TRICKTEAM_SUIT suit);
    void draw_card(NVGcontext* dc, float x, float y, float rot_rad, float size_w, trickteam_card card);
    void draw_challenge(NVGcontext* dc); //TODO

    /////
    // core frontend

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
        if (nvgCreateFont(data.dc, "ff_num", "../plugins/mgp_trickteam/res/fonts/opensans/OpenSans-ExtraBold.ttf") < 0) {
            //TODO error and deconstruct
        }

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

        {
            //TODO main frame drawing

            /*TODO orderless
            other players each with card cnt, challenges, communications
            current game state, i.e. dealing cards? dealing challenges? what challenge is being specified
            communication menu for hand cards? and passing and passed state of other players
            communication for pov player
            challenges for pov player
            */

            // show last trick
            //TODO

            // show current trick
            //TODO

            // show player hand cards
            //TODO
        }

        nvgRestore(dc);

        nvgEndFrame(dc);

        return ERR_OK;
    }

    error_code is_game_compatible(const game_methods* methods)
    {
        if (strcmp(methods->game_name, "TrickTeam") == 0 && strcmp(methods->variant_name, "Standard") == 0 && strcmp(methods->impl_name, "mirabel_supplemental") == 0) {
            return ERR_OK;
        }
        return ERR_INVALID_INPUT;
    }

    /////
    // additional helpers

    void draw_symbol(NVGcontext* dc, float x, float y, float size, TRICKTEAM_SUIT suit)
    {
    }

    void draw_card(NVGcontext* dc, float x, float y, float rot_rad, float size_w, trickteam_card card)
    {
        const float size_h = size_w * 1.6;
        const float hsize_w = size_w / 2;
        const float hsize_h = size_h / 2;
        const float strokew = size_w * 0.04;
        const float corner_rad = size_w * 0.08;
        nvgSave(dc);
        nvgTranslate(dc, x, y);
        nvgRotate(dc, rot_rad);
        nvgStrokeWidth(dc, strokew);
        for (int i = 0; i < 2; i++) {
            nvgBeginPath(dc);
            for (int j = 0; j < 4; j++) {
                nvgRoundedRect(dc, -hsize_w, -hsize_h, size_w, size_h, corner_rad);
            }
            if (i == 0) {
                nvgStrokeColor(dc, color_black);
                nvgStroke(dc);
            } else {
                nvgFillColor(dc, color_card_stock);
                nvgFill(dc);
            }
        }

        //TODO want mode for big and small? i.e. with center image and side values and one with just center value

        // center value mode
        // draw number
        nvgFontSize(dc, size_w);
        nvgFontFace(dc, "ff_num");
        nvgBeginPath(dc);
        nvgTextAlign(dc, NVG_ALIGN_MIDDLE | NVG_ALIGN_CENTER);
        char textbuf[2] = "?";
        if (card.value > 0) {
            textbuf[0] = '0' + card.value;
        }
        switch (card.suit) {
            case TRICKTEAM_SUIT_NONE:
            case TRICKTEAM_SUIT_BLACK: {
                nvgFillColor(dc, color_black);
            } break;
            case TRICKTEAM_SUIT_YELLOW: {
                nvgFillColor(dc, color_suit_yellow);
            } break;
            case TRICKTEAM_SUIT_TEAL: {
                nvgFillColor(dc, color_suit_teal);
            } break;
            case TRICKTEAM_SUIT_PINK: {
                nvgFillColor(dc, color_suit_pink);
            } break;
            case TRICKTEAM_SUIT_GREEN: {
                nvgFillColor(dc, color_suit_green);
            } break;
            case TRICKTEAM_SUIT_COUNT:
            case TRICKTEAM_SUIT_SIZE_MAX: {
                assert(0);
            } break;
        }
        nvgText(dc, 0, -size_h * 0.15, textbuf, NULL);
        nvgFill(dc);
        if (card.value == 6 || card.value == 9) {
            nvgBeginPath(dc);
            nvgCircle(dc, hsize_w * 0.5, -size_w * 0.02, hsize_w * 0.25);
            nvgFillColor(dc, color_white);
            nvgFill(dc);
            nvgBeginPath(dc);
            nvgCircle(dc, hsize_w * 0.5, -size_w * 0.02, hsize_w * 0.15);
            switch (card.suit) {
                case TRICKTEAM_SUIT_NONE:
                case TRICKTEAM_SUIT_BLACK: {
                    nvgFillColor(dc, color_black);
                } break;
                case TRICKTEAM_SUIT_YELLOW: {
                    nvgFillColor(dc, color_suit_yellow);
                } break;
                case TRICKTEAM_SUIT_TEAL: {
                    nvgFillColor(dc, color_suit_teal);
                } break;
                case TRICKTEAM_SUIT_PINK: {
                    nvgFillColor(dc, color_suit_pink);
                } break;
                case TRICKTEAM_SUIT_GREEN: {
                    nvgFillColor(dc, color_suit_green);
                } break;
                case TRICKTEAM_SUIT_COUNT:
                case TRICKTEAM_SUIT_SIZE_MAX: {
                    assert(0);
                } break;
            }
            nvgFill(dc);
        }
        // draw symbol
        draw_symbol(dc, 0, size_h * 0.25, size_w * 0.6, card.suit);

        nvgRestore(dc);
    }

    void draw_challenge(NVGcontext* dc) //TODO
    {
        //TODO how to display these properly?
        //TODO big enum?
    }

    /*
    //TODO also need zoom like function to get a closer view at challenges
    //TODO how to show hand cards, spread in an arc or just a line?
    */

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
