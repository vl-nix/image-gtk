/*
* Copyright 2022 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "image-app.h"
#include "image-win.h"

struct _ImageApp
{
	GtkApplication  parent_instance;
};

G_DEFINE_TYPE ( ImageApp, image_app, GTK_TYPE_APPLICATION )

static void image_app_open ( GApplication *app, GFile **files, G_GNUC_UNUSED int n_files, G_GNUC_UNUSED const char *hint )
{
	image_win_new ( files[0], IMAGE_APP ( app ) );
}

static void image_app_activate ( GApplication *app )
{
	image_win_new ( NULL, IMAGE_APP ( app ) );
}

static void image_app_init ( G_GNUC_UNUSED ImageApp *app )
{

}

static void image_app_finalize ( GObject *object )
{
	G_OBJECT_CLASS ( image_app_parent_class )->finalize ( object );
}

static void image_app_class_init ( ImageAppClass *class )
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	G_APPLICATION_CLASS (class)->open     = image_app_open;
	G_APPLICATION_CLASS (class)->activate = image_app_activate;

	object_class->finalize = image_app_finalize;
}

ImageApp * image_app_new ( void )
{
	return g_object_new ( IMAGE_TYPE_APP, "application-id", "org.gtk.image-gtk", "flags", G_APPLICATION_HANDLES_OPEN, NULL );
}
