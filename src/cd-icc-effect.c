/*
 * Clutter.
 *
 * Copyright (C) 2010  Intel Corporation.
 * Copyright (C) 2011  Richard Hughes <richard@hughsie.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 *   Richard Hughes <richard@hughsie.com>
 */

#define CD_ICC_EFFECT_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CD_TYPE_ICC_EFFECT, CdIccEffectClass))
#define CD_IS_ICC_EFFECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CD_TYPE_ICC_EFFECT))
#define CD_ICC_EFFECT_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CD_TYPE_ICC_EFFECT, CdIccEffectClass))

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define COGL_ENABLE_EXPERIMENTAL_API

#include "cd-icc-effect.h"

#include <cogl/cogl.h>
#include <lcms2.h>

/* the texture will be 16x16x16 and 3bpp */
#define GCM_GLSL_LOOKUP_SIZE  16

static const gchar *glsl_shader =
"uniform sampler2D main_texture;\n"
"uniform sampler3D color_data1;\n"
"uniform sampler3D color_data2;\n"
"uniform sampler2D indirect_texture;\n"
"\n"
"void\n"
"main ()\n"
"{\n"
"  vec3 tex_color = texture2D (main_texture, gl_TexCoord[0].st).rgb;\n"
"  float idx = texture2D (indirect_texture, gl_TexCoord[0].st).a;\n"
"  if (idx > 0.6)\n"
"    gl_FragColor = texture3D (color_data1, tex_color);\n"
"  else if (idx > 0.1)\n"
"    gl_FragColor = texture3D (color_data2, tex_color);\n"
"  else\n"
"    gl_FragColor.rgb = tex_color;\n"
"}";

struct _CdIccEffect
{
  ClutterOffscreenEffect parent_instance;

  /* a back pointer to our actor, so that we can query it */
  ClutterActor *actor;

  CoglHandle shader;
  CoglHandle program;

  gint main_texture_uniform;
  gint indirect_texture_uniform;
  gint color_data1_uniform;
  gint color_data2_uniform;

  guint is_compiled : 1;
};

struct _CdIccEffectClass
{
  ClutterOffscreenEffectClass parent_class;
};

G_DEFINE_TYPE (CdIccEffect,
               cd_icc_effect,
               CLUTTER_TYPE_OFFSCREEN_EFFECT);

static gboolean
cd_icc_effect_pre_paint (ClutterEffect *effect)
{
  CdIccEffect *self = CD_ICC_EFFECT (effect);
  ClutterEffectClass *parent_class;
  ClutterActorBox allocation;
  gfloat width, height;

  if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (effect)))
    return FALSE;

  self->actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (effect));
  if (self->actor == NULL)
    return FALSE;

  if (!clutter_feature_available (CLUTTER_FEATURE_SHADERS_GLSL))
    {
      /* if we don't have support for GLSL shaders then we
       * forcibly disable the ActorMeta
       */
      g_warning ("Unable to use the ShaderEffect: the graphics hardware "
                 "or the current GL driver does not implement support "
                 "for the GLSL shading language.");
      clutter_actor_meta_set_enabled (CLUTTER_ACTOR_META (effect), FALSE);
      return FALSE;
    }

  clutter_actor_get_allocation_box (self->actor, &allocation);
  clutter_actor_box_get_size (&allocation, &width, &height);

  if (self->shader == COGL_INVALID_HANDLE)
    {
      self->shader = cogl_create_shader (COGL_SHADER_TYPE_FRAGMENT);
      cogl_shader_source (self->shader, glsl_shader);

      self->is_compiled = FALSE;
      self->main_texture_uniform = -1;
      self->indirect_texture_uniform = -1;
      self->color_data1_uniform = -1;
      self->color_data2_uniform = -1;
    }

  if (self->program == COGL_INVALID_HANDLE)
    self->program = cogl_create_program ();

  if (!self->is_compiled)
    {
      g_assert (self->shader != COGL_INVALID_HANDLE);
      g_assert (self->program != COGL_INVALID_HANDLE);

      cogl_shader_compile (self->shader);
      if (!cogl_shader_is_compiled (self->shader))
        {
          gchar *log_buf = cogl_shader_get_info_log (self->shader);

          g_warning (G_STRLOC ": Unable to compile the icc shader: %s",
                     log_buf);
          g_free (log_buf);

          cogl_handle_unref (self->shader);
          cogl_handle_unref (self->program);

          self->shader = COGL_INVALID_HANDLE;
          self->program = COGL_INVALID_HANDLE;
        }
      else
        {
          cogl_program_attach_shader (self->program, self->shader);
          cogl_program_link (self->program);

          cogl_handle_unref (self->shader);

          self->is_compiled = TRUE;

          self->main_texture_uniform =
            cogl_program_get_uniform_location (self->program, "main_texture");
          self->indirect_texture_uniform =
            cogl_program_get_uniform_location (self->program, "indirect_texture");
          self->color_data1_uniform =
            cogl_program_get_uniform_location (self->program, "color_data1");
          self->color_data2_uniform =
            cogl_program_get_uniform_location (self->program, "color_data2");
        }
    }

  parent_class = CLUTTER_EFFECT_CLASS (cd_icc_effect_parent_class);
  return parent_class->pre_paint (effect);
}

