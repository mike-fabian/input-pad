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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <string.h> /* strlen */

#include "i18n.h"
#include "button-gtk.h"
#include "input-pad.h"
#include "input-pad-group.h"
#include "input-pad-window-gtk.h"
#include "input-pad-private.h"
#include "input-pad-marshal.h"
#include "keysym-str2val.h"

#define INPUT_PAD_UI_FILE INPUT_PAD_UI_GTK_DIR "/input-pad.ui"

#define INPUT_PAD_GTK_WINDOW_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), INPUT_PAD_TYPE_GTK_WINDOW, InputPadGtkWindowPrivate))

enum {
    BUTTON_PRESSED,
    LAST_SIGNAL
};

struct _InputPadGtkWindowPrivate {
    InputPadGroup              *group;
    guint                       show_all : 1;
    GModule                    *module_gdk_xtest;
};

static guint                    signals[LAST_SIGNAL] = { 0 };
static gboolean                 use_module_xtest = FALSE;
#if 0
static GtkBuildableIface       *parent_buildable_iface;
#endif

static GOptionEntry entries[] = {
#ifdef MODULE_XTEST_GDK_BASE
  { "enable-module-xtest", 'x', 0, G_OPTION_ARG_NONE, &use_module_xtest,
    N_("Use XTEST module to send key events"), NULL},
#endif
  { NULL }
};

static InputPadGroup *get_nth_pad_group (InputPadGroup *group, int nth);
static InputPadTable *get_nth_pad_table (InputPadTable *table, int nth);
static void create_char_table (GtkWidget *vbox, InputPadTable *table_data);
static void input_pad_gtk_window_real_destroy (GtkObject *object);
static void input_pad_gtk_window_buildable_interface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (InputPadGtkWindow, input_pad_gtk_window,
                         GTK_TYPE_WINDOW,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                input_pad_gtk_window_buildable_interface_init))

static void
on_window_close (InputPadGtkWindow *window, gpointer data)
{
    g_return_if_fail (window != NULL &&
                      INPUT_PAD_IS_GTK_WINDOW (window));

    if (window->child) {
        gtk_widget_hide (GTK_WIDGET (window));
    } else {
        gtk_main_quit ();
    }
}

static void
on_about_activate (GtkAction *action, gpointer data)
{
    GtkWidget *about_dlg = GTK_WIDGET (data);

    gtk_dialog_run (GTK_DIALOG (about_dlg));
    gtk_widget_hide (about_dlg);
}

static void
on_contents_activate (GtkAction *action, gpointer data)
{
    GtkWidget *contents_dlg = GTK_WIDGET (data);

    gtk_dialog_run (GTK_DIALOG (contents_dlg));
    gtk_widget_hide (contents_dlg);
}

static void
on_button_ok_clicked (GtkButton *button, gpointer data)
{
    GtkWidget *contents_dlg = GTK_WIDGET (data);

    gtk_widget_hide (contents_dlg);
}

static void
on_close_activate (GtkAction *quit, gpointer data)
{
    g_return_if_fail (data != NULL &&
                      INPUT_PAD_IS_GTK_WINDOW (data));

    on_window_close (INPUT_PAD_GTK_WINDOW (data), NULL);
}

static void
on_main_switch_page (GtkNotebook       *notebook,
                     GtkNotebookPage   *page,
                     guint              page_num,
                     gpointer           data)
{
    GtkWidget *sub_notebook;
    gint sub_page;
    GtkWidget *vbox;
    InputPadGroup *group = (InputPadGroup *) data;
    InputPadTable *table;

    sub_notebook = gtk_notebook_get_nth_page (notebook, (int) page_num);
    sub_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (sub_notebook));
    vbox = gtk_notebook_get_nth_page (GTK_NOTEBOOK (sub_notebook), sub_page);
    group = get_nth_pad_group (group, (int) page_num);
    table = get_nth_pad_table (group->table, sub_page);

    g_return_if_fail (table != NULL);
    create_char_table (vbox, table);
}

static void
on_sub_switch_page (GtkNotebook        *notebook,
                    GtkNotebookPage    *page,
                    guint               page_num,
                    gpointer            data)
{
    GtkWidget *vbox;
    InputPadTable *table = (InputPadTable *) data;

    vbox = gtk_notebook_get_nth_page (notebook, (int) page_num);
    table = get_nth_pad_table (table, (int) page_num);

    g_return_if_fail (table != NULL);
    create_char_table (vbox, table);
}

