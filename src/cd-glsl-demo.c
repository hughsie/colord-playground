/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2011 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define COGL_ENABLE_EXPERIMENTAL_API
#include <cogl/cogl.h>
#include <clutter/clutter.h>
#include <lcms2.h>

/* the texture will be 16x16x16 and 3bpp */
#define GCM_GLSL_LOOKUP_SIZE 	16

static const char fragment_shader_source[] =
	"uniform sampler2D main_texture;\n"
	"uniform sampler3D color_data;\n"
	"\n"
	"void\n"
	"main ()\n"
	"{\n"
	"	vec3 tex_color = texture2D (main_texture, gl_TexCoord[0].st).rgb;\n"
	"	gl_FragColor = texture3D (color_data, tex_color);\n"
	"}";

/**
 * gcm_glsl_error_cb:
 **/
static void
gcm_glsl_error_cb (cmsContext ContextID, cmsUInt32Number errorcode, const char *text)
{
	g_warning ("LCMS error %i: %s", errorcode, text);
}

/**
 * gcm_glsl_generate_cogl_color_data:
 **/
static CoglHandle
gcm_glsl_generate_cogl_color_data (const gchar *filename, GError **error)
{
	CoglHandle tex = NULL;
	cmsHPROFILE device_profile;
	cmsHPROFILE srgb_profile;
	cmsUInt8Number *data;
	cmsHTRANSFORM transform;
	guint array_size;
	guint r, g, b;
	guint8 *p;

	srgb_profile = cmsCreate_sRGBProfile ();
	device_profile = cmsOpenProfileFromFile (filename, "r");

	/* create a cube and cut itup into parts */
	array_size = GCM_GLSL_LOOKUP_SIZE * GCM_GLSL_LOOKUP_SIZE * GCM_GLSL_LOOKUP_SIZE;
	data = g_new0 (cmsUInt8Number, 3 * array_size);
	transform = cmsCreateTransform (srgb_profile, TYPE_RGB_8, device_profile, TYPE_RGB_8, INTENT_PERCEPTUAL, 0);

	/* we failed */
	if (transform == NULL) {
		g_set_error (error, 1, 0, "could not create transform");
		goto out;
	}

	/* create mapping (blue->r, green->t, red->s) */
	for (p = data, b = 0; b < GCM_GLSL_LOOKUP_SIZE; b++)  {
		for (g = 0; g < GCM_GLSL_LOOKUP_SIZE; g++) {
			for (r = 0; r < GCM_GLSL_LOOKUP_SIZE; r++)  {
				*(p++) = (r * 255) / (GCM_GLSL_LOOKUP_SIZE - 1);
				*(p++) = (g * 255) / (GCM_GLSL_LOOKUP_SIZE - 1);
				*(p++) = (b * 255) / (GCM_GLSL_LOOKUP_SIZE - 1);
			}
		}
	}

	cmsDoTransform (transform, data, data, array_size);

	/* creates a cogl texture from the data */
	tex = cogl_texture_3d_new_from_data (GCM_GLSL_LOOKUP_SIZE, /* width */
					     GCM_GLSL_LOOKUP_SIZE, /* height */
					     GCM_GLSL_LOOKUP_SIZE, /* depth */
					     COGL_TEXTURE_NO_AUTO_MIPMAP,
					     COGL_PIXEL_FORMAT_RGB_888,
					     COGL_PIXEL_FORMAT_ANY,
					     /* data is tightly packed so we can pass zero */
					     0, 0,
					     data, error);
out:
	cmsCloseProfile (device_profile);
	cmsCloseProfile (srgb_profile);
	if (transform != NULL)
		cmsDeleteTransform (transform);
	g_free (data);
	return tex;
}

static void
paint_override (ClutterActor *actor, gpointer user_data)
{
	g_debug ("Paint");
}

#define GCM_FSCM_LAYER			2

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	ClutterActor *stage, *actor;
	GError *error = NULL;
	CoglHandle material;
	CoglHandle color_data;
	ClutterShader *shader;
	ClutterInitError init_error;
	ClutterColor stage_color = { 0x00, 0x00, 0x00, 0xff };
	ClutterColor actor_color = { 0xff, 0xff, 0xff, 0x99 };

	init_error = clutter_init (&argc, &argv);
	if (init_error != CLUTTER_INIT_SUCCESS) {
		g_warning ("failed to startup clutter: %i", init_error);
		return 0;
	}
	cmsSetLogErrorHandler (gcm_glsl_error_cb);

	/* create a regular texture containing the image */
	actor = clutter_texture_new_from_file ("./image-widget-good.png", &error);
	if (actor == NULL) {
		g_warning ("Failed to load texture: %s", error->message);
		g_clear_error (&error);
		return 0;
	}

	/* modify this material so we can add the 3D color data */
	material = clutter_texture_get_cogl_material (CLUTTER_TEXTURE (actor));

	color_data = gcm_glsl_generate_cogl_color_data ("/usr/share/color/icc/FakeBRG.icc",
							&error);
	if (color_data == COGL_INVALID_HANDLE) {
		g_warning ("Error creating lookup texture: %s", error->message);
		g_clear_error (&error);
	} else if (0) {
		/* add the texture into the second layer of the material */
		cogl_material_set_layer (material, GCM_FSCM_LAYER, color_data);

		/* we want to use linear interpolation for the texture */
		cogl_material_set_layer_filters (material, GCM_FSCM_LAYER,
						 COGL_MATERIAL_FILTER_LINEAR,
						 COGL_MATERIAL_FILTER_LINEAR);

		/* clamp to the maximum values */
		cogl_material_set_layer_wrap_mode (material, GCM_FSCM_LAYER,
						   COGL_MATERIAL_WRAP_MODE_CLAMP_TO_EDGE);

		cogl_handle_unref (color_data);
	}

	/* we can set a shader on the actor using the Clutter shader API */
	shader = clutter_shader_new ();
	clutter_shader_set_fragment_source (shader, fragment_shader_source, -1);
	if (clutter_shader_compile (shader, &error)) {
		/* tell the uniforms which units these are in */
		if(0)clutter_actor_set_shader (actor, shader);
		clutter_actor_set_shader_param_int (actor, "main_texture", 0);
		clutter_actor_set_shader_param_int (actor, "color_data", 1);
	} else {
		g_warning ("Failed to compile shader: %s", error->message);
		g_clear_error (&error);
	}
	g_object_unref (shader);

	/* setup stage */
	stage = clutter_stage_get_default ();
	clutter_actor_set_size (stage, 300, 370);
	clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
	g_signal_connect (stage, "paint",
			  G_CALLBACK (paint_override), NULL);

	/* we can just drop the texture on the stage and let it paint normally */
	clutter_container_add_actor (CLUTTER_CONTAINER (stage), actor);

{
ClutterEffect *effect;
effect = clutter_blur_effect_new ();
if(1)clutter_actor_add_effect (stage, effect);
}

	/* title */
	ClutterActor *label = clutter_text_new_full ("Sans 12", "GLSL Shader Demo", &actor_color);
	clutter_actor_set_size (label, 500, 500);
	clutter_actor_set_position (label, 20, 320);
	clutter_container_add_actor (CLUTTER_CONTAINER (stage), label);
	clutter_actor_show (label);

	clutter_actor_show (stage);

	clutter_main ();

	return 0;
}