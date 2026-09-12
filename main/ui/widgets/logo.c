/* ------------------------------------------------------------------------
 * Foyer's logo — see logo.h.
 * --------------------------------------------------------------------- */
#include "logo.h"

#include <math.h>

#include "logo_shape.h"
#include "comuni.h"

/* From drawing units to pixels. Coordinates are rounded one by one, edge by
   edge and not position plus width: so two rectangles that touch in the
   drawing touch on the screen too, instead of leaving a thread of
   background between the stem and the arm of an F. */
static int32_t px(float units, float scale) { return (int32_t)lroundf(units * scale); }

static void free_points(lv_event_t *e)
{
    void *points = lv_event_get_user_data(e);
    if (points) lv_free(points);
}

static void draw(lv_obj_t *c, const logo_prim_t *p, float s, lv_color_t ink)
{
    const lv_color_t colour = p->role == 'a' ? C_MARCHIO : ink;

    switch (p->kind) {
    case LOGO_RECT: {
        const int32_t x0 = px(p->p[0].x, s), y0 = px(p->p[0].y, s);
        const int32_t x1 = px(p->p[0].x + p->p[1].x, s);
        const int32_t y1 = px(p->p[0].y + p->p[1].y, s);
        lv_obj_t *r = ui_pannello(c, colour);
        lv_obj_set_pos(r, x0, y0);
        lv_obj_set_size(r, x1 - x0 > 0 ? x1 - x0 : 1, y1 - y0 > 0 ? y1 - y0 : 1);
        break;
    }
    case LOGO_CIRCLE: {
        const float r = p->p[1].x;
        const int32_t x0 = px(p->p[0].x - r, s), y0 = px(p->p[0].y - r, s);
        const int32_t d = px(p->p[0].x + r, s) - x0;
        lv_obj_t *o = ui_pannello(c, colour);
        lv_obj_set_pos(o, x0, y0);
        lv_obj_set_size(o, d, d);
        lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
        break;
    }
    case LOGO_ARC: {
        /* lv_arc draws the stroke **inside** its box: the arc's outer edge
           is the object's edge. The drawing's radius is the stroke's middle
           line, so the box grows by half a stroke on each side. */
        const float outer = p->p[1].x + p->width / 2;
        const int32_t x0 = px(p->p[0].x - outer, s);
        const int32_t y0 = px(p->p[0].y - outer, s);
        const int32_t d = px(p->p[0].x + outer, s) - x0;
        lv_obj_t *a = lv_arc_create(c);
        lv_obj_remove_style_all(a);
        lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(a, x0, y0);
        lv_obj_set_size(a, d, d);
        lv_arc_set_bg_angles(a, (lv_value_precise_t)p->p[2].x,
                             (lv_value_precise_t)p->p[2].y);
        lv_obj_set_style_arc_width(a, px(p->width, s), LV_PART_MAIN);
        lv_obj_set_style_arc_color(a, colour, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(a, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(a, false, LV_PART_MAIN);
        /* The indicator and the knob belong to arcs used as controls: here
           there is nothing to turn. */
        lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, LV_PART_KNOB);
        break;
    }
    case LOGO_LINE: {
        const int n = p->points;
        lv_point_precise_t *points = lv_malloc(sizeof *points * (size_t)n);
        if (!points) return;
        for (int k = 0; k < n; k++) {
            points[k].x = (lv_value_precise_t)px(p->p[k].x, s);
            points[k].y = (lv_value_precise_t)px(p->p[k].y, s);
        }
        /* lv_line keeps the pointer, not a copy: the points live as long
           as the line, and go with it. */
        lv_obj_t *l = lv_line_create(c);
        lv_obj_remove_style_all(l);
        lv_line_set_points(l, points, (uint32_t)n);
        lv_obj_add_event_cb(l, free_points, LV_EVENT_DELETE, points);
        lv_obj_set_pos(l, 0, 0);
        lv_obj_set_style_line_width(l, px(p->width, s), 0);
        lv_obj_set_style_line_color(l, colour, 0);
        lv_obj_set_style_line_opa(l, (lv_opa_t)lroundf(p->opa * 255.0f), 0);
        lv_obj_set_style_line_rounded(l, p->rounded, 0);
        break;
    }
    }
}

int32_t logo_foyer_width(int32_t height, bool with_name)
{
    const float s = (float)height / (float)LOGO_HEIGHT;
    return px(with_name ? LOGO_WIDTH : LOGO_HEIGHT, s);
}

lv_obj_t *logo_foyer(lv_obj_t *parent, int32_t height, bool with_name,
                     lv_color_t ink)
{
    const float s = (float)height / (float)LOGO_HEIGHT;

    lv_obj_t *c = ui_pannello(parent, C_BG);
    lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
    lv_obj_set_size(c, logo_foyer_width(height, with_name), height);
    /* A drawing, not a control: the touch goes to whatever is underneath. */
    lv_obj_remove_flag(c, LV_OBJ_FLAG_CLICKABLE);

    for (unsigned n = 0; n < LOGO_MARK_N; n++)
        draw(c, &logo_mark[n], s, ink);
    if (with_name)
        for (unsigned n = 0; n < LOGO_NAME_N; n++)
            draw(c, &logo_name[n], s, ink);

    /* The pieces too: they are born clickable, and a logo that steals the
       touch from the button it sits on is a button that does not answer. */
    for (uint32_t n = 0; n < lv_obj_get_child_count(c); n++)
        lv_obj_remove_flag(lv_obj_get_child(c, n), LV_OBJ_FLAG_CLICKABLE);
    return c;
}