/*
 * % xterm -xrm "XTerm*allowSendEvents: true"
 */
static int
send_key_event (GdkWindow      *gdkwindow,
                guint           keysym,
                guint           keycode,
                guint           state)
{
    XEvent xevent;
    Window input_focus;
    int revert;

    XGetInputFocus (GDK_WINDOW_XDISPLAY (gdkwindow), &input_focus, &revert);
    if (input_focus == (Window) 0) {
        g_warning ("WARNING: Could not get a focused window.");
        return FALSE;
    }

    xevent.type = KeyPress;
    xevent.xkey.type = KeyPress;
    xevent.xkey.serial = 0L;
    xevent.xkey.send_event = True;
    xevent.xkey.display = GDK_WINDOW_XDISPLAY (gdkwindow);
    xevent.xkey.root = XDefaultRootWindow (xevent.xkey.display);
    xevent.xkey.window = input_focus;
    xevent.xkey.subwindow = xevent.xkey.window;
    xevent.xkey.time = CurrentTime;
    xevent.xkey.state = state;
    if (keycode) {
        xevent.xkey.keycode = keycode;
    } else {
        xevent.xkey.keycode = XKeysymToKeycode (xevent.xkey.display,
                                                (KeySym) keysym);
    }
    xevent.xkey.same_screen = True;
    XSendEvent (xevent.xkey.display, xevent.xkey.window, True,
                KeyPressMask, &xevent);

    XSync (xevent.xkey.display, False);
    xevent.type = KeyRelease;
    xevent.xkey.type = KeyRelease;
    XSendEvent (xevent.xkey.display, xevent.xkey.window, True,
                KeyReleaseMask, &xevent);
    XSync (xevent.xkey.display, False);

    return TRUE;
}

/* TODO: use Xklavior */
static guint
get_state (const char *str, guint keysym)
{
    guint state = 0;

    if (!str || !*str || keysym > 0x7f) {
        return 0;
    }
    if (*str >= 'A' && *str <= 'Z') {
        return ShiftMask;
    }
    switch (*str) {
    case '!':
    case '\"':
    case '#':
    case '$':
    case '%':
    case '&':
    case '\'':
    case '(':
    case ')':
    case '~':
    case '=':
    case '|':
    case '{':
    case '}':
    case '`':
    case '+':
    case '*':
    case '?':
    case '_':
        state = ShiftMask;
        break;
    default:;
    }
    return state;
}

static void
on_button_pressed (GtkButton *button, gpointer data)
{
    InputPadGtkWindow *window;
    InputPadTableType  type;
    const char *str = gtk_button_get_label (button);
    guint keysym;
    guint state = 0;
    gboolean retval = FALSE;

    g_return_if_fail (INPUT_PAD_IS_GTK_BUTTON (button));
    g_return_if_fail (data != NULL &&
                      INPUT_PAD_IS_GTK_WINDOW (data));

    window = INPUT_PAD_GTK_WINDOW (data);
    type = input_pad_gtk_button_get_table_type (INPUT_PAD_GTK_BUTTON (button));
    keysym = input_pad_gtk_button_get_keysym (INPUT_PAD_GTK_BUTTON (button));
    state = get_state (str, keysym);

    g_signal_emit (window, signals[BUTTON_PRESSED], 0,
                   str, type, keysym, 0, state, &retval);
}

static InputPadGroup *
get_nth_pad_group (InputPadGroup *group, int nth)
{
    InputPadGroup *retval = group;
    int i;
    for (i = 0; i < nth; i++) {
        retval = retval->next;
    }
    return retval;
}

static InputPadTable *
get_nth_pad_table (InputPadTable *table, int nth)
{
    InputPadTable *retval = table;
    int i;
    for (i = 0; i < nth; i++) {
        retval = retval->next;
    }
    return retval;
}

static guint
keysym_get_value_from_string (const gchar *str)
{
    int i;
    guint retval = 0x0;

    for (i = 0; input_pad_keysym_table[i].str;  i++) {
        if (!g_strcmp0 (input_pad_keysym_table[i].str, str)) {
            retval = input_pad_keysym_table[i].keysym;
            break;
        }
    }
    if (retval == 0x0) {
        g_warning ("keysym str %s does not have the value.", str);
    }
    return retval;
}

