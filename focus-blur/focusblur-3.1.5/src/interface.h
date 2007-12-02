/* Focus Blur -- blur with focus plug-in.
 * Copyright (C) 2002-2007 Kyoichiro Suda
 *
 * The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __FOCUSBLUR_INTERFACE_H__
#define __FOCUSBLUR_INTERFACE_H__

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "focusblurtypes.h"

G_BEGIN_DECLS


gboolean        focusblur_execute               (FblurParam     *param,
                                                 GimpPreview    *preview);
#ifdef ENABLE_MP
void            focusblur_execute_thread        (gpointer        data,
                                                 gpointer        user_data);
#endif
gboolean        focusblur_dialog                (FblurParam     *param);
void            focusblur_widget_set_enable_depth_map   (FblurParam     *param,
                                                         gboolean        bool);
void            focusblur_widget_set_enable_shine       (FblurParam     *param,
                                                         gboolean        bool);


G_END_DECLS

#endif /* __FOCUSBLUR_INTERFACE_H__ */
