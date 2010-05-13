/* vim:set et sts=4: */
/* input-pad - The input pad
 * Copyright (C) 2010 Takao Fujiwara <takao.fujiwara1@gmail.com>
 * Copyright (C) 2010 Red Hat, Inc.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __INPUT_PAD_BUTTON_GTK_H__
#define __INPUT_PAD_BUTTON_GTK_H__

#include <gtk/gtk.h>

#include "input-pad-group.h"

G_BEGIN_DECLS

#define INPUT_PAD_TYPE_GTK_BUTTON               (input_pad_gtk_button_get_type ())
#define INPUT_PAD_GTK_BUTTON(o)                 (G_TYPE_CHECK_INSTANCE_CAST ((o), INPUT_PAD_TYPE_GTK_BUTTON, InputPadGtkButton))
#define INPUT_PAD_GTK_BUTTON_CLASS(k)           (G_TYPE_CHECK_CLASS_CAST ((k), INPUT_PAD_TYPE_GTK_BUTTON, InputPadGtkButtonClass))
#define INPUT_PAD_IS_GTK_BUTTON(o)              (G_TYPE_CHECK_INSTANCE_TYPE ((o), INPUT_PAD_TYPE_GTK_BUTTON))
#define INPUT_PAD_IS_GTK_BUTTON_CLASS(k)        (G_TYPE_CHECK_CLASS_TYPE ((k), INPUT_PAD_TYPE_GTK_BUTTON))
#define INPUT_PAD_GTK_BUTTON_GET_CLASS(l)       (G_TYPE_INSTANCE_GET_CLASS ((o), INPUT_PAD_TYPE_GTK_BUTTON, InputPadGtkButtonClass))

typedef struct _InputPadGtkButtonPrivate InputPadGtkButtonPrivate;
typedef struct _InputPadGtkButton InputPadGtkButton;
typedef struct _InputPadGtkButtonClass InputPadGtkButtonClass;

struct _InputPadGtkButton {
    GtkButton                           parent;

    InputPadGtkButtonPrivate           *priv;
};

struct _InputPadGtkButtonClass {
    GtkButtonClass              parent_class;

    /* Padding for future expansion */
    void (*_button_reserved1) (void);
    void (*_button_reserved2) (void);
    void (*_button_reserved3) (void);
    void (*_button_reserved4) (void);
};

GType               input_pad_gtk_button_get_type (void);
GtkWidget *         input_pad_gtk_button_new_with_label (const gchar *label);
guint               input_pad_gtk_button_get_keysym
                                       (InputPadGtkButton *button);
void                input_pad_gtk_button_set_keysym
                                       (InputPadGtkButton *button,
                                        guint keysym);
InputPadTableType   input_pad_gtk_button_get_table_type
                                       (InputPadGtkButton *button);
void                input_pad_gtk_button_set_table_type
                                       (InputPadGtkButton *button,
                                        InputPadTableType type);

G_END_DECLS

#endif