static void
create_char_table (GtkWidget *vbox, InputPadTable *table_data)
{
    GtkWidget *scrolled;
    GtkWidget *viewport;
    GtkWidget *table;
    GtkWidget *button = NULL;
    gchar **char_table;
    gchar *str;
    gchar buff[6];
    const int TABLE_COLUMN = table_data->column;
    int i, num, raw, col, len, code;
    guint keysym;

    if (table_data->priv->inited) {
        return;
    }

    scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_NEVER,
                                    GTK_POLICY_ALWAYS);
    gtk_box_pack_start (GTK_BOX (vbox), scrolled, TRUE, TRUE, 0);
    gtk_widget_show (scrolled);

    viewport = gtk_viewport_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (scrolled), viewport);
    gtk_widget_show (viewport);

    if (table_data->type == INPUT_PAD_TABLE_TYPE_CHARS) {
        char_table = g_strsplit_set (table_data->data.chars, " \t\n", -1);
    } else if (table_data->type == INPUT_PAD_TABLE_TYPE_KEYSYMS) {
        char_table = g_strsplit_set (table_data->data.keysyms, " \t\n", -1);
    } else {
        g_warning ("Currently your table type is not supported.");
        table_data->priv->inited = 1;
        return;
    }
    for (i = 0, num = 0; char_table[i]; i++) {
        str = char_table[i];
        len = strlen (str);
        if (len > 0) {
            num++;
        }
    }
    col = TABLE_COLUMN;
    raw = num / col;
    if (num % col) {
        raw++;
    }

    table = gtk_table_new (raw, col, TRUE);
    gtk_table_set_row_spacings (GTK_TABLE (table), 0);
    gtk_table_set_col_spacings (GTK_TABLE (table), 0);
    gtk_container_add (GTK_CONTAINER (viewport), table);
    gtk_widget_show (table);

    for (i = 0, num = 0; char_table[i]; i++) {
        str = char_table[i];
        len = strlen (str);
        if (len > 0) {
            if (table_data->type == INPUT_PAD_TABLE_TYPE_CHARS) {
                if (str[0] == '0' && 
                    (str[1] == 'x' || str[1] == 'X')) {
                    str += 2;
                    len -= 2;
                }
                code = (int) g_ascii_strtoll (str, NULL, 16);
                buff[g_unichar_to_utf8 ((gunichar) code, buff)] = '\0';
                button = input_pad_gtk_button_new_with_label (buff);
                if (code <= 0x7f) {
                    input_pad_gtk_button_set_keysym (INPUT_PAD_GTK_BUTTON (button),
                                                     code);
                }
            } else if (table_data->type == INPUT_PAD_TABLE_TYPE_KEYSYMS) {
                button = input_pad_gtk_button_new_with_label (str);
                keysym = keysym_get_value_from_string (str);
                input_pad_gtk_button_set_keysym (INPUT_PAD_GTK_BUTTON (button),
                                                 keysym);
            }
            input_pad_gtk_button_set_table_type (INPUT_PAD_GTK_BUTTON (button),
                                                 table_data->type);
            raw = num / TABLE_COLUMN;
            col = num % TABLE_COLUMN;
            gtk_table_attach_defaults (GTK_TABLE (table), button,
                                       col, col + 1, raw, raw + 1);
            gtk_widget_show (button);
            g_signal_connect (G_OBJECT (button), "pressed",
                              G_CALLBACK (on_button_pressed),
                              table_data->priv->signal_window);
            num++;
        }
    }
    g_strfreev (char_table);

    table_data->priv->inited = 1;
}

static void
set_about (GtkWidget *widget)
{
    GtkAboutDialog *about_dlg = GTK_ABOUT_DIALOG (widget);
    gchar *license1, *license2, *license3, *license;
    license1 = _(""
"This program is free software; you can redistribute it and/or modify "
"it under the terms of the GNU General Public License as published by "
"the Free Software Foundation; either version 2 of the License, or "
"(at your option) any later version.");

    license2 = _(""
"This program is distributed in the hope that it will be useful, "
"but WITHOUT ANY WARRANTY; without even the implied warranty of "
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
"GNU General Public License for more details.");

    license3 = _(""
"You should have received a copy of the GNU General Public License along "
"with this program; if not, write to the Free Software Foundation, Inc., "
"51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.");

    license = g_strdup_printf ("%s\n\n%s\n\n%s",
                               license1, license2, license3);
    gtk_about_dialog_set_license (about_dlg, license);
    gtk_about_dialog_set_version (about_dlg, VERSION);
    g_free (license);
}

