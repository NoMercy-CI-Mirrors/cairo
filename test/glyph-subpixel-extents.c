/*
 * Copyright © 2026 Behdad Esfahbod
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "cairo-test.h"

#define SIZE 48

/* A user font isolates positioning from FreeType's hint-metrics handling.
 * https://gitlab.freedesktop.org/cairo/cairo/-/issues/990
 */
static cairo_status_t
render_glyph (cairo_scaled_font_t *font,
	      unsigned long index,
	      cairo_t *cr,
	      cairo_text_extents_t *extents)
{
    cairo_rectangle (cr, 0, -1, 1, 1);
    cairo_fill (cr);
    extents->x_bearing = 0;
    extents->y_bearing = -1;
    extents->width = 1;
    extents->height = 1;
    extents->x_advance = 1;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
draw_glyphs (cairo_surface_t *surface,
	     cairo_font_face_t *face,
	     cairo_glyph_t *glyphs,
	     int num_glyphs,
	     cairo_bool_t clip)
{
    cairo_font_options_t *options;
    cairo_status_t status;
    cairo_t *cr = cairo_create (surface);

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_paint (cr);
    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgb (cr, 1, 1, 1);

    if (clip) {
	/* This forces a clip mask without touching the glyphs. */
	cairo_move_to (cr, 0, 0);
	cairo_line_to (cr, SIZE, 0);
	cairo_line_to (cr, SIZE, SIZE);
	cairo_line_to (cr, 1, SIZE);
	cairo_close_path (cr);
	cairo_clip (cr);
    }

    options = cairo_font_options_create ();
    cairo_font_options_set_antialias (options, CAIRO_ANTIALIAS_GRAY);
    cairo_set_font_options (cr, options);
    cairo_font_options_destroy (options);
    cairo_set_font_face (cr, face);
    cairo_set_font_size (cr, 8);
    cairo_show_glyphs (cr, glyphs, num_glyphs);
    status = cairo_status (cr);
    cairo_destroy (cr);
    return status;
}

static cairo_test_status_t
check_target (cairo_test_context_t *ctx,
	      const cairo_boilerplate_target_t *target,
	      cairo_font_face_t *face)
{
    cairo_surface_t *surface;
    cairo_test_status_t result = CAIRO_TEST_SUCCESS;
    void *closure = NULL;
    int n, i;

    surface = target->create_surface (ctx->test_name, target->content,
				      SIZE, SIZE, SIZE, SIZE,
				      CAIRO_BOILERPLATE_MODE_TEST, &closure);
    if (surface == NULL)
	return CAIRO_TEST_UNTESTED;
    if (cairo_surface_status (surface)) {
	result = cairo_test_status_from_status (ctx, cairo_surface_status (surface));
	cairo_surface_destroy (surface);
	return result;
    }

    for (n = 1; n <= 2 && result == CAIRO_TEST_SUCCESS; n++) {
	for (i = 0; i < 64; i++) {
	    cairo_glyph_t glyphs[2];
	    cairo_surface_t *images[2];
	    cairo_status_t status;
	    int clip;

	    glyphs[0].index = 1;
	    glyphs[0].x = 8 + (i % 8) / 8.;
	    glyphs[0].y = 24 + (i / 8) / 8.;
	    glyphs[1] = glyphs[0];
	    glyphs[1].x += 16;

	    for (clip = 0; clip < 2; clip++) {
		status = draw_glyphs (surface, face, glyphs, n, clip);
		if (status)
		    break;
		images[clip] = target->get_image_surface (surface, 0, SIZE, SIZE);
		status = cairo_surface_status (images[clip]);
		if (status) {
		    cairo_surface_destroy (images[clip]);
		    break;
		}
	    }
	    if (status) {
		if (clip == 1)
		    cairo_surface_destroy (images[0]);
		result = cairo_test_status_from_status (ctx, status);
		break;
	    }

	    cairo_surface_flush (images[0]);
	    cairo_surface_flush (images[1]);
	    if (memcmp (cairo_image_surface_get_data (images[0]),
			cairo_image_surface_get_data (images[1]),
			cairo_image_surface_get_stride (images[0]) * SIZE) != 0)
	    {
		cairo_test_log (ctx, "%s, content %x, %d glyphs at (%g, %g): clip lost ink\n",
				target->name, target->content, n,
				glyphs[0].x, glyphs[0].y);
		result = CAIRO_TEST_FAILURE;
	    }
	    cairo_surface_destroy (images[0]);
	    cairo_surface_destroy (images[1]);
	    if (result != CAIRO_TEST_SUCCESS)
		break;
	}
    }

    cairo_surface_destroy (surface);
    if (target->cleanup)
	target->cleanup (closure);
    return result;
}

static cairo_test_status_t
preamble (cairo_test_context_t *ctx)
{
    cairo_font_face_t *face;
    cairo_test_status_t result = CAIRO_TEST_UNTESTED;
    size_t i;

    face = cairo_user_font_face_create ();
    cairo_user_font_face_set_render_glyph_func (face, render_glyph);

    for (i = 0; i < ctx->num_targets; i++) {
	const cairo_boilerplate_target_t *target = ctx->targets_to_test[i];
	cairo_test_status_t status;

	if (strcmp (target->name, "image") && strcmp (target->name, "xlib"))
	    continue;
	status = check_target (ctx, target, face);
	if (status == CAIRO_TEST_UNTESTED)
	    continue;
	result = status;
	if (result != CAIRO_TEST_SUCCESS)
	    break;
    }

    cairo_font_face_destroy (face);
    return result;
}

CAIRO_TEST (glyph_subpixel_extents,
	    "Subpixel glyph positioning preserves ink under a clip",
	    "text, extents, clip", /* keywords */
	    NULL, /* requirements */
	    0, 0,
	    preamble, NULL)
