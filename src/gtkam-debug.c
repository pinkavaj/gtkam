/* gtkam-debug.c
 *
 * Copyright (C) 2001 Lutz M�ller <urc8@rz.uni-karlsruhe.de>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "gtkam-debug.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include <stdio.h>
#include <string.h>

#include <gtk/gtktextview.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkvscrollbar.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkfilesel.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkstock.h>

#include <gphoto2/gphoto2-port-log.h>

struct _GtkamDebugPrivate
{
	GtkTextBuffer *buffer;

	gboolean log_error, log_verbose, log_debug, log_data;

	guint log_func_id;
};

#define PARENT_TYPE GTK_TYPE_DIALOG
static GtkDialogClass *parent_class;

static void
gtkam_debug_destroy (GtkObject *object)
{
	GtkamDebug *debug = GTKAM_DEBUG (object);

	if (debug->priv->log_func_id) {
		gp_log_remove_func (debug->priv->log_func_id);
		debug->priv->log_func_id = 0;
	}

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gtkam_debug_finalize (GObject *object)
{
	GtkamDebug *debug = GTKAM_DEBUG (object);

	g_free (debug->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtkam_debug_class_init (gpointer g_class, gpointer class_data)
{
	GtkObjectClass *object_class;
	GObjectClass *gobject_class;

	object_class = GTK_OBJECT_CLASS (g_class);
	object_class->destroy  = gtkam_debug_destroy;

	gobject_class = G_OBJECT_CLASS (g_class);
	gobject_class->finalize = gtkam_debug_finalize;

	parent_class = g_type_class_peek_parent (g_class);
}

static void
gtkam_debug_init (GTypeInstance *instance, gpointer g_class)
{
	GtkamDebug *debug = GTKAM_DEBUG (instance);

	debug->priv = g_new0 (GtkamDebugPrivate, 1);
}

GType
gtkam_debug_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo ti;

		memset (&ti, 0, sizeof (GTypeInfo));
		ti.class_size    = sizeof (GtkamDebugClass);
		ti.class_init    = gtkam_debug_class_init;
		ti.instance_size = sizeof (GtkamDebug);
		ti.instance_init = gtkam_debug_init;

		type = g_type_register_static (PARENT_TYPE, "GtkamDebug",
					       &ti, 0);
	}

	return (type);
}

static void
log_func (GPLogLevel level, const char *domain, const char *format,
	  va_list args, void *data)
{
	GtkamDebug *debug;
	const gchar *err = "*** ERROR *** ";
	gchar *message;
	GtkTextIter iter;

        g_return_if_fail (GTKAM_IS_DEBUG (data));

        debug = GTKAM_DEBUG (data);

	switch (level) {
	case GP_LOG_ERROR:
		if (!debug->priv->log_error)
			return;
		gtk_text_buffer_insert (debug->priv->buffer, &iter, err,
					strlen (err));
		break;
	case GP_LOG_VERBOSE:
		if (!debug->priv->log_verbose)
			return;
		break;
	case GP_LOG_DEBUG:
		if (!debug->priv->log_debug)
			return;
		break;
	case GP_LOG_DATA:
		if (!debug->priv->log_data)
			return;
		break;
	default:
		break;
	}

	/* Show the message */
	message = g_strdup_vprintf (format, args);
       	gtk_text_buffer_insert (debug->priv->buffer, &iter,
				message, strlen (message));
	g_free (message);
	gtk_text_buffer_insert (debug->priv->buffer, &iter, "\n", 1);
}

static void
on_debug_toggled (GtkToggleButton *toggle, GtkamDebug *debug)
{
	debug->priv->log_debug = toggle->active;
}

static void
on_verbose_toggled (GtkToggleButton *toggle, GtkamDebug *debug)
{
	debug->priv->log_verbose = toggle->active;
}

static void
on_error_toggled (GtkToggleButton *toggle, GtkamDebug *debug)
{
	debug->priv->log_error = toggle->active;
}

static void
on_data_toggled (GtkToggleButton *toggle, GtkamDebug *debug)
{
	debug->priv->log_data = toggle->active;
}

static void
on_ok_clicked (GtkButton *button, gboolean *ok)
{
	*ok = TRUE;
	gtk_main_quit ();
}

