/* gtkam-list.c
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
#include "gtkam-list.h"

#include <stdio.h>

#include <gtk/gtkmain.h>

#include <gphoto2/gphoto2-list.h>

#include "gtkam-error.h"
#include "util.h"
#include "frontend.h"
#include "../pixmaps/no_thumbnail.xpm"
#include "gtkam-save.h"

struct _GtkamListPrivate
{
	Camera *camera;

	gboolean thumbnails;
};

#define PARENT_TYPE GTK_TYPE_ICON_LIST
static GtkIconListClass *parent_class;

enum {
	FILE_DELETED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

//FIXME
extern GtkWidget *gp_gtk_progress_window;

static void
gtkam_list_destroy (GtkObject *object)
{
	GtkamList *list = GTKAM_LIST (object);

	if (list->priv->camera) {
		gp_camera_unref (list->priv->camera);
		list->priv->camera = NULL;
	}

	if (list->path) {
		g_free (list->path);
		list->path = NULL;
	}

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
gtkam_list_finalize (GtkObject *object)
{
	GtkamList *list = GTKAM_LIST (object);

	g_free (list->priv);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtkam_list_class_init (GtkamListClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy  = gtkam_list_destroy;
	object_class->finalize = gtkam_list_finalize;

	signals[FILE_DELETED] = gtk_signal_new ("file_deleted",
		GTK_RUN_LAST, object_class->type,
		GTK_SIGNAL_OFFSET (GtkamListClass, file_deleted),
		gtk_marshal_NONE__POINTER_POINTER, GTK_TYPE_NONE, 2,
		GTK_TYPE_POINTER, GTK_TYPE_POINTER);
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	parent_class = gtk_type_class (PARENT_TYPE);
}

static void
gtkam_list_init (GtkamList *list)
{
	list->priv = g_new0 (GtkamListPrivate, 1);
	list->priv->thumbnails = TRUE;
}

GtkType
gtkam_list_get_type (void)
{
	static GtkType list_type = 0;

	if (!list_type) {
		static const GtkTypeInfo list_info = {
			"GtkamList",
			sizeof (GtkamList),
			sizeof (GtkamListClass),
			(GtkClassInitFunc)  gtkam_list_class_init,
			(GtkObjectInitFunc) gtkam_list_init,
			NULL, NULL, NULL};
		list_type = gtk_type_unique (PARENT_TYPE, &list_info);
	}

	return (list_type);
}

GtkWidget *
gtkam_list_new (void)
{
	GtkamList *list;

	list = gtk_type_new (GTKAM_TYPE_LIST);
	gtk_icon_list_construct (GTK_ICON_LIST (list), 80,
				 GTK_ICON_LIST_TEXT_BELOW);
	gtk_icon_list_set_selection_mode (GTK_ICON_LIST (list),
					  GTK_SELECTION_MULTIPLE);

	return (GTK_WIDGET (list));
}

void
gtkam_list_set_camera (GtkamList *list, Camera *camera)
{
	g_return_if_fail (GTKAM_IS_LIST (list));

	if (list->priv->camera)
		gp_camera_unref (list->priv->camera);
	list->priv->camera = camera;
	if (camera)
		gp_camera_ref (camera);

	if (list->path) {
		g_free (list->path);
		list->path = NULL;
	}

	gtkam_list_refresh (list);
}

static gint
on_button_press_event (GtkWidget *widget, GdkEventButton *event,
		       GtkamList *list)
{
	GtkWidget *dialog, *window;
	GtkIconListItem *item;
	CameraFile *file;
	GdkPixmap *pixmap;
	GdkBitmap *bitmap;
	const char *data;
	long int size;
	int result;
	gchar *msg;
	gint i;
	GList *glist;

	glist = GTK_ICON_LIST (list)->icons;
	for (item = NULL, i = 0; i < g_list_length (glist); i++) {
		item = g_list_nth_data (glist, i);
		if ((item->entry == widget) || (item->eventbox == widget))
			break;
	}
	g_return_val_if_fail (i != g_list_length (glist), TRUE);

	/* Double-click: Get thumbnail */
	if (event->type == GDK_2BUTTON_PRESS) {
		gp_file_new (&file);
		result = gp_camera_file_get (list->priv->camera,
					     list->path, item->label,
					     GP_FILE_TYPE_PREVIEW, file);
		if (result < 0) {
			window = gtk_widget_get_ancestor (GTK_WIDGET (list),
							  GTK_TYPE_WINDOW);
			msg = g_strdup_printf ("Could not get preview of file "
					       "'%s' in folder '%s'",
					       item->label,
					       list->path);
			dialog = gtkam_error_new (msg, result,
						  list->priv->camera, window);
			g_free (msg);
			gtk_widget_show (dialog);
		} else {
			gp_file_get_data_and_size (file, &data, &size);
			gdk_image_new_from_data ((char*) data, size, 1,
						 &pixmap, &bitmap);
			gtk_pixmap_set (GTK_PIXMAP (item->pixmap),
					pixmap, bitmap);
			item->state = GTK_STATE_SELECTED;
		}
		gp_file_unref (file);
	}

	if (item->state == GTK_STATE_SELECTED)
		gtk_icon_list_unselect_icon (GTK_ICON_LIST (list), item);
	else
		gtk_icon_list_select_icon (GTK_ICON_LIST (list), item);

	while (gtk_events_pending ())
		gtk_main_iteration ();

	gtk_entry_set_position (GTK_ENTRY (item->entry), 10000);

	return (TRUE);
}

