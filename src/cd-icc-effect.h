/*
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

#ifndef __CD_ICC_EFFECT_H__
#define __CD_ICC_EFFECT_H__

#include <clutter/clutter.h>

G_BEGIN_DECLS

#define CD_TYPE_ICC_EFFECT        (cd_icc_effect_get_type ())
#define CD_ICC_EFFECT(obj)        (G_TYPE_CHECK_INSTANCE_CAST ((obj), CD_TYPE_ICC_EFFECT, CdIccEffect))
#define CD_IS_ICC_EFFECT(obj)     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CD_TYPE_ICC_EFFECT))

typedef struct _CdIccEffect       CdIccEffect;
typedef struct _CdIccEffectClass  CdIccEffectClass;

GType cd_icc_effect_get_type (void) G_GNUC_CONST;

ClutterEffect *cd_icc_effect_new (void);

G_END_DECLS

#endif /* __CD_ICC_EFFECT_H__ */
