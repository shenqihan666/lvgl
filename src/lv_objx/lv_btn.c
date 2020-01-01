/**
 * @file lv_btn.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_btn.h"
#if LV_USE_BTN != 0

#include <string.h>
#include "../lv_core/lv_group.h"
#include "../lv_core/lv_debug.h"
#include "../lv_draw/lv_draw.h"
#include "../lv_themes/lv_theme.h"
#include "../lv_misc/lv_area.h"
#include "../lv_misc/lv_color.h"
#include "../lv_misc/lv_math.h"

/*********************
 *      DEFINES
 *********************/
#define LV_OBJX_NAME "lv_btn"
#define LV_BTN_INK_VALUE_MAX 256
#define LV_BTN_INK_VALUE_MAX_SHIFT 8

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_design_res_t lv_btn_design(lv_obj_t * btn, const lv_area_t * clip_area, lv_design_mode_t mode);
static lv_res_t lv_btn_signal(lv_obj_t * btn, lv_signal_t sign, void * param);

#if LV_USE_ANIMATION && LV_BTN_INK_EFFECT
static void lv_btn_ink_effect_anim(lv_obj_t * btn, lv_anim_value_t val);
static void lv_btn_ink_effect_anim_ready(lv_anim_t * a);
#endif

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_signal_cb_t ancestor_signal;
static lv_design_cb_t ancestor_design;

#if LV_USE_ANIMATION && LV_BTN_INK_EFFECT
static lv_coord_t ink_act_value;
static lv_obj_t * ink_obj;
static lv_btn_state_t ink_bg_state;
static lv_btn_state_t ink_top_state;
static bool ink_ready;
static bool ink_playback;
static lv_point_t ink_point;
#endif

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Create a button object
 * @param par pointer to an object, it will be the parent of the new button
 * @param copy pointer to a button object, if not NULL then the new object will be copied from it
 * @return pointer to the created button
 */
lv_obj_t * lv_btn_create(lv_obj_t * par, const lv_obj_t * copy)
{
    LV_LOG_TRACE("button create started");

    lv_obj_t * new_btn;

    new_btn = lv_cont_create(par, copy);
    LV_ASSERT_MEM(new_btn);
    if(new_btn == NULL) return NULL;

    if(ancestor_signal == NULL) ancestor_signal = lv_obj_get_signal_cb(new_btn);
    if(ancestor_design == NULL) ancestor_design = lv_obj_get_design_cb(new_btn);

    /*Allocate the extended data*/
    lv_btn_ext_t * ext = lv_obj_allocate_ext_attr(new_btn, sizeof(lv_btn_ext_t));
    LV_ASSERT_MEM(ext);
    if(ext == NULL) {
        lv_obj_del(new_btn);
        return NULL;
    }

    ext->toggle = 0;

#if LV_USE_ANIMATION && LV_BTN_INK_EFFECT
    ext->ink_in_time   = 0;
    ext->ink_wait_time = 0;
    ext->ink_out_time  = 0;
#endif

    lv_obj_set_signal_cb(new_btn, lv_btn_signal);
    lv_obj_set_design_cb(new_btn, lv_btn_design);

    /*If no copy do the basic initialization*/
    if(copy == NULL) {
        /*Set layout if the button is not a screen*/
        if(par != NULL) {
            lv_btn_set_layout(new_btn, LV_LAYOUT_CENTER);
        }

        lv_obj_set_click(new_btn, true); /*Be sure the button is clickable*/

        /*Set the default styles*/
        lv_obj_reset_style(new_btn, LV_BTN_PART_MAIN);
        lv_obj_add_style_class(new_btn, LV_BTN_PART_MAIN, _t(BTN));
    }
    /*Copy 'copy'*/
    else {
        lv_btn_ext_t * copy_ext = lv_obj_get_ext_attr(copy);
        ext->toggle             = copy_ext->toggle;
#if LV_USE_ANIMATION && LV_BTN_INK_EFFECT
        ext->ink_in_time   = copy_ext->ink_in_time;
        ext->ink_wait_time = copy_ext->ink_wait_time;
        ext->ink_out_time  = copy_ext->ink_out_time;
#endif
//        memcpy((void*) ext->styles, copy_ext->styles, sizeof(ext->styles));

        /*Refresh the style with new signal function*/
        lv_obj_refresh_style(new_btn,LV_BTN_PART_MAIN);
    }

    LV_LOG_INFO("button created");

    return new_btn;
}