void
gtkam_list_set_path (GtkamList *list, const gchar *path)
{
	GtkWidget *dialog, *window;
	GtkIconListItem *item;
	gchar *msg;
	CameraList flist;
	CameraFile *file;
	int result;
	const char *name, *data;
	long int size;
	GdkPixmap *pixmap;
	GdkBitmap *bitmap;
	gint i;

	g_return_if_fail (GTKAM_IS_LIST (list));
	g_return_if_fail (path != NULL);

	if (list->path)
		g_free (list->path);
	list->path = g_strdup (path);

	gtk_icon_list_clear (GTK_ICON_LIST (list));

	if (!list->priv->camera)
		return;

	window = gtk_widget_get_ancestor (GTK_WIDGET (list), GTK_TYPE_WINDOW);

	result = gp_camera_folder_list_files (list->priv->camera, path, &flist);
	if (result < 0) {
		msg = g_strdup_printf ("Could not get file list for folder "
				       "'%s'", path);
		dialog = gtkam_error_new (msg, result, list->priv->camera,
					  window);
		gtk_widget_show (dialog);
		return;
	}

//FIXME
	gtk_widget_show (gp_gtk_progress_window);

	gp_file_new (&file);
	for (i = 0; i < gp_list_count (&flist); i++) {
		gp_list_get_name (&flist, i, &name);

//FIXME: Write a nice dialog for this
		msg = g_strdup_printf ("Getting Thumbnail #%04i of %04i\n%s",
				       i + 1, gp_list_count (&flist), name);
		frontend_message (NULL, msg);
		g_free (msg);
		while (gtk_events_pending ())
			gtk_main_iteration ();

		item = gtk_icon_list_add_from_data (
				GTK_ICON_LIST (list), no_thumbnail_xpm,
				name, NULL);
		if (list->priv->thumbnails &&
		    (list->priv->camera->abilities->file_operations &
		     GP_FILE_OPERATION_PREVIEW)) {
			result = gp_camera_file_get (list->priv->camera, path,
					name, GP_FILE_TYPE_PREVIEW, file);
			if (result < 0) {
				msg = g_strdup_printf ("Could not get file "
						       "'%s'", name);
				dialog = gtkam_error_new (msg, result,
							  list->priv->camera,
							  window);
				gtk_widget_show (dialog);
			} else {
				gp_file_get_data_and_size (file, &data, &size);
				gdk_image_new_from_data ((char*) data, size, 1,
							 &pixmap, &bitmap);
				gtk_pixmap_set (GTK_PIXMAP (item->pixmap),
						pixmap, bitmap);
			}
		}
		gtk_signal_connect (GTK_OBJECT (item->eventbox),
				    "button_press_event",
				    GTK_SIGNAL_FUNC (on_button_press_event),
				    list);
		gtk_signal_connect (GTK_OBJECT (item->entry),
				    "button_press_event",
				    GTK_SIGNAL_FUNC (on_button_press_event),
				    list);
	}
	gp_file_unref (file);

//FIXME
	gtk_widget_hide (gp_gtk_progress_window);
}