static GtkWidget *
sub_notebook_new_with_data (InputPadTable *table_data,
                            GtkWidget         *signal_window)
{
    GtkWidget *notebook;
    InputPadTable *table = table_data;
    GtkWidget *label;
    GtkWidget *vbox;

    g_return_val_if_fail (table_data != NULL, NULL);

    notebook = gtk_notebook_new ();
    while (table) {
        label = gtk_label_new (table->name);
        vbox = gtk_vbox_new (FALSE, 10);
        gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                                  vbox,
                                  label);
        gtk_widget_show (label);
        gtk_widget_show (vbox);
        table->priv->signal_window = signal_window;
        table = table->next;
    }

    return notebook;
}

static void
load_notebook_data (GtkWidget *main_notebook, 
                    InputPadGroup *group_data,
                    GtkWidget *signal_window)
{
    InputPadGroup *group = group_data;
    GtkWidget *label;
    GtkWidget *sub_notebook;

    g_return_if_fail (GTK_IS_NOTEBOOK (main_notebook));
    g_return_if_fail (group_data != NULL);

    while (group) {
        label = gtk_label_new (group->name);
        sub_notebook = sub_notebook_new_with_data (group->table, signal_window);
        gtk_notebook_append_page (GTK_NOTEBOOK (main_notebook),
                                  sub_notebook,
                                  label);
        gtk_widget_show (label);
        gtk_widget_show (sub_notebook);
        group->priv->signal_window = signal_window;
        group = group->next;
    }
}

static GtkWidget *
create_ui (void)
{
    const gchar *filename = INPUT_PAD_UI_FILE;
    GError *error = NULL;
    GtkWidget *window = NULL;
    GtkWidget *contents_dlg;
    GtkWidget *contents_ok;
    GtkWidget *about_dlg;
    GtkWidget *main_notebook;
    GtkWidget *sub_notebook;
    GtkAction *close_item;
    GtkAction *contents_item;
    GtkAction *about_item;
    GtkBuilder *builder = gtk_builder_new ();
    int i, n;
    InputPadGroup *group;

    if (!filename ||
        !g_file_test (filename, G_FILE_TEST_EXISTS)) {
        g_error ("File Not Found: %s\n", filename ? filename : "(null)");
        return NULL;
    }

    gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
    gtk_builder_add_from_file (builder, filename, &error);
    if (error) {
        g_error ("ERROR: %s\n",
                 error ? error->message ? error->message : "" : "");
        g_error_free (error);
        return NULL;
    }

    window = GTK_WIDGET (gtk_builder_get_object (builder, "TopWindow"));
    gtk_window_set_icon_from_file (GTK_WINDOW (window),
                                   DATAROOTDIR "/pixmaps/input-pad.png",
                                   &error);
    error = NULL;
    gtk_window_set_default_icon_from_file (DATAROOTDIR "/pixmaps/input-pad.png",
                                           &error);
    g_signal_connect (G_OBJECT (window), "delete_event",
                      G_CALLBACK (on_window_close), NULL);
    g_signal_connect (G_OBJECT (window), "destroy",
                      G_CALLBACK (on_window_close), NULL);

    group = INPUT_PAD_GTK_WINDOW (window)->priv->group;

    close_item = GTK_ACTION (gtk_builder_get_object (builder, "Close"));
    g_signal_connect (G_OBJECT (close_item), "activate",
                      G_CALLBACK (on_close_activate), (gpointer) window);

    about_dlg = GTK_WIDGET (gtk_builder_get_object (builder, "AboutDialog"));
    set_about (about_dlg);
    about_item = GTK_ACTION (gtk_builder_get_object (builder, "About"));
    g_signal_connect (G_OBJECT (about_item), "activate",
                      G_CALLBACK (on_about_activate), (gpointer) about_dlg);

    contents_dlg = GTK_WIDGET (gtk_builder_get_object (builder, "ContentsDialog"));
    contents_item = GTK_ACTION (gtk_builder_get_object (builder, "Contents"));
    contents_ok = GTK_WIDGET (gtk_builder_get_object (builder, "ContentsOKButton"));
    g_signal_connect (G_OBJECT (contents_item), "activate",
                      G_CALLBACK (on_contents_activate), (gpointer) contents_dlg);
    g_signal_connect (G_OBJECT (contents_ok), "clicked",
                      G_CALLBACK (on_button_ok_clicked), (gpointer) contents_dlg);

    main_notebook = GTK_WIDGET (gtk_builder_get_object (builder, "TopNotebook"));
    load_notebook_data (main_notebook, group, window);
    g_signal_connect (G_OBJECT (main_notebook), "switch-page",
                      G_CALLBACK (on_main_switch_page), group);
    n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (main_notebook));
    for (i = 0; i < n; i++) {
        sub_notebook = gtk_notebook_get_nth_page (GTK_NOTEBOOK (main_notebook),
                                                  i);
         g_signal_connect (G_OBJECT (sub_notebook), "switch-page",
                           G_CALLBACK (on_sub_switch_page), group->table);
        if (i == 0) {
            on_sub_switch_page (GTK_NOTEBOOK (sub_notebook), NULL, i, group->table);
        }
        group = group->next;
    }

    gtk_builder_connect_signals (builder, NULL);
    g_object_unref (G_OBJECT (builder));

    return window;
}

