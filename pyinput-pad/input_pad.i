%module input_pad

%{
#include <input-pad.h>
#include <glib.h>
#include <glib-object.h>
%}

%include input-pad.h

%pythoncode
{
get_version = input_pad_get_version

class InputPadWindow:
    def __init__(self, argv=None, type=INPUT_PAD_WINDOW_TYPE_GTK):
        self.window = None
        self.type = type

        if argv == None:
            import sys
            argv = sys.argv
        if type == INPUT_PAD_WINDOW_TYPE_GTK:
            import pygtk
            import gtk

        _input_pad_window_init_wrapper (argv, type)
    def new (self, child=0):
        self.window = _input_pad_window_new_with_gtype (child, True)
    def show(self):
        input_pad_window_show(self.window)
    def hide(self):
        input_pad_window_hide(self.window)
    def get_visible(self):
        return input_pad_window_get_visible(self.window)
    def set_paddir(self, paddir, domain=None):
        return input_pad_window_set_paddir(self.window, paddir, domain)
    def set_char_button_sensitive(self, sensitive):
        return input_pad_window_set_char_button_sensitive(self.window, sensitive)
    def reorder_button_pressed(self):
        return input_pad_window_reorder_button_pressed(self.window)
    def connect(self, signal_id, signal_cb, data=None):
        _input_pad_window_connect_wrapper(self.window, signal_id, signal_cb,
                                          data)
    def main(self):
        input_pad_window_main(self.window)
    def destroy(self):
        input_pad_window_destroy(self.window)
}

%inline %{
/* workaround */
extern void*
_input_pad_window_new_with_gtype (unsigned int  child,
                                  unsigned int  gtype);

void
_input_pad_window_init_wrapper (PyObject *pyargv, InputPadWindowType type)
{
    int i;
    int argc;
    char **argv;

    if (!PyList_Check (pyargv)) {
        PyErr_Warn (PyExc_Warning, "type of argument 2 is not list");
        return;
    }
    argc = PyList_Size (pyargv);
    for (i = 0; i < argc; i++) {
        if (!PyString_Check (PyList_GetItem (pyargv, i))) {
            PyErr_Warn (PyExc_Warning, "argument is not string");
            return;
        }
    }
    argv = calloc (sizeof (char*), argc);
    for (i = 0; i < argc; i++) {
        argv[i] = strdup (PyString_AsString (PyList_GetItem (pyargv, i)));
    }
    input_pad_window_init (&argc, &argv, type);
    PySys_SetArgv (argc, argv);
    for (i = 0; argv && i < argc; i++) {
        free (argv[i]);
    }
    free (argv);
}

typedef struct _python_callback_data
{
    PyObject *pysignal_cb;
    PyObject *pydata;
} python_callback_data;

static unsigned int
button_pressed_cb (void *window,
                   gchar *str,
                   guint type,
                   guint keysym,
                   guint keycode,
                   guint state,
                   gpointer data)
{
    PyObject *retval;
    PyObject *pywindow = NULL;
    PyObject *pystr = NULL;
    python_callback_data *cb_data = (python_callback_data *) data;

    pywindow = SWIG_NewPointerObj (SWIG_as_voidptr(window), SWIGTYPE_p_void,
                                   0 |  0 );
    pystr = PyString_FromString (str);
    retval = PyObject_CallFunction (cb_data->pysignal_cb,
                                   "OOiiiiO",
                                   pywindow, pystr, type,
                                   keysym, keycode, state,
                                   cb_data->pydata);
    Py_DECREF (pystr);

    return FALSE;
}

unsigned long
_input_pad_window_connect_wrapper(void     *window,
                                  char     *signal_id,
                                  PyObject *pysignal_cb,
                                  PyObject *pydata)
{
    gchar *message = NULL;
    python_callback_data *cb_data;

    if (!PyCallable_Check (pysignal_cb)) {
        PyErr_Warn (PyExc_Warning, "not function");
        return 0;
    }

    if (g_strcmp0 (signal_id, "button-pressed") != 0) {
        message = g_strdup_printf ("Your signal_id is not supported: %s",
                                   signal_id ? signal_id : "(null)");
        PyErr_Warn (PyExc_Warning, message);
        g_free (message);
        return 0;
    }

    cb_data = (python_callback_data *) g_new0 (python_callback_data, 1);
    cb_data->pysignal_cb = pysignal_cb;
    cb_data->pydata = pydata;
    return g_signal_connect (G_OBJECT (window), signal_id,
                             G_CALLBACK (button_pressed_cb), cb_data);
}
%}
