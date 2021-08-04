/*
* Copyright 2021 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "preview.h"

struct _Preview
{
	GtkBox parent_instance;

	GtkImage *image;
};

G_DEFINE_TYPE ( Preview, preview, GTK_TYPE_BOX )

static void preview_message_dialog ( const char *f_error, const char *file_or_info, GtkMessageType mesg_type, GtkWindow *window )
{
	GtkMessageDialog *dialog = ( GtkMessageDialog *)gtk_message_dialog_new ( window, GTK_DIALOG_MODAL, mesg_type, 
		GTK_BUTTONS_CLOSE, "%s\n%s", f_error, file_or_info );

	gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), "system-file-manager" );

	gtk_dialog_run     ( GTK_DIALOG ( dialog ) );
	gtk_widget_destroy ( GTK_WIDGET ( dialog ) );
}

static void preview_signal_name_set_vhlr ( Preview *preview, enum p_num num )
{
	GdkPixbuf *pb = NULL;
	GdkPixbuf *pixbuf = gtk_image_get_pixbuf ( preview->image );

	if ( num == PHR ) pb = gdk_pixbuf_flip ( pixbuf, TRUE  );
	if ( num == PVT ) pb = gdk_pixbuf_flip ( pixbuf, FALSE );

	if ( num == PRT ) pb = gdk_pixbuf_rotate_simple ( pixbuf, GDK_PIXBUF_ROTATE_CLOCKWISE );
	if ( num == PLT ) pb = gdk_pixbuf_rotate_simple ( pixbuf, GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE );

	gtk_image_set_from_pixbuf ( preview->image, pb );

	if ( pb ) g_object_unref ( pb );
}

static void preview_set_label ( int org_w, int org_h, int scale_w, int scale_h, Preview *preview )
{
	double prc = (double)( scale_w * scale_h ) * 100 / ( org_w * org_h );

	char text[256];
	sprintf ( text, "%.2f%%  %d x %d", prc, org_w, org_h );

	g_signal_emit_by_name ( preview, "preview-set-label", text );
}

static void preview_signal_name_plus_minus ( Preview *preview, const char *path, gboolean plus_minus )
{
	GdkPixbuf *pbset = NULL;
	GdkPixbuf *pbimage = gtk_image_get_pixbuf ( preview->image );

	int width  = gdk_pixbuf_get_width  ( pbimage );
	int height = gdk_pixbuf_get_height ( pbimage );

	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file ( path, NULL );

	int pw = gdk_pixbuf_get_width  ( pixbuf );
	int ph = gdk_pixbuf_get_height ( pixbuf );

	int set_w = ( plus_minus ) ? width  + ( pw / 10 ) : width  - ( pw / 10 );
	int set_h = ( plus_minus ) ? height + ( ph / 10 ) : height - ( ph / 10 );

	if ( set_w > 16 && set_h > 16 ) pbset = gdk_pixbuf_new_from_file_at_size ( path, set_w, set_h, NULL );

	if ( pbset ) preview_set_label ( pw, ph, set_w, set_h, preview );
	if ( pbset ) gtk_image_set_from_pixbuf ( preview->image, pbset );

	if ( pbset  ) g_object_unref ( pbset  );
	if ( pixbuf ) g_object_unref ( pixbuf );
}

static void preview_signal_name_set_file ( Preview *preview, const char *path, int w, int h, gboolean org )
{
	GError *error = NULL;
	GdkPixbuf *pbf_org = gdk_pixbuf_new_from_file ( path, &error );

	if ( error )
	{
		GtkWidget *window = gtk_widget_get_toplevel ( GTK_WIDGET ( preview->image ) );

		preview_message_dialog ( "", error->message, GTK_MESSAGE_ERROR, GTK_WINDOW ( window ) );

		g_message ( "%s:: %s ", __func__, error->message );
		g_error_free ( error );

		return;
	}

	int pw = gdk_pixbuf_get_width  ( pbf_org );
	int ph = gdk_pixbuf_get_height ( pbf_org );

	if ( org ) preview_set_label ( pw, ph, pw, ph, preview );
	if ( org ) { gtk_image_set_from_file ( preview->image, path ); g_object_unref ( pbf_org ); return; }

	int set_w = ( pw < w ) ? pw : w;
	int set_h = ( ph < h ) ? ph : h;

	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size ( path, set_w, set_h, NULL );

	if ( pixbuf ) preview_set_label ( pw, ph, set_w, set_h, preview );
	if ( pixbuf ) gtk_image_set_from_pixbuf ( preview->image, pixbuf );

	if ( pixbuf  ) g_object_unref ( pixbuf ) ;
	if ( pbf_org ) g_object_unref ( pbf_org );
}

static void preview_init ( Preview *preview )
{
	GtkBox *box = GTK_BOX ( preview );
	gtk_orientable_set_orientation ( GTK_ORIENTABLE ( box ), GTK_ORIENTATION_VERTICAL );

	gtk_box_set_spacing ( box, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( box ), TRUE );

	preview->image = (GtkImage *)gtk_image_new ();
	gtk_widget_set_visible ( GTK_WIDGET ( preview->image ), TRUE );

	gtk_box_pack_start ( box, GTK_WIDGET ( preview->image ), TRUE, TRUE, 0 );

	g_signal_connect ( preview, "preview-set-vhlr",   G_CALLBACK ( preview_signal_name_set_vhlr   ), NULL );
	g_signal_connect ( preview, "preview-set-file",   G_CALLBACK ( preview_signal_name_set_file   ), NULL );
	g_signal_connect ( preview, "preview-plus-minus", G_CALLBACK ( preview_signal_name_plus_minus ), NULL );
}

static void preview_finalize ( GObject *object )
{
	G_OBJECT_CLASS ( preview_parent_class )->finalize ( object );
}

static void preview_class_init ( PreviewClass *class )
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);

	oclass->finalize = preview_finalize;

	g_signal_new ( "preview-set-vhlr",   G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT );
	g_signal_new ( "preview-set-label",  G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING );
	g_signal_new ( "preview-plus-minus", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_BOOLEAN );
	g_signal_new ( "preview-set-file",   G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, 
		G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_BOOLEAN );
}

Preview * preview_new ( void )
{
	return g_object_new ( PREVIEW_TYPE_BOX, NULL );
}