#ifdef MODULE_XTEST_GDK_BASE

static void
input_pad_gtk_window_xtest_gdk_destroy (InputPadGtkWindow *window)
{
    const gchar *error = NULL;

    g_return_if_fail (window != NULL &&
                      INPUT_PAD_IS_GTK_WINDOW (window));
    g_return_if_fail (window->priv != NULL);
    g_return_if_fail (window->priv->module_gdk_xtest != NULL);

    if (!g_module_close (window->priv->module_gdk_xtest)) {
        error = g_module_error ();
        g_warning ("Cannot close %s: %s", MODULE_XTEST_GDK_BASE,
                   error ? error : "");
        window->priv->module_gdk_xtest = NULL;
        return;
    }
    window->priv->module_gdk_xtest = NULL;
}

static void
input_pad_gtk_window_xtest_gdk_init (InputPadGtkWindow *window)
{
    gchar *filename;
    const gchar *error = NULL;
    const gchar *symbol_name;
    const gchar *module_name;
    GModule *module = NULL;
    gboolean (* init) (InputPadGtkWindow *window);

    g_return_if_fail (window != NULL &&
                      INPUT_PAD_IS_GTK_WINDOW (window));
    g_return_if_fail (window->priv != NULL);

    if (window->priv->module_gdk_xtest) {
        return;
    }

    g_return_if_fail (MODULE_XTEST_GDK_BASE != NULL);

    if (!g_module_supported ()) {
        error = g_module_error ();
        g_warning ("Module (%s) is not supported on your platform: %s",
                   MODULE_XTEST_GDK_BASE, error ? error : "");
        return;
    }

    filename = g_module_build_path (MODULE_XTEST_GDK_DIR, MODULE_XTEST_GDK_BASE);
    g_return_if_fail (filename != NULL);

    module = g_module_open (filename, G_MODULE_BIND_LAZY);
    if (module == NULL) {
        error = g_module_error ();
        g_warning ("Could not open %s: %s", filename, error ? error : "");
        g_free (filename);
        return;
    }
    g_free (filename);

    window->priv->module_gdk_xtest = module;

    symbol_name = "input_pad_module_init";
    module_name = g_module_name (module);
    if (!g_module_symbol (module, symbol_name, (gpointer *)&init)) {
        error = g_module_error ();
        g_warning ("Could not find '%s' in %s: %s", symbol_name,
                   module_name ? module_name : "",
                   error ? error : "");
        input_pad_gtk_window_xtest_gdk_destroy (window);
        return;
    }
    if (init == NULL) {
        g_warning ("Function '%s' is NULL in %s", symbol_name,
                   module_name ? module_name : "");
        input_pad_gtk_window_xtest_gdk_destroy (window);
        return;
    }
    if (!init (window)) {
        g_warning ("Function '%s' failed to be run in %s", symbol_name,
                   module_name ? module_name : "");
        input_pad_gtk_window_xtest_gdk_destroy (window);
        return;
    }
}