void
gtkam_list_set_thumbnails (GtkamList *list, gboolean thumbnails)
{
	g_return_if_fail (GTKAM_IS_LIST (list));

	if (list->priv->thumbnails == thumbnails)
		return;

	list->priv->thumbnails = thumbnails;
	gtkam_list_refresh (list);
}

void
gtkam_list_save_selected (GtkamList *list)
{
	GtkIconListItem *item;
	GSList *filenames = NULL;
	GtkWidget *save;
	guint i;

	g_return_if_fail (GTKAM_IS_LIST (list));

	if (!g_list_length (GTK_ICON_LIST (list)->selection))
		return;

	for (i = 0; i < g_list_length (GTK_ICON_LIST (list)->selection); i++) {
		item = g_list_nth_data (GTK_ICON_LIST (list)->selection, i);
		filenames = g_slist_append (filenames,
				gtk_entry_get_text (GTK_ENTRY (item->entry)));
	}

	save = gtkam_save_new (list->priv->camera, list->path,
			       filenames);
	g_slist_free (filenames);
	gtk_widget_show (save);
}

void
gtkam_list_delete_selected (GtkamList *list)
{
	GtkIconListItem *item;
	guint i;
	const gchar *filename;
	gchar *msg;
	GtkWidget *dialog, *window;
	int result;

	g_return_if_fail (GTKAM_IS_LIST (list));

	if (!list->priv->camera || !list->path)
		return;

	if (!g_list_length (GTK_ICON_LIST (list)->selection)) 
		return;

	for (i = 0; i < g_list_length (GTK_ICON_LIST (list)->selection); i++) {
		item = g_list_nth_data (GTK_ICON_LIST (list)->selection, i);
		filename = gtk_entry_get_text (GTK_ENTRY (item->entry));
		result = gp_camera_file_delete (list->priv->camera,
						list->path, filename);
		if (result < 0) {
			window = gtk_widget_get_ancestor (GTK_WIDGET (list),
							  GTK_TYPE_WINDOW);
			msg = g_strdup_printf ("Could not delete '%s' in "
					       "folder '%s'", filename,
					       list->path);
			dialog = gtkam_error_new (msg, result,
				list->priv->camera, window);
			g_free (msg);
			gtk_widget_show (dialog);
		} else {
			gtk_icon_list_freeze (GTK_ICON_LIST (list));
			gtk_icon_list_remove (GTK_ICON_LIST (list), item);
			gtk_icon_list_thaw   (GTK_ICON_LIST (list));
			gtk_signal_emit (GTK_OBJECT (list),
					 signals[FILE_DELETED],
					 list->path, filename);
		}
	}
}

void
gtkam_list_delete_all (GtkamList *list)
{
	int result;
	GtkWidget *dialog, *window;

	g_return_if_fail (GTKAM_IS_LIST (list));

	if (!list->priv->camera || !list->path)
		return;

	result = gp_camera_folder_delete_all (list->priv->camera,
					      list->path);
	if (result < 0) {
		window = gtk_widget_get_ancestor (GTK_WIDGET (list),
						  GTK_TYPE_WINDOW);
		dialog = gtkam_error_new ("Could not delete all photos",
					  result, list->priv->camera,
					  window);
		gtk_widget_show (dialog);
	}

	gtkam_list_refresh (list);
}

void
gtkam_list_refresh (GtkamList *list)
{
	gchar *path = NULL;

	g_return_if_fail (GTKAM_IS_LIST (list));

	gtk_icon_list_clear (GTK_ICON_LIST (list));

	if (list->path && list->priv->camera) {
		path = g_strdup (list->path);
		gtkam_list_set_path (list, path);
		g_free (path);
	}
}