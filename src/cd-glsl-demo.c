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
#include <clutter/clutter.h>

#include "cd-icc-effect.h"

/**
 * main:
 **/
int
main (int argc, char **argv)
{
  ClutterActor *stage, *actor;
  ClutterColor actor_color = { 0xff, 0xff, 0xff, 0x99 };
  ClutterColor stage_color = { 0x00, 0x00, 0x00, 0xff };
  ClutterEffect *effect;
  ClutterInitError init_error;
  GError *error = NULL;

  init_error = clutter_init (&argc, &argv);
  if (init_error != CLUTTER_INIT_SUCCESS) {
    g_warning ("failed to startup clutter: %i", init_error);
    return 0;
  }

  /* create a regular texture containing the image */
  actor = clutter_texture_new_from_file ("./image-widget-good.png", &error);
  if (actor == NULL)
    {
      g_warning ("Failed to load texture: %s", error->message);
      g_clear_error (&error);
      return 0;
    }

  /* setup stage */
  stage = clutter_stage_get_default ();
  clutter_actor_set_size (stage, 300, 370);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

  /* we can just drop the texture on the stage and let it paint normally */
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), actor);

  /* do the ICC transform */
  effect = cd_icc_effect_new ();
  clutter_actor_add_effect (stage, effect);

  /* do the not-quite Guassian blur */
  effect = clutter_blur_effect_new ();
  clutter_actor_add_effect (stage, effect);

  /* title */
  ClutterActor *label = clutter_text_new_full ("Sans 12",
                                               "GLSL Shader Demo",
                                               &actor_color);
  clutter_actor_set_size (label, 500, 500);
  clutter_actor_set_position (label, 20, 320);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), label);
  clutter_actor_show (label);

  clutter_actor_show (stage);

  clutter_main ();

  return 0;
}