static void
input_pad_gtk_window_xtest_gdk_setup (InputPadGtkWindow *window)
{
    const gchar *symbol_name = "input_pad_module_setup";
    const gchar *module_name;
    const gchar *error = NULL;
    gboolean (* setup) (InputPadGtkWindow *window);

    g_return_if_fail (window != NULL &&
                      INPUT_PAD_IS_GTK_WINDOW (window));
    g_return_if_fail (window->priv != NULL);
    g_return_if_fail (window->priv->module_gdk_xtest != NULL);

    module_name = g_module_name (window->priv->module_gdk_xtest);
    if (!g_module_symbol (window->priv->module_gdk_xtest, symbol_name,
                          (gpointer *)&setup)) {
        error = g_module_error ();
        g_warning ("Could not find '%s' in %s: %s", symbol_name,
                   module_name ? module_name : "",
                   error ? error : "");
        return;
    }
    if (setup == NULL) {
        g_warning ("Function '%s' is NULL in %s", symbol_name,
                   module_name ? module_name : "");
        return;
    }
    if (!setup (window)) {
        g_warning ("Function '%s' failed to be run in %s", symbol_name,
                   module_name ? module_name : "");
        return;
    }
}

#endif

static void
input_pad_gtk_window_set_group (InputPadGtkWindow *window)
{
    InputPadGtkWindowPrivate *priv = INPUT_PAD_GTK_WINDOW_GET_PRIVATE (window);
    if (priv->group == NULL) {
        priv->group = input_pad_group_parse_all_files ();
    }
    window->priv = priv;
}

static void
input_pad_gtk_window_init (InputPadGtkWindow *window)
{
    window->child = TRUE;
    input_pad_gtk_window_set_group (window);
#ifdef MODULE_XTEST_GDK_BASE
    if (use_module_xtest) {
        input_pad_gtk_window_xtest_gdk_init (window);
    }
#endif
}

#if 0
static void
input_pad_gtk_window_buildable_parser_finished (GtkBuildable *buildable,
                                                     GtkBuilder   *builder)
{
    InputPadGtkWindow *window = INPUT_PAD_GTK_WINDOW (buildable);
    input_pad_gtk_window_set_group (window);
    parent_buildable_iface->parser_finished (buildable, builder);
}
#endif

static void
input_pad_gtk_window_buildable_interface_init (GtkBuildableIface *iface)
{
#if 0
    parent_buildable_iface = g_type_interface_peek_parent (iface);
    iface->set_buildable_property = parent_buildable_iface->set_buildable_property;
    iface->parser_finished = input_pad_gtk_window_buildable_parser_finished;
    iface->custom_tag_start = parent_buildable_iface->custom_tag_start;
    iface->custom_finished = parent_buildable_iface->custom_finished;
#endif
}

static void
input_pad_gtk_window_real_destroy (GtkObject *object)
{
    InputPadGtkWindow *window = INPUT_PAD_GTK_WINDOW (object);

    if (window->priv) {
        if (window->priv->group) {
            input_pad_group_destroy (window->priv->group);
            window->priv->group = NULL;
        }
#ifdef MODULE_XTEST_GDK_BASE
        if (use_module_xtest) {
            input_pad_gtk_window_xtest_gdk_destroy (window);
        }
#endif
        window->priv = NULL;
    }
    GTK_OBJECT_CLASS (input_pad_gtk_window_parent_class)->destroy (object);
}

static void
input_pad_gtk_window_real_realize (GtkWidget *window)
{
#ifdef MODULE_XTEST_GDK_BASE
    if (use_module_xtest) {
        input_pad_gtk_window_xtest_gdk_setup (INPUT_PAD_GTK_WINDOW (window));
    }
#endif
    GTK_WIDGET_CLASS (input_pad_gtk_window_parent_class)->realize (window);
}

static gboolean
input_pad_gtk_window_real_button_pressed (InputPadGtkWindow *window,
                                          const gchar       *str,
                                          guint              type,
                                          guint              keysym,
                                          guint              keycode,
                                          guint              state)
{
    if (type == INPUT_PAD_TABLE_TYPE_CHARS) {
        if (keysym > 0) {
            send_key_event (GTK_WIDGET(window)->window, keysym, keycode, state);
        } else {
            g_print ("%s", str ? str : "");
        }
    } else if (type == INPUT_PAD_TABLE_TYPE_KEYSYMS) {
        send_key_event (GTK_WIDGET(window)->window, keysym, keycode, state);
    } else {
        g_warning ("Currently your table type is not supported.");
    }
    return FALSE;
}