/*=====================
 * Setter functions
 *====================*/

/**
 * Enable the toggled states
 * @param btn pointer to a button object
 * @param tgl true: enable toggled states, false: disable
 */
void lv_btn_set_toggle(lv_obj_t * btn, bool tgl)
{
    LV_ASSERT_OBJ(btn, LV_OBJX_NAME);

    lv_btn_ext_t * ext = lv_obj_get_ext_attr(btn);

    ext->toggle = tgl != false ? 1 : 0;
}

/**
 * Set the state of the button
 * @param btn pointer to a button object
 * @param state the new state of the button (from lv_btn_state_t enum)
 */
void lv_btn_set_state(lv_obj_t * btn, lv_btn_state_t state)
{
    LV_ASSERT_OBJ(btn, LV_OBJX_NAME);

    switch(state) {
        case LV_BTN_STATE_REL:
            lv_obj_clear_state(btn, LV_OBJ_STATE_PRESSED | LV_OBJ_STATE_CHECKED);
            break;
        case LV_BTN_STATE_PR:
            lv_obj_clear_state(btn, LV_OBJ_STATE_CHECKED);
            lv_obj_set_state(btn, LV_OBJ_STATE_PRESSED);
            break;
        case LV_BTN_STATE_TGL_REL:
            lv_obj_set_state(btn, LV_OBJ_STATE_CHECKED);
            lv_obj_clear_state(btn, LV_OBJ_STATE_PRESSED);
            break;
        case LV_BTN_STATE_TGL_PR:
            lv_obj_set_state(btn, LV_OBJ_STATE_PRESSED | LV_OBJ_STATE_CHECKED);
            break;
    }
}

/**
 * Toggle the state of the button (ON->OFF, OFF->ON)
 * @param btn pointer to a button object
 */
void lv_btn_toggle(lv_obj_t * btn)
{
    LV_ASSERT_OBJ(btn, LV_OBJX_NAME);

    if(lv_obj_get_state(btn) & LV_OBJ_STATE_CHECKED) {
        lv_obj_clear_state(btn, LV_OBJ_STATE_CHECKED);
    } else {
        lv_obj_set_state(btn, LV_OBJ_STATE_CHECKED);
    }
}

/**
 * Set time of the ink effect (draw a circle on click to animate in the new state)
 * @param btn pointer to a button object
 * @param time the time of the ink animation
 */
void lv_btn_set_ink_in_time(lv_obj_t * btn, uint16_t time)
{
    LV_ASSERT_OBJ(btn, LV_OBJX_NAME);

#if LV_USE_ANIMATION && LV_BTN_INK_EFFECT
    lv_btn_ext_t * ext = lv_obj_get_ext_attr(btn);
    ext->ink_in_time   = time;
#else
    (void)btn;  /*Unused*/
    (void)time; /*Unused*/
    LV_LOG_WARN("`lv_btn_set_ink_ink_time` has no effect if LV_BTN_INK_EFEFCT or LV_USE_ANIMATION "
                "is disabled")
#endif
}

/**
 * Set the wait time before the ink disappears
 * @param btn pointer to a button object
 * @param time the time of the ink animation
 */