static void
on_debug_save_as_clicked (GtkButton *button, GtkamDebug *debug)
{
	GtkWidget *fsel;
	gboolean ok = FALSE;
	const char *fname;

	fsel = gtk_file_selection_new (_("Save As..."));
	gtk_window_set_transient_for (GTK_WINDOW (fsel), GTK_WINDOW (debug));
	gtk_widget_show (fsel);

	gtk_file_selection_set_filename (GTK_FILE_SELECTION (fsel),
					 "gtkam.debug");
	g_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (fsel)->ok_button),
			    "clicked", GTK_SIGNAL_FUNC (on_ok_clicked), &ok);
	g_signal_connect (GTK_OBJECT (
				GTK_FILE_SELECTION (fsel)->cancel_button),
			    "clicked", GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

	gtk_main ();

	if (ok) {
		FILE *file;
		gchar *buffer;

		fname = gtk_file_selection_get_filename (
						GTK_FILE_SELECTION (fsel));
		file = fopen (fname, "w");
		if (!file)
			g_warning (_("Could not open '%s'!"), fname);
		else {
			buffer = gtk_text_buffer_get_text (
					debug->priv->buffer, NULL, NULL,
					TRUE);
			fputs (buffer, file);
			g_free (buffer);
			fclose (file);
		}
	}

	gtk_object_destroy (GTK_OBJECT (fsel));
}

static void
on_debug_close_clicked (GtkButton *button, GtkamDebug *debug)
{
	gtk_object_destroy (GTK_OBJECT (debug));
}

GtkWidget *
gtkam_debug_new (void)
{
	GtkamDebug *debug;
	GtkWidget *button, *text, *vscrollbar, *hbox, *check, *label;

	debug = g_object_new (GTKAM_TYPE_DEBUG, NULL);

	debug->priv->log_func_id = gp_log_add_func (GP_LOG_ALL,
						    log_func, debug);

	hbox = gtk_hbox_new (FALSE, 5);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (debug)->vbox), hbox,
			    FALSE, FALSE, 0);

	label = gtk_label_new (_("Type of messages to log:"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	check = gtk_check_button_new_with_label (_("Error"));
	gtk_widget_show (check);
	gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, FALSE, 0);
	g_signal_connect (GTK_OBJECT (check), "toggled",
			    GTK_SIGNAL_FUNC (on_error_toggled), debug);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);

	check = gtk_check_button_new_with_label (_("Verbose"));
	gtk_widget_show (check);
	gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, FALSE, 0);
	g_signal_connect (GTK_OBJECT (check), "toggled",
			    GTK_SIGNAL_FUNC (on_verbose_toggled), debug);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);

	check = gtk_check_button_new_with_label (_("Debug"));
	gtk_widget_show (check);
	gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, FALSE, 0);
	g_signal_connect (GTK_OBJECT (check), "toggled",
			    GTK_SIGNAL_FUNC (on_debug_toggled), debug);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);

	check = gtk_check_button_new_with_label (_("Data"));
	gtk_widget_show (check);
	gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, FALSE, 0);
	g_signal_connect (GTK_OBJECT (check), "toggled",
			    GTK_SIGNAL_FUNC (on_data_toggled), debug);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), FALSE); 

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (debug)->vbox), hbox,
			    TRUE, TRUE, 0);

	debug->priv->buffer = gtk_text_buffer_new (NULL);

	text = gtk_text_view_new_with_buffer (debug->priv->buffer);
	gtk_widget_show (text);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (text), FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), text, TRUE, TRUE, 0);

	vscrollbar = gtk_vscrollbar_new (GTK_TEXT_VIEW (text)->vadjustment);
	gtk_widget_show (vscrollbar);
	gtk_box_pack_end (GTK_BOX (hbox), vscrollbar, FALSE, FALSE, 0);

	button = gtk_button_new_from_stock (GTK_STOCK_SAVE_AS);
	gtk_widget_show (button);
	g_signal_connect (GTK_OBJECT (button), "clicked",
			    GTK_SIGNAL_FUNC (on_debug_save_as_clicked), debug);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (debug)->action_area),
			   button);

	button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	gtk_widget_show (button);
	g_signal_connect (GTK_OBJECT (button), "clicked",
			    GTK_SIGNAL_FUNC (on_debug_close_clicked), debug);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (debug)->action_area),
			   button);
	gtk_widget_grab_focus (button);

	return (GTK_WIDGET (debug));
}