static void
input_pad_gtk_window_class_init (InputPadGtkWindowClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkObjectClass *object_class = (GtkObjectClass *) klass;
    GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

    object_class->destroy = input_pad_gtk_window_real_destroy;
    widget_class->realize = input_pad_gtk_window_real_realize;

    klass->button_pressed = input_pad_gtk_window_real_button_pressed;

    g_type_class_add_private (klass, sizeof (InputPadGtkWindowPrivate));

    signals[BUTTON_PRESSED] =
        g_signal_new (I_("button-pressed"),
                      G_TYPE_FROM_CLASS (gobject_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (InputPadGtkWindowClass, button_pressed),
                      g_signal_accumulator_true_handled, NULL,
                      INPUT_PAD_BOOL__STRING_UINT_UINT_UINT_UINT,
                      G_TYPE_BOOLEAN,
                      5, G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                      G_TYPE_UINT, G_TYPE_UINT,
                      G_TYPE_UINT, G_TYPE_UINT);
}

GtkWidget *
_input_pad_gtk_window_new_with_gtype (GtkWindowType type,
                                      unsigned int child,
                                      gboolean     gtype)
{
    GtkWidget *window;

    g_return_val_if_fail (type >= GTK_WINDOW_TOPLEVEL && type <= GTK_WINDOW_POPUP, NULL);

    /* FIXME: Add INPUT_PAD_TYPE_GTK_WINDOW for a workaround.
     * python + gtkbuilder outputs an error:
     * ** ERROR **: ERROR: Invalid object type `InputPadGtkWindow'
     * swig bug with gtkbuilder? Ignored -export-dynamic
     */
    if (gtype) {
        INPUT_PAD_TYPE_GTK_WINDOW;
    }

    window = create_ui ();
    INPUT_PAD_GTK_WINDOW (window)->parent.type = type;
    INPUT_PAD_GTK_WINDOW (window)->child = child;

    return window; 
}

GtkWidget *
input_pad_gtk_window_new (GtkWindowType type, unsigned int child)
{
    return _input_pad_gtk_window_new_with_gtype (type, child, FALSE);
}

void
input_pad_window_init (int *argc, char ***argv, InputPadWindowType type)
{
    GOptionContext *context;
    GError *error = NULL;

#ifdef ENABLE_NLS
    bindtextdomain (GETTEXT_PACKAGE, IBUS_LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

    gtk_set_locale ();
#endif

    if (type != INPUT_PAD_WINDOW_TYPE_GTK) {
        g_warning ("Currently GTK type only is supported. Ignoring...");
    }

    context = g_option_context_new (N_("Input Pad"));
    g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
    g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
    g_option_context_add_group (context, gtk_get_option_group (TRUE));
    g_option_context_parse (context, argc, argv, &error);
    g_option_context_free (context);

    gtk_init (argc, argv);
}

void *
input_pad_window_new (unsigned int ibus)
{
    return input_pad_gtk_window_new (GTK_WINDOW_TOPLEVEL, ibus);
}

void *
_input_pad_window_new_with_gtype (unsigned int ibus, gboolean gtype)
{
    return _input_pad_gtk_window_new_with_gtype (GTK_WINDOW_TOPLEVEL,
                                                 ibus, gtype);
}

void
input_pad_window_show (void *window_data)
{
    GtkWidget *window;
    InputPadGtkWindow *input_pad_window;

    g_return_if_fail (window_data != NULL &&
                      INPUT_PAD_IS_GTK_WINDOW (window_data));

    window = GTK_WIDGET (window_data);
    input_pad_window = INPUT_PAD_GTK_WINDOW (window);

    if (input_pad_window->priv && !input_pad_window->priv->show_all) {
        gtk_widget_show_all (window);
        input_pad_window->priv->show_all = TRUE;
    } else {
        gtk_widget_show (window);
    }
}

void
input_pad_window_hide (void *window_data)
{
    g_return_if_fail (window_data != NULL &&
                      GTK_IS_WIDGET (window_data));

    gtk_widget_hide (GTK_WIDGET (window_data));
}

void
input_pad_window_main (void *window_data)
{
    gtk_main ();
}

void
input_pad_window_destroy (void *window_data)
{
    g_return_if_fail (window_data != NULL &&
                      GTK_IS_WIDGET (window_data));

    gtk_widget_destroy (GTK_WIDGET (window_data));
}