void lv_btn_set_ink_wait_time(lv_obj_t * btn, uint16_t time)
{
    LV_ASSERT_OBJ(btn, LV_OBJX_NAME);


#if LV_USE_ANIMATION && LV_BTN_INK_EFFECT
    lv_btn_ext_t * ext = lv_obj_get_ext_attr(btn);
    ext->ink_wait_time = time;
#else
    (void)btn;  /*Unused*/
    (void)time; /*Unused*/
    LV_LOG_WARN("`lv_btn_set_ink_wait_time` has no effect if LV_BTN_INK_EFEFCT or LV_USE_ANIMATION "
                "is disabled")
#endif
}

/**
 * Set time of the ink out effect (animate to the released state)
 * @param btn pointer to a button object
 * @param time the time of the ink animation
 */
void lv_btn_set_ink_out_time(lv_obj_t * btn, uint16_t time)
{
    LV_ASSERT_OBJ(btn, LV_OBJX_NAME);

#if LV_USE_ANIMATION && LV_BTN_INK_EFFECT
    lv_btn_ext_t * ext = lv_obj_get_ext_attr(btn);
    ext->ink_out_time  = time;
#else
    (void)btn;  /*Unused*/
    (void)time; /*Unused*/
    LV_LOG_WARN("`lv_btn_set_ink_out_time` has no effect if LV_BTN_INK_EFEFCT or LV_USE_ANIMATION "
                "is disabled")
#endif
}

/*=====================
 * Getter functions
 *====================*/

/**
 * Get the current state of the button
 * @param btn pointer to a button object
 * @return the state of the button (from lv_btn_state_t enum)
 */
lv_btn_state_t lv_btn_get_state(const lv_obj_t * btn)
{
    LV_ASSERT_OBJ(btn, LV_OBJX_NAME);

    lv_obj_state_t state = lv_obj_get_state(btn);

    if(state & LV_OBJ_STATE_CHECKED) {
        if(state & LV_OBJ_STATE_PRESSED) return LV_BTN_STATE_TGL_PR;
        else return LV_BTN_STATE_TGL_REL;
    } else {
        if(state & LV_OBJ_STATE_PRESSED) return LV_BTN_STATE_PR;
        else return LV_BTN_STATE_REL;
    }

}

/**
 * Get the toggle enable attribute of the button
 * @param btn pointer to a button object
 * @return true: toggle enabled, false: disabled
 */
bool lv_btn_get_toggle(const lv_obj_t * btn)
{
    LV_ASSERT_OBJ(btn, LV_OBJX_NAME);

    lv_btn_ext_t * ext = lv_obj_get_ext_attr(btn);

    return ext->toggle != 0 ? true : false;
}

/**
 * Get time of the ink in effect (draw a circle on click to animate in the new state)
 * @param btn pointer to a button object
 * @return the time of the ink animation
 */
uint16_t lv_btn_get_ink_in_time(const lv_obj_t * btn)
{
    LV_ASSERT_OBJ(btn, LV_OBJX_NAME);

#if LV_USE_ANIMATION && LV_BTN_INK_EFFECT
    lv_btn_ext_t * ext = lv_obj_get_ext_attr(btn);
    return ext->ink_in_time;
#else
    (void)btn; /*Unused*/
    return 0;
#endif
}

/**
 * Get the wait time before the ink disappears
 * @param btn pointer to a button object
 * @return the time of the ink animation
 */
uint16_t lv_btn_get_ink_wait_time(const lv_obj_t * btn)
{
    LV_ASSERT_OBJ(btn, LV_OBJX_NAME);

#if LV_USE_ANIMATION && LV_BTN_INK_EFFECT
    lv_btn_ext_t * ext = lv_obj_get_ext_attr(btn);
    return ext->ink_wait_time;
#else
    (void)btn; /*Unused*/
    return 0;
#endif
}
/**
 * Get time of the ink out effect (animate to the releases state)
 * @param btn pointer to a button object
 * @return the time of the ink animation
 */
uint16_t lv_btn_get_ink_out_time(const lv_obj_t * btn)
{
    LV_ASSERT_OBJ(btn, LV_OBJX_NAME);

#if LV_USE_ANIMATION && LV_BTN_INK_EFFECT
    lv_btn_ext_t * ext = lv_obj_get_ext_attr(btn);
    return ext->ink_out_time;
#else
    (void)btn; /*Unused*/
    return 0;
#endif
}

lv_style_dsc_t * lv_btn_get_style(lv_obj_t * cont, uint8_t type)
{
    lv_style_dsc_t * style_dsc_p;
    switch(type) {
    case LV_BTN_PART_MAIN:
        style_dsc_p = &cont->style_dsc;
        break;
    default:
        style_dsc_p = NULL;
    }

    return style_dsc_p;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Handle the drawing related tasks of the drop down lists
 * @param btn pointer to an object
 * @param mask the object will be drawn only in this area
 * @param mode LV_DESIGN_COVER_CHK: only check if the object fully covers the 'mask_p' area
 *                                  (return 'true' if yes)
 *             LV_DESIGN_DRAW: draw the object (always return 'true')
 *             LV_DESIGN_DRAW_POST: drawing after every children are drawn
 * @param return an element of `lv_design_res_t`
 */
static lv_design_res_t lv_btn_design(lv_obj_t * btn, const lv_area_t * clip_area, lv_design_mode_t mode)
{
    if(mode == LV_DESIGN_COVER_CHK) {
        return ancestor_design(btn, clip_area, mode);
    } else if(mode == LV_DESIGN_DRAW_MAIN) {

#if LV_USE_ANIMATION && LV_BTN_INK_EFFECT
        if(btn != ink_obj) {
            ancestor_design(btn, clip_area, mode);
        } else {
            lv_opa_t opa_scale = lv_obj_get_opa_scale(btn);
            lv_btn_ext_t * ext = lv_obj_get_ext_attr(btn);

            /*Draw the normal button*/
            if(ink_playback == false) {
                lv_style_t style_tmp;
                lv_style_copy(&style_tmp, ext->styles[ink_bg_state]);
                style_tmp.body.shadow.width = ext->styles[ink_top_state]->body.shadow.width;
                lv_draw_rect(&btn->coords, clip_area, &style_tmp, opa_scale);

                lv_coord_t w     = lv_obj_get_width(btn);
                lv_coord_t h     = lv_obj_get_height(btn);
                lv_coord_t r_max = LV_MATH_MIN(w, h) / 2;

                /*In the first part of the animation increase the size of the circle (ink effect) */
                lv_area_t cir_area;

                lv_coord_t coord_state =
                    ink_act_value < LV_BTN_INK_VALUE_MAX / 2 ? ink_act_value : LV_BTN_INK_VALUE_MAX / 2;
                lv_point_t p_act;
                p_act.x          = ink_point.x;
                p_act.y          = ink_point.y;
                lv_coord_t x_err = (btn->coords.x1 + w / 2) - p_act.x;
                lv_coord_t y_err = (btn->coords.y1 + h / 2) - p_act.y;

                p_act.x += (x_err * coord_state) >> (LV_BTN_INK_VALUE_MAX_SHIFT - 1);
                p_act.y += (y_err * coord_state) >> (LV_BTN_INK_VALUE_MAX_SHIFT - 1);

                lv_coord_t half_side = LV_MATH_MAX(w, h) / 2;
                cir_area.x1          = p_act.x - ((half_side * coord_state) >> (LV_BTN_INK_VALUE_MAX_SHIFT - 1));
                cir_area.y1          = p_act.y - ((half_side * coord_state) >> (LV_BTN_INK_VALUE_MAX_SHIFT - 1));
                cir_area.x2          = p_act.x + ((half_side * coord_state) >> (LV_BTN_INK_VALUE_MAX_SHIFT - 1));
                cir_area.y2          = p_act.y + ((half_side * coord_state) >> (LV_BTN_INK_VALUE_MAX_SHIFT - 1));

                lv_area_intersect(&cir_area, &btn->coords,
                                  &cir_area); /*Limit the area. (It might be too big on the smaller side)*/

                /*In the second part animate the radius. Circle -> body.radius*/
                lv_coord_t r_state =
                    ink_act_value > LV_BTN_INK_VALUE_MAX / 2 ? ink_act_value - LV_BTN_INK_VALUE_MAX / 2 : 0;

                lv_style_copy(&style_tmp, ext->styles[ink_top_state]);
                style_tmp.body.radius       = r_max + (((ext->styles[ink_bg_state]->body.radius - r_max) * r_state) >>
                                                 (LV_BTN_INK_VALUE_MAX_SHIFT - 1));
                style_tmp.body.border.width = 0;

                /*Draw the circle*/
                lv_draw_rect(&cir_area, clip_area, &style_tmp, opa_scale);
            } else {
                lv_style_t res;
                lv_style_copy(&res, ext->styles[ink_bg_state]);
                lv_style_mix(ext->styles[ink_bg_state], ext->styles[ink_top_state], &res, ink_act_value);
                lv_draw_rect(&btn->coords, clip_area, &res, opa_scale);
            }
        }
#else

        lv_draw_rect_dsc_t draw_dsc;
        lv_draw_rect_dsc_init(&draw_dsc);
        lv_obj_init_draw_rect_dsc(btn, LV_OBJ_PART_MAIN, &draw_dsc);
        lv_draw_rect(&btn->coords, clip_area, &draw_dsc);

        if(lv_obj_get_style_value(btn, LV_OBJ_PART_MAIN, LV_STYLE_CLIP_CORNER)) {
            lv_draw_mask_radius_param_t * mp = lv_mem_buf_get(sizeof(lv_draw_mask_radius_param_t));

            lv_coord_t r = lv_obj_get_style_value(btn, LV_OBJ_PART_MAIN, LV_STYLE_RADIUS);

            lv_draw_mask_radius_init(mp, &btn->coords, r, false);
            /*Add the mask and use `obj+8` as custom id. Don't use `obj` directly because it might be used by the user*/
            lv_draw_mask_add(mp, btn + 8);
        }
#endif
    } else if(mode == LV_DESIGN_DRAW_POST) {
        ancestor_design(btn, clip_area, mode);
    }

    return LV_DESIGN_RES_OK;
}

/**
 * Signal function of the button
 * @param btn pointer to a button object
 * @param sign a signal type from lv_signal_t enum
 * @param param pointer to a signal specific variable
 * @return LV_RES_OK: the object is not deleted in the function; LV_RES_INV: the object is deleted
 */
static lv_res_t lv_btn_signal(lv_obj_t * btn, lv_signal_t sign, void * param)
{
    lv_res_t res;

    /* Include the ancient signal function */
    res = ancestor_signal(btn, sign, param);
    if(res != LV_RES_OK) return res;
    if(sign == LV_SIGNAL_GET_TYPE) return lv_obj_handle_get_type_signal(param, LV_OBJX_NAME);

    bool tgl           = lv_btn_get_toggle(btn);
    lv_btn_state_t state = lv_btn_get_state(btn);
    if(sign == LV_SIGNAL_PRESSED) {
        /*Refresh the state*/
        if(state == LV_BTN_STATE_REL) {
            lv_btn_set_state(btn, LV_BTN_STATE_PR);
#if LV_USE_ANIMATION && LV_BTN_INK_EFFECT
            ink_bg_state  = LV_BTN_STATE_REL;
            ink_top_state = LV_BTN_STATE_PR;
#endif
        } else if(state == LV_BTN_STATE_TGL_REL) {
            lv_btn_set_state(btn, LV_BTN_STATE_TGL_PR);
#if LV_USE_ANIMATION && LV_BTN_INK_EFFECT
            ink_bg_state  = LV_BTN_STATE_TGL_REL;
            ink_top_state = LV_BTN_STATE_TGL_PR;
#endif
        }

#if LV_USE_ANIMATION && LV_BTN_INK_EFFECT
        /*Forget the old inked button*/
        if(ink_obj != NULL && ink_obj != btn) {
            lv_anim_del(ink_obj, (lv_anim_exec_xcb_t)lv_btn_ink_effect_anim);
            lv_obj_invalidate(ink_obj);
            ink_obj = NULL;
        }
        /*Save the new data for inking and start it's animation if enabled*/
        if(ext->ink_in_time > 0) {
            ink_obj      = btn;
            ink_playback = false;
            ink_ready    = false;
            lv_indev_get_point(lv_indev_get_act(), &ink_point);

            lv_anim_t a;
            a.var            = btn;
            a.start          = 0;
            a.end            = LV_BTN_INK_VALUE_MAX;
            a.exec_cb        = (lv_anim_exec_xcb_t)lv_btn_ink_effect_anim;
            a.path_cb        = lv_anim_path_linear;
            a.ready_cb       = lv_btn_ink_effect_anim_ready;
            a.act_time       = 0;
            a.time           = ext->ink_in_time;
            a.playback       = 0;
            a.playback_pause = 0;
            a.repeat         = 0;
            a.repeat_pause   = 0;
            lv_anim_create(&a);
        }
#endif
    } else if(sign == LV_SIGNAL_PRESS_LOST) {
        /*Refresh the state*/
        if(state == LV_BTN_STATE_PR)
            lv_btn_set_state(btn, LV_BTN_STATE_REL);
        else if(state == LV_BTN_STATE_TGL_PR)
            lv_btn_set_state(btn, LV_BTN_STATE_TGL_REL);
    } else if(sign == LV_SIGNAL_PRESSING) {
        /*When the button begins to drag revert pressed states to released*/
        if(lv_indev_is_dragging(param) != false) {
            if(state == LV_BTN_STATE_PR)
                lv_btn_set_state(btn, LV_BTN_STATE_REL);
            else if(state == LV_BTN_STATE_TGL_PR)
                lv_btn_set_state(btn, LV_BTN_STATE_TGL_REL);
        }
    } else if(sign == LV_SIGNAL_RELEASED) {
        /*If not dragged and it was not long press action then
         *change state and run the action*/
        if(lv_indev_is_dragging(param) == false) {
            uint32_t toggled = 0;
            if(state == LV_BTN_STATE_PR && tgl == false) {
                lv_btn_set_state(btn, LV_BTN_STATE_REL);
                toggled = 0;
            } else if(state == LV_BTN_STATE_TGL_PR && tgl == false) {
                lv_btn_set_state(btn, LV_BTN_STATE_TGL_REL);
                toggled = 1;
            } else if(state == LV_BTN_STATE_PR && tgl == true) {
                lv_btn_set_state(btn, LV_BTN_STATE_TGL_REL);
                toggled = 1;
            } else if(state == LV_BTN_STATE_TGL_PR && tgl == true) {
                lv_btn_set_state(btn, LV_BTN_STATE_REL);
                toggled = 0;
            }

            if(tgl) {
                res = lv_event_send(btn, LV_EVENT_VALUE_CHANGED, &toggled);
                if(res != LV_RES_OK) return res;
            }
        } else { /*If dragged change back the state*/
            if(state == LV_BTN_STATE_PR) {
                lv_btn_set_state(btn, LV_BTN_STATE_REL);
            } else if(state == LV_BTN_STATE_TGL_PR) {
                lv_btn_set_state(btn, LV_BTN_STATE_TGL_REL);
            }
        }

#if LV_USE_ANIMATION && LV_BTN_INK_EFFECT
        /*Draw the toggled state in the inking instead*/
        if(ext->toggle) {
            ink_top_state = ext->state;
        }
        /*If not a toggle button and the "IN" inking is ready then start an "OUT" inking*/
        else if(ink_ready && ext->ink_out_time > 0) {
            ink_obj      = btn;
            ink_playback = true; /*It is the playback. If not set `lv_btn_ink_effect_anim_ready`
                                    will start its own playback*/
            lv_indev_get_point(lv_indev_get_act(), &ink_point);

            lv_anim_t a;
            a.var            = ink_obj;
            a.start          = LV_BTN_INK_VALUE_MAX;
            a.end            = 0;
            a.exec_cb        = (lv_anim_exec_xcb_t)lv_btn_ink_effect_anim;
            a.path_cb        = lv_anim_path_linear;
            a.ready_cb       = lv_btn_ink_effect_anim_ready;
            a.act_time       = 0;
            a.time           = ext->ink_out_time;
            a.playback       = 0;
            a.playback_pause = 0;
            a.repeat         = 0;
            a.repeat_pause   = 0;
            lv_anim_create(&a);
        }
#endif
    } else if(sign == LV_SIGNAL_CONTROL) {
        char c = *((char *)param);
        if(c == LV_KEY_RIGHT || c == LV_KEY_UP) {
            if(lv_btn_get_toggle(btn)) {
                lv_btn_set_state(btn, LV_BTN_STATE_TGL_REL);

                uint32_t state = 1;
                res            = lv_event_send(btn, LV_EVENT_VALUE_CHANGED, &state);
                if(res != LV_RES_OK) return res;
            }

        } else if(c == LV_KEY_LEFT || c == LV_KEY_DOWN) {
            if(lv_btn_get_toggle(btn)) {
                lv_btn_set_state(btn, LV_BTN_STATE_REL);

                uint32_t state = 0;
                res            = lv_event_send(btn, LV_EVENT_VALUE_CHANGED, &state);
                if(res != LV_RES_OK) return res;
            }
        }
    } else if(sign == LV_SIGNAL_CLEANUP) {
#if LV_USE_ANIMATION && LV_BTN_INK_EFFECT
        if(btn == ink_obj) {
            lv_anim_del(ink_obj, (lv_anim_exec_xcb_t)lv_btn_ink_effect_anim);
            ink_obj = NULL;
        }
#endif
    }

    return res;
}

#if LV_USE_ANIMATION && LV_BTN_INK_EFFECT

/**
 * The animator function of inking. CAlled to increase the radius of ink
 * @param btn pointer to the animated button
 * @param val the new radius
 */
static void lv_btn_ink_effect_anim(lv_obj_t * btn, lv_anim_value_t val)
{
    if(btn) {
        ink_act_value = val;
        lv_obj_invalidate(btn);
    }
}

/**
 * Called to clean up when the ink animation is ready
 * @param a unused
 */
static void lv_btn_ink_effect_anim_ready(lv_anim_t * a)
{
    (void)a; /*Unused*/

    lv_btn_ext_t * ext   = lv_obj_get_ext_attr(ink_obj);
    lv_btn_state_t state = lv_btn_get_state(ink_obj);

    lv_obj_invalidate(ink_obj);
    ink_ready = true;

    if((state == LV_BTN_STATE_REL || state == LV_BTN_STATE_TGL_REL) && ext->toggle == 0 && ink_playback == false) {
        lv_anim_t new_a;
        new_a.var            = ink_obj;
        new_a.start          = LV_BTN_INK_VALUE_MAX;
        new_a.end            = 0;
        new_a.exec_cb        = (lv_anim_exec_xcb_t)lv_btn_ink_effect_anim;
        new_a.path_cb        = lv_anim_path_linear;
        new_a.ready_cb       = lv_btn_ink_effect_anim_ready;
        new_a.act_time       = -ext->ink_wait_time;
        new_a.time           = ext->ink_out_time;
        new_a.playback       = 0;
        new_a.playback_pause = 0;
        new_a.repeat         = 0;
        new_a.repeat_pause   = 0;
        lv_anim_create(&new_a);

        ink_playback = true;
    } else {
        ink_obj = NULL;
    }
}
#endif /*LV_USE_ANIMATION*/

#endif