/**
 * cd_icc_effect_error_cb:
 **/
static void
cd_icc_effect_error_cb (cmsContext ContextID, cmsUInt32Number errorcode, const char *text)
{
  g_warning ("LCMS error %i: %s", errorcode, text);
}


/**
 * cd_icc_effect_generate_cogl_color_data:
 **/
static CoglHandle
cd_icc_effect_generate_cogl_color_data (const gchar *filename, GError **error)
{
  CoglHandle tex = NULL;
  cmsHPROFILE device_profile;
  cmsHPROFILE srgb_profile;
  cmsUInt8Number *data;
  cmsHTRANSFORM transform;
  guint array_size;
  guint r, g, b;
  guint8 *p;

  cmsSetLogErrorHandler (cd_icc_effect_error_cb);

  srgb_profile = cmsCreate_sRGBProfile ();
  device_profile = cmsOpenProfileFromFile (filename, "r");

  /* create a cube and cut itup into parts */
  array_size = GCM_GLSL_LOOKUP_SIZE * GCM_GLSL_LOOKUP_SIZE * GCM_GLSL_LOOKUP_SIZE;
  data = g_new0 (cmsUInt8Number, 3 * array_size);
  transform = cmsCreateTransform (srgb_profile, TYPE_RGB_8, device_profile, TYPE_RGB_8, INTENT_PERCEPTUAL, 0);

  /* we failed */
  if (transform == NULL)
    {
      g_set_error (error, 1, 0, "could not create transform");
      goto out;
    }

  /* create mapping (blue->r, green->t, red->s) */
  for (p = data, b = 0; b < GCM_GLSL_LOOKUP_SIZE; b++) {
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

/**
 * cd_icc_effect_generate_indirect_data:
 **/
static CoglHandle
cd_icc_effect_generate_indirect_data (CdIccEffect *self, GError **error)
{
  CoglHandle tex = NULL;
  guint width;
  guint height;
  guint x, y;
  guint8 *data;
  guint8 *p;

  /* create a redirection array */
  width = clutter_actor_get_width (self->actor);
  height = clutter_actor_get_height (self->actor);
  data = g_new0 (guint8, width * height);

  for (y=0; y<height; y++)
    for (x=0; x<width; x++)
      {
        p = data + (x + (y * width));
        if (x > 150)
          *(p+0) = 255;
        else if (x < 120)
          *(p+0) = 128;
      }

  /* creates a cogl texture from the data */
  tex = cogl_texture_new_from_data (width,
                                    height,
                                    COGL_TEXTURE_NO_AUTO_MIPMAP,
                                    COGL_PIXEL_FORMAT_A_8,
                                    COGL_PIXEL_FORMAT_ANY,
                                    /* data is tightly packed so we can pass zero */
                                    0,
                                    data);
  g_free (data);
  return tex;
}

#define GCM_FSCM_LAYER_COLOR1      1
#define GCM_FSCM_LAYER_COLOR2      2
#define GCM_FSCM_LAYER_INDIRECT    3

static void
cd_icc_effect_paint_target (ClutterOffscreenEffect *effect)
{
  CdIccEffect *self = CD_ICC_EFFECT (effect);
  ClutterOffscreenEffectClass *parent;
  CoglHandle material;
  CoglHandle color_data;
  CoglHandle indirect_texture;
  GError *error = NULL;

  if (self->program == COGL_INVALID_HANDLE)
    goto out;

  if (self->main_texture_uniform > -1)
    cogl_program_set_uniform_1i (self->program,
                                 self->main_texture_uniform,
                                 0);

  if (self->color_data1_uniform > -1)
    cogl_program_set_uniform_1i (self->program,
                                 self->color_data1_uniform,
                                 1); /* not the layer number, just co-incidence */
  if (self->color_data2_uniform > -1)
    cogl_program_set_uniform_1i (self->program,
                                 self->color_data2_uniform,
                                 2);

  if (self->indirect_texture_uniform > -1)
    cogl_program_set_uniform_1i (self->program,
                                 self->indirect_texture_uniform,
                                 3);

  material = clutter_offscreen_effect_get_target (effect);




  /* get the color textures */
  color_data = cd_icc_effect_generate_cogl_color_data ("/usr/share/color/icc/FakeBRG.icc",
                                                       &error);
  if (color_data == COGL_INVALID_HANDLE)
    {
      g_warning ("Error creating lookup texture: %s", error->message);
      g_error_free (error);
      goto out;
    }

  /* add the texture into the second layer of the material */
  cogl_material_set_layer (material, GCM_FSCM_LAYER_COLOR1, color_data);

  /* we want to use linear interpolation for the texture */
  cogl_material_set_layer_filters (material, GCM_FSCM_LAYER_COLOR1,
                                   COGL_MATERIAL_FILTER_LINEAR,
                                   COGL_MATERIAL_FILTER_LINEAR);

  /* clamp to the maximum values */
  cogl_material_set_layer_wrap_mode (material, GCM_FSCM_LAYER_COLOR1,
                                     COGL_MATERIAL_WRAP_MODE_CLAMP_TO_EDGE);

  cogl_handle_unref (color_data);





  /* get the color textures */
  color_data = cd_icc_effect_generate_cogl_color_data ("/usr/share/color/icc/FakeRBG.icc",
                                                        &error);
  if (color_data == COGL_INVALID_HANDLE)
    {
      g_warning ("Error creating lookup texture: %s", error->message);
      g_error_free (error);
      goto out;
    }

  /* add the texture into the second layer of the material */
  cogl_material_set_layer (material, GCM_FSCM_LAYER_COLOR2, color_data);

  /* we want to use linear interpolation for the texture */
  cogl_material_set_layer_filters (material, GCM_FSCM_LAYER_COLOR2,
                                   COGL_MATERIAL_FILTER_LINEAR,
                                   COGL_MATERIAL_FILTER_LINEAR);

  /* clamp to the maximum values */
  cogl_material_set_layer_wrap_mode (material, GCM_FSCM_LAYER_COLOR2,
                                     COGL_MATERIAL_WRAP_MODE_CLAMP_TO_EDGE);

  cogl_handle_unref (color_data);





  /* get the indirect texture */
  indirect_texture = cd_icc_effect_generate_indirect_data (self, &error);
  if (indirect_texture == COGL_INVALID_HANDLE)
    {
      g_warning ("Error creating indirect texture: %s", error->message);
      g_error_free (error);
      goto out;
    }

  /* add the texture into the second layer of the material */
  cogl_material_set_layer (material, GCM_FSCM_LAYER_INDIRECT, indirect_texture);

  /* we want to use linear interpolation for the texture */
  cogl_material_set_layer_filters (material, GCM_FSCM_LAYER_INDIRECT,
                                   COGL_MATERIAL_FILTER_LINEAR,
                                   COGL_MATERIAL_FILTER_LINEAR);

  /* clamp to the maximum values */
  cogl_material_set_layer_wrap_mode (material, GCM_FSCM_LAYER_INDIRECT,
                                     COGL_MATERIAL_WRAP_MODE_CLAMP_TO_EDGE);

  cogl_handle_unref (indirect_texture);

  cogl_material_set_user_program (material, self->program);

out:
  parent = CLUTTER_OFFSCREEN_EFFECT_CLASS (cd_icc_effect_parent_class);
  parent->paint_target (effect);
}

static void
cd_icc_effect_dispose (GObject *gobject)
{
  CdIccEffect *self = CD_ICC_EFFECT (gobject);

  if (self->program != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (self->program);

      self->program = COGL_INVALID_HANDLE;
      self->shader = COGL_INVALID_HANDLE;
    }

  G_OBJECT_CLASS (cd_icc_effect_parent_class)->dispose (gobject);
}

static void
cd_icc_effect_class_init (CdIccEffectClass *klass)
{
  ClutterEffectClass *effect_class = CLUTTER_EFFECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterOffscreenEffectClass *offscreen_class;

  gobject_class->dispose = cd_icc_effect_dispose;

  effect_class->pre_paint = cd_icc_effect_pre_paint;

  offscreen_class = CLUTTER_OFFSCREEN_EFFECT_CLASS (klass);
  offscreen_class->paint_target = cd_icc_effect_paint_target;
}

static void
cd_icc_effect_init (CdIccEffect *self)
{
}

/**
 * cd_icc_effect_new:
 */
ClutterEffect *
cd_icc_effect_new (void)
{
  return g_object_new (CD_TYPE_ICC_EFFECT, NULL);
}
