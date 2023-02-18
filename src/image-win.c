/*
* Copyright 2023 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "image-win.h"

#include <errno.h>

#define ITEM_WIDTH 80
#define UNUSED G_GNUC_UNUSED

G_LOCK_DEFINE_STATIC ( done_th );

enum cols_enm
{
	COL_PATH,
	COL_NAME,
	COL_IS_DIR,
	COL_IS_LINK,
	COL_IS_PIXBUF,
	COL_PIXBUF,
	NUM_COLS
};

enum size_enm
{
	SIZEx24,
	SIZEx32,
	SIZEx48,
	SIZEx64,
	SIZEx96,
	SIZEx128,
	SIZEx256,
	SISE_N
};

typedef struct _SizeDescr SizeDescr;

struct _SizeDescr
{
	enum size_enm size_n;
	uint16_t size;
};

const SizeDescr icon_descr_size_n[] =
{
	{ SIZEx24, 	 24 },
	{ SIZEx32, 	 32 },
	{ SIZEx48, 	 48 },
	{ SIZEx64, 	 64 },
	{ SIZEx96, 	 96 },
	{ SIZEx128, 128 },
	{ SIZEx256, 256 }
};

enum pb_enm
{
	POR,
	PHR,
	PVT,
	PLT,
	PRT
};

enum bt_enm
{
	BUP, BPR, BNX, BST,
	BLT, BRT, BVR, BHR, 
	BRM, BIF, BIA, 
	BFT, BOR, BMN, BPL, 
	BAL
};

typedef struct _ButtonIcon ButtonIcon;

struct _ButtonIcon
{
	enum bt_enm bt_n;
	const char *icon;
};

const ButtonIcon button_icon_n[] =
{
	{ BUP, "go-up"       },
	{ BPR, "go-previous" },
	{ BNX, "go-next"     },
	{ BST, "media-playback-start"   },

	{ BLT, "object-rotate-left"     },
	{ BRT, "object-rotate-right"    },
	{ BVR, "object-flip-vertical"   },
	{ BHR, "object-flip-horizontal" },

	{ BRM, "remove" },
	{ BIF, "dialog-information" },
	{ BIA, "folder" },

	{ BFT, "zoom-fit-best" },
	{ BOR, "zoom-original" },
	{ BMN, "zoom-out" },
	{ BPL, "zoom-in"  }
};

struct _ImageWin
{
	GtkWindow parent_instance;

	GtkBox *bar_box;
	GtkLabel *bar_label;

	GdkCursor *cursor;
	GtkAdjustment *adjv;
	GtkAdjustment *adjh;

	GFile *file;

	GtkImage *image;
	GtkScrolledWindow *swin_img;

	GtkButton *button_play;
	GtkPopover *popover_time;

	enum pb_enm pb_type_lr;
	enum pb_enm pb_type_hv;

	double av_val;
	double ah_val;

	uint src_play;
	uint16_t timeout;

	gboolean config;
	gboolean original;

	GtkIconView *icon_view;
	GtkScrolledWindow *swin_prw;

	GFile *dir;
	GtkTreeModel *model_t;

	uint8_t   mod_t;
	uint8_t limit_t;
	uint64_t  end_t;

	uint16_t icon_size;

	gboolean preview;

	gboolean break_t;
	gboolean done_t_0;
	gboolean done_t_1;
	gboolean done_t_2;
	gboolean done_t_3;
};

G_DEFINE_TYPE ( ImageWin, image_win, GTK_TYPE_WINDOW )

typedef void ( *fp ) ( ImageWin * );

static void image_win_run_autoplay ( ImageWin * );
static void win_set_dir_file ( GFile *, ImageWin * );

static void dialog_message ( const char *f_error, const char *file_or_info, GtkMessageType mesg_type, GtkWindow *window )
{
	GtkMessageDialog *dialog = ( GtkMessageDialog *)gtk_message_dialog_new ( window, GTK_DIALOG_MODAL, mesg_type, GTK_BUTTONS_CLOSE, "%s\n%s", f_error, file_or_info );

	gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), "system-file-manager" );
	if ( window && GTK_IS_WINDOW ( window ) ) gtk_widget_set_opacity ( GTK_WIDGET ( dialog ), gtk_widget_get_opacity ( GTK_WIDGET ( window ) ) );

	gtk_dialog_run     ( GTK_DIALOG ( dialog ) );
	gtk_widget_destroy ( GTK_WIDGET ( dialog ) );
}

static void win_about ( GtkWindow *window )
{
	GtkAboutDialog *dialog = (GtkAboutDialog *)gtk_about_dialog_new ();
	gtk_window_set_transient_for ( GTK_WINDOW ( dialog ), window );

	gtk_about_dialog_set_logo_icon_name ( dialog, "image" );
	gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), "image" );
	gtk_widget_set_opacity   ( GTK_WIDGET ( dialog ), gtk_widget_get_opacity ( GTK_WIDGET ( window ) ) );

	const char *authors[] = { "Stepan Perun", " ", NULL };

	gtk_about_dialog_set_program_name ( dialog, "Image-Gtk" );
	gtk_about_dialog_set_version ( dialog, VERSION );
	gtk_about_dialog_set_license_type ( dialog, GTK_LICENSE_GPL_3_0 );
	gtk_about_dialog_set_authors ( dialog, authors );
	gtk_about_dialog_set_website ( dialog,   "https://github.com/vl-nix/image-gtk" );
	gtk_about_dialog_set_copyright ( dialog, "Copyright 2023 Image-Gtk" );
	gtk_about_dialog_set_comments  ( dialog, "Lightweight picture viewer" );

	gtk_dialog_run ( GTK_DIALOG (dialog) );
	gtk_widget_destroy ( GTK_WIDGET (dialog) );
}

static gboolean image_win_check_pixbuf ( const char *path )
{
	gboolean ret = TRUE;

	GError *error = NULL;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file ( path, &error );

	if ( error )
	{
		ret = FALSE;

		g_warning ( "%s:: %s ", __func__, error->message );
		g_error_free ( error );
	}

	if ( pixbuf ) g_object_unref ( pixbuf );

	return ret;
}

static void image_win_changed_timeout ( GtkSpinButton *button, ImageWin *win )
{
	gtk_spin_button_update ( button );

	win->timeout = (uint16_t)gtk_spin_button_get_value_as_int ( button );
}

static GtkSpinButton * image_win_create_spinbutton ( uint16_t val, uint16_t min, uint16_t max, uint16_t step, const char *text )
{
	GtkSpinButton *spinbutton = (GtkSpinButton *)gtk_spin_button_new_with_range ( min, max, step );
	gtk_spin_button_set_value ( spinbutton, val );

	gtk_entry_set_icon_from_icon_name ( GTK_ENTRY ( spinbutton ), GTK_ENTRY_ICON_PRIMARY, "appointment-new" );
	gtk_entry_set_icon_tooltip_text   ( GTK_ENTRY ( spinbutton ), GTK_ENTRY_ICON_PRIMARY, text );

	return spinbutton;
}

static void image_win_timeout_run ( G_GNUC_UNUSED GtkButton *button, ImageWin *win )
{
	image_win_run_autoplay ( win );

	GtkImage *image = (GtkImage *)gtk_button_get_image ( win->button_play );
	gtk_image_set_from_icon_name ( image, "media-playback-stop", GTK_ICON_SIZE_MENU );

	gtk_widget_set_visible ( GTK_WIDGET ( win->popover_time ), FALSE );
}

static GtkPopover * image_win_pp_timeout ( ImageWin *win )
{
	GtkPopover *popover = (GtkPopover *)gtk_popover_new ( NULL );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	GtkSpinButton *spinbutton = image_win_create_spinbutton ( win->timeout, 1, 3600, 1, "Seconds" );
	g_signal_connect ( spinbutton, "changed", G_CALLBACK ( image_win_changed_timeout ), win );

	gtk_widget_set_visible ( GTK_WIDGET ( spinbutton ), TRUE );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( spinbutton ), TRUE, TRUE, 0 );

	GtkButton *button = (GtkButton *)gtk_button_new_from_icon_name ( "gtk-apply", GTK_ICON_SIZE_MENU );
	g_signal_connect ( button, "clicked", G_CALLBACK ( image_win_timeout_run ), win );

	gtk_widget_set_visible ( GTK_WIDGET ( button ), TRUE );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( button ), TRUE, TRUE, 0 );

	gtk_container_add ( GTK_CONTAINER ( popover ), GTK_WIDGET ( h_box ) );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	return popover;
}

static void image_win_set_label ( int org_w, int org_h, int scale_w, int scale_h, ImageWin *win )
{
	GFileInfo *finfo = g_file_query_info ( win->file, "standard::size", 0, NULL, NULL );

	uint64_t size = g_file_info_get_attribute_uint64 ( finfo, G_FILE_ATTRIBUTE_STANDARD_SIZE );

	g_autofree char *gsize = g_format_size ( size );

	double prc = (double)( scale_w * scale_h ) * 100 / ( org_w * org_h );

	char text[256];
	sprintf ( text, "%u%%  %d x %d  %s", (uint)prc, org_w, org_h, gsize );

	gtk_label_set_text ( win->bar_label, text );

	if ( finfo ) g_object_unref ( finfo );
}

static void image_win_set_image_plus_minus ( gboolean plus_minus, ImageWin *win )
{
	GdkPixbuf *pbimage = gtk_image_get_pixbuf ( win->image );

	if ( pbimage == NULL ) return;

	g_autofree char *path = g_file_get_path ( win->file );

	GdkPixbuf *pbset = NULL;

	int width  = gdk_pixbuf_get_width  ( pbimage );
	int height = gdk_pixbuf_get_height ( pbimage );

	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file ( path, NULL );

	int pw = gdk_pixbuf_get_width  ( pixbuf );
	int ph = gdk_pixbuf_get_height ( pixbuf );

	int set_w = ( plus_minus ) ? width  + ( pw / 10 ) : width  - ( pw / 10 );
	int set_h = ( plus_minus ) ? height + ( ph / 10 ) : height - ( ph / 10 );

	if ( set_w > 16 && set_h > 16 ) pbset = gdk_pixbuf_new_from_file_at_size ( path, set_w, set_h, NULL );

	if ( pbset ) image_win_set_label ( pw, ph, set_w, set_h, win );
	if ( pbset ) gtk_image_set_from_pixbuf ( win->image, pbset );

	if ( pbset  ) g_object_unref ( pbset  );
	if ( pixbuf ) g_object_unref ( pixbuf );
}

static void image_win_set_image_vhlr ( enum pb_enm num, ImageWin *win )
{
	GdkPixbuf *pbimage = gtk_image_get_pixbuf ( win->image );

	if ( pbimage == NULL ) return;

	GdkPixbuf *pb = NULL;

	if ( num == PHR ) pb = gdk_pixbuf_flip ( pbimage, TRUE  );
	if ( num == PVT ) pb = gdk_pixbuf_flip ( pbimage, FALSE );

	if ( num == PRT ) pb = gdk_pixbuf_rotate_simple ( pbimage, GDK_PIXBUF_ROTATE_CLOCKWISE );
	if ( num == PLT ) pb = gdk_pixbuf_rotate_simple ( pbimage, GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE );

	gtk_image_set_from_pixbuf ( win->image, pb );

	if ( pb ) g_object_unref ( pb );
}

static void image_win_set_image ( ImageWin *win )
{
	g_autofree char *path = g_file_get_path ( win->file );

	if ( path == NULL ) return;

	int w = gtk_widget_get_allocated_width  ( GTK_WIDGET ( win ) );
	int h = gtk_widget_get_allocated_height ( GTK_WIDGET ( win ) );

	gboolean vis = gtk_widget_get_visible ( GTK_WIDGET ( win->bar_box ) );

	if ( vis )
	{
		int bar_h = gtk_widget_get_allocated_height ( GTK_WIDGET ( win->bar_box ) );

		h -= bar_h;
	}

	GError *error = NULL;
	GdkPixbuf *pbf_org = gdk_pixbuf_new_from_file ( path, &error );

	if ( error )
	{
		g_warning ( "%s:: %s ", __func__, error->message );

		g_error_free ( error );

		return;
	}

	gtk_image_clear ( win->image );

	int pw = gdk_pixbuf_get_width  ( pbf_org );
	int ph = gdk_pixbuf_get_height ( pbf_org );

	if ( win->original )
	{
		image_win_set_label ( pw, ph, pw, ph, win );
		gtk_image_set_from_file ( win->image, path ); g_object_unref ( pbf_org );

		if ( win->pb_type_lr != POR ) image_win_set_image_vhlr ( win->pb_type_lr, win );
		if ( win->pb_type_hv != POR ) image_win_set_image_vhlr ( win->pb_type_hv, win );

		return;
	}

	int set_w = ( pw < w ) ? pw : w;
	int set_h = ( ph < h ) ? ph : h;

	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size ( path, set_w, set_h, NULL );

	if ( pixbuf ) image_win_set_label ( pw, ph, set_w, set_h, win );
	if ( pixbuf ) gtk_image_set_from_pixbuf ( win->image, pixbuf );

	if ( pixbuf  ) g_object_unref ( pixbuf ) ;
	if ( pbf_org ) g_object_unref ( pbf_org );

	if ( win->pb_type_lr != POR ) image_win_set_image_vhlr ( win->pb_type_lr, win );
	if ( win->pb_type_hv != POR ) image_win_set_image_vhlr ( win->pb_type_hv, win );
}

static void image_set_file ( GFile *file, ImageWin *win )
{
	g_autofree char *path_new = NULL;

	win->pb_type_lr = POR;
	win->pb_type_hv = POR;

	if ( file ) path_new = g_file_get_path ( file );

	if ( !path_new ) return;

	gboolean check_pb = image_win_check_pixbuf ( path_new );

	if ( check_pb )
	{
		gtk_widget_set_visible ( GTK_WIDGET ( win->swin_img ), TRUE  );
		gtk_widget_set_visible ( GTK_WIDGET ( win->swin_prw ), FALSE );

		if ( win->file ) g_object_unref ( win->file );

		win->file = g_file_parse_name ( path_new );

		image_win_set_image ( win );
	}
}

static void image_win_left ( ImageWin *win )
{
	win->pb_type_lr = ( win->pb_type_lr == POR ) ? PLT : POR;

	image_win_set_image_vhlr ( PLT, win );
}

static void image_win_right ( ImageWin *win )
{
	win->pb_type_lr = ( win->pb_type_lr == POR ) ? PRT : POR;

	image_win_set_image_vhlr ( PRT, win );
}

static void image_win_vertical ( ImageWin *win )
{
	win->pb_type_hv = ( win->pb_type_hv == POR ) ? PVT : POR;

	image_win_set_image_vhlr ( PVT, win );
}

static void image_win_horizont ( ImageWin *win )
{
	win->pb_type_hv = ( win->pb_type_hv == POR ) ? PHR : POR;

	image_win_set_image_vhlr ( PHR, win );
}

static void image_win_inp ( ImageWin *win )
{
	image_win_set_image_plus_minus ( TRUE, win );
}

static void image_win_out ( ImageWin *win )
{
	image_win_set_image_plus_minus ( FALSE, win );
}

static void image_win_fit ( ImageWin *win )
{
	win->original = FALSE;

	image_win_set_image ( win );
}

static void image_win_org ( ImageWin *win )
{
	win->original = TRUE;

	image_win_set_image ( win );
}

static int image_sort_func_list ( gconstpointer a, gconstpointer b )
{
	g_autofree char *na = g_utf8_collate_key_for_filename ( a, -1 );
	g_autofree char *nb = g_utf8_collate_key_for_filename ( b, -1 );

	return g_strcmp0 ( na, nb );
}

static char * image_win_list_sort_get_data ( const char *path, gboolean reverse, GList *list )
{
	char *data = NULL;
	gboolean found = FALSE;

	GList *list_sort = g_list_sort ( list, image_sort_func_list );

	if ( reverse ) list_sort = g_list_reverse ( list_sort );

	char *data_0 = (char *)g_list_nth ( list_sort, 0 )->data;

	while ( list_sort != NULL )
	{
		data = (char *)list_sort->data;

		if ( !path || found ) break;

		if ( path && g_str_has_suffix ( path, data ) ) found = TRUE;

		list_sort = list_sort->next;
	}

	return ( list_sort ) ? data : data_0;
}

static void image_win_dir ( const char *dir_path, const char *path, gboolean reverse, ImageWin *win )
{
	GList *list = NULL;
	GDir *dir = g_dir_open ( dir_path, 0, NULL );

	if ( dir )
	{
		const char *name = NULL;

		while ( ( name = g_dir_read_name ( dir ) ) != NULL )
		{
			char *path_name = g_strconcat ( dir_path, "/", name, NULL );

			if ( path_name && g_file_test ( path_name, G_FILE_TEST_IS_REGULAR ) )
			{
				list = g_list_append ( list, g_strdup ( path_name ) );
			}

			free ( path_name );
		}

		g_dir_close ( dir );
	}
	else
	{
		g_critical ( "%s: opening directory %s failed.", __func__, dir_path );
	}

	if ( list )
	{
		uint num = g_list_length ( list );

		if ( num > 1 )
		{
			char *path_new = image_win_list_sort_get_data ( path, reverse, list );

			GFile *file = g_file_parse_name ( path_new );

			if ( file ) image_set_file ( file, win );

			if ( file ) g_object_unref ( file );
		}
	}

	g_list_free_full ( list, (GDestroyNotify) g_free );
}

static void image_win_back ( ImageWin *win )
{
	g_autofree char *path = g_file_get_path ( win->file );

	g_autofree char *dir = g_path_get_dirname ( path );

	image_win_dir ( dir, path, TRUE, win );
}

static void image_win_forward ( ImageWin *win )
{
	g_autofree char *path = g_file_get_path ( win->file );

	g_autofree char *dir = g_path_get_dirname ( path );

	image_win_dir ( dir, path, FALSE, win );
}

static void image_win_stop ( ImageWin *win )
{
	GtkImage *image = (GtkImage *)gtk_button_get_image ( win->button_play );
	gtk_image_set_from_icon_name ( image, "media-playback-start", GTK_ICON_SIZE_MENU );

	if ( win->src_play ) g_source_remove ( win->src_play );

	win->src_play = 0;
}

static gboolean image_win_time_autoplay ( ImageWin *win )
{
	gboolean vis = gtk_widget_get_visible ( GTK_WIDGET ( win->swin_prw ) );

	if ( vis ) { image_win_stop ( win ); return FALSE; }

	image_win_forward ( win );

	return TRUE;
}

static void image_win_run_autoplay ( ImageWin *win )
{
	win->src_play = g_timeout_add_seconds ( win->timeout, (GSourceFunc)image_win_time_autoplay, win );
}

static void image_win_play ( ImageWin *win )
{
	if ( win->src_play )
		image_win_stop ( win );
	else
	{
		gtk_popover_set_relative_to ( win->popover_time, GTK_WIDGET ( win->button_play ) );
		gtk_popover_popup ( win->popover_time );
	}
}

static void image_win_info ( ImageWin *win )
{
	win_about ( GTK_WINDOW ( win ) );
}

static void image_win_up ( ImageWin *win )
{
	gboolean vis = gtk_widget_get_visible ( GTK_WIDGET ( win->swin_prw ) );

	GFile *dir_file = ( vis ) ? g_file_get_parent ( win->dir ) : g_file_get_parent ( win->file );

	if ( dir_file ) win_set_dir_file ( dir_file, win );

	if ( dir_file ) g_object_unref ( dir_file );
}

static void image_win_prw_pl_mn ( gboolean pl_mn, ImageWin *win )
{
	uint16_t c = 0; for ( c = 0; c < SISE_N; c++ )
	{
		if ( win->icon_size == icon_descr_size_n[c].size )
		{
			if (  pl_mn ) win->icon_size = icon_descr_size_n[c+1].size;
			if ( !pl_mn ) win->icon_size = icon_descr_size_n[c-1].size;

			if (  pl_mn && c+1 >= SISE_N ) win->icon_size = 256;
			if ( !pl_mn && c-1 <=      0 ) win->icon_size =  24;

			break;
		}
	}

	win_set_dir_file ( win->dir, win );
}

static void image_win_prw_mn ( ImageWin *win )
{
	image_win_prw_pl_mn ( FALSE, win );
}

static void image_win_prw_pl ( ImageWin *win )
{
	image_win_prw_pl_mn ( TRUE, win );
}

static void image_win_prw_ia ( GtkButton *button, ImageWin *win )
{
	win->preview = !win->preview;

	GtkImage *image = (GtkImage *)gtk_button_get_image ( button );
	gtk_image_set_from_icon_name ( image, ( win->preview ) ? "desktop" : "folder", GTK_ICON_SIZE_MENU );

	win_set_dir_file ( win->dir, win );
}

static void image_win_remove ( ImageWin *win )
{
	GFile *file = g_file_dup ( win->file );

	image_win_forward ( win );

	GError *error = NULL;
	g_file_trash ( file, NULL, &error );

	if ( error ) { dialog_message ( " ", error->message, GTK_MESSAGE_ERROR, GTK_WINDOW ( win ) ); g_error_free ( error ); }

	g_object_unref ( file );
}

static void image_win_bar_signal_all_buttons ( GtkButton *button, ImageWin *win )
{
	const char *name = gtk_widget_get_name ( GTK_WIDGET ( button ) );

	uint8_t num = ( uint8_t )( atoi ( name ) );

	if ( num == BUP ) { image_win_up   ( win ); return; }
	if ( num == BIF ) { image_win_info ( win ); return; }

	gboolean vis = gtk_widget_get_visible ( GTK_WIDGET ( win->swin_prw ) );

	if ( vis && num == BMN ) { image_win_prw_mn ( win ); return; }
	if ( vis && num == BPL ) { image_win_prw_pl ( win ); return; }

	if ( vis && num == BIA ) { image_win_prw_ia ( button, win ); return; }

	if ( vis ) return;

	fp funcs[] = { NULL, image_win_back, image_win_forward, image_win_play, image_win_left, image_win_right, image_win_vertical, image_win_horizont, 
		image_win_remove, NULL, NULL, image_win_fit, image_win_org, image_win_out, image_win_inp };

	if ( funcs[num] ) funcs[num] ( win );
}

static void image_win_bar_create ( GtkBox *box, ImageWin *win )
{
	win->bar_label = (GtkLabel *)gtk_label_new ( " " );
	gtk_widget_set_visible (  GTK_WIDGET ( win->bar_label ), TRUE );

	GtkButton *button[BAL];

	uint8_t c = 0; for ( c = 0; c < BAL; c++ )
	{
		button[c] = (GtkButton *)gtk_button_new_from_icon_name ( button_icon_n[c].icon, GTK_ICON_SIZE_MENU );

		gtk_button_set_relief ( button[c], GTK_RELIEF_NONE );
		gtk_widget_set_visible ( GTK_WIDGET ( button[c] ), TRUE );
		if ( c < BIF ) gtk_box_pack_start ( box, GTK_WIDGET ( button[c] ), FALSE, FALSE, 0 ); else gtk_box_pack_end ( box, GTK_WIDGET ( button[c] ), FALSE, FALSE, 0 );

		char buf[20];
		sprintf ( buf, "%d", c );
		gtk_widget_set_name ( GTK_WIDGET ( button[c] ), buf );

		g_signal_connect ( button[c], "clicked", G_CALLBACK ( image_win_bar_signal_all_buttons ), win );

		if ( c == BST ) win->button_play = button[c];
		if ( c == BIF ) gtk_box_pack_start ( box, GTK_WIDGET ( win->bar_label ), TRUE, TRUE, 0 );
	}
}

static gboolean image_win_fullscreen ( ImageWin *win )
{
	GdkWindowState state = gdk_window_get_state ( gtk_widget_get_window ( GTK_WIDGET ( win ) ) );

	if ( state & GDK_WINDOW_STATE_FULLSCREEN )
		{ gtk_window_unfullscreen ( GTK_WINDOW ( win ) ); return FALSE; }
	else
		{ gtk_window_fullscreen   ( GTK_WINDOW ( win ) ); return TRUE;  }

	return TRUE;
}

static gboolean image_win_time_update ( ImageWin *win )
{
	image_win_set_image ( win );

	return FALSE;
}

static gboolean image_win_press_event ( G_GNUC_UNUSED GtkScrolledWindow *swin, GdkEventButton *event, ImageWin *win )
{
	gboolean vis = gtk_widget_get_visible ( GTK_WIDGET ( win->swin_prw ) );

	if ( vis ) return GDK_EVENT_STOP;

	if ( event->button == GDK_BUTTON_PRIMARY )
	{
		win->ah_val = gtk_adjustment_get_value ( win->adjh ) + event->x;
		win->av_val = gtk_adjustment_get_value ( win->adjv ) + event->y;

		if ( event->type == GDK_2BUTTON_PRESS )
		{
			image_win_fullscreen ( win );

			g_timeout_add ( 250, (GSourceFunc)image_win_time_update, win );
		}
	}

	if ( event->button == GDK_BUTTON_MIDDLE )
	{
		win->original = !win->original;

		image_win_set_image ( win );
	}

	if ( event->button == GDK_BUTTON_SECONDARY )
	{
		gboolean set = gtk_widget_get_visible ( GTK_WIDGET ( win->bar_box ) );

		gtk_widget_set_visible ( GTK_WIDGET ( win->bar_box ), !set );

		image_win_set_image ( win );
	}

	return GDK_EVENT_STOP;
}

static gboolean image_win_release_event ( GtkScrolledWindow *swin, G_GNUC_UNUSED GdkEventButton *event, G_GNUC_UNUSED ImageWin *win )
{
	gdk_window_set_cursor ( gtk_widget_get_window ( GTK_WIDGET ( swin ) ), NULL );

	return GDK_EVENT_STOP;
}

static gboolean image_win_notify_event ( GtkScrolledWindow *swin, GdkEventMotion *event, ImageWin *win )
{
	if ( event->state & GDK_BUTTON1_MASK )
	{
		gdk_window_set_cursor ( gtk_widget_get_window ( GTK_WIDGET ( swin ) ), win->cursor );

		gtk_adjustment_set_value ( win->adjh, win->ah_val - event->x );
		gtk_adjustment_set_value ( win->adjv, win->av_val - event->y );
	}

	return GDK_EVENT_STOP;
}

static gboolean image_win_config_timeout ( ImageWin *win )
{
	win->config = TRUE;

	image_win_set_image ( win );

	return FALSE;
}

static gboolean image_win_config_event ( G_GNUC_UNUSED GtkWindow *window, G_GNUC_UNUSED GdkEventConfigure *event, ImageWin *win )
{
	gboolean vis = gtk_widget_get_visible ( GTK_WIDGET ( win->swin_img ) );

	if ( vis && win->config ) { win->config = FALSE; g_timeout_add ( 200, (GSourceFunc)image_win_config_timeout, win ); }

	return GDK_EVENT_PROPAGATE;
}

static gboolean image_win_scroll_event ( G_GNUC_UNUSED GtkWindow *window, GdkEventScroll *evscroll, ImageWin *win )
{
	gboolean vis = gtk_widget_get_visible ( GTK_WIDGET ( win->swin_img ) );

	if ( !vis ) return GDK_EVENT_STOP;

	if ( evscroll->direction == GDK_SCROLL_UP   ) image_win_back ( win );
	if ( evscroll->direction == GDK_SCROLL_DOWN ) image_win_forward ( win );

	return GDK_EVENT_STOP;
}

static int icon_sort_func_az ( GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, G_GNUC_UNUSED gpointer data )
{
	int ret = 1;
	gboolean is_dir_a, is_dir_b;

	g_autofree char *name_a = NULL;
	g_autofree char *name_b = NULL;

	gtk_tree_model_get ( model, a, COL_IS_DIR, &is_dir_a, COL_NAME, &name_a, -1 );
	gtk_tree_model_get ( model, b, COL_IS_DIR, &is_dir_b, COL_NAME, &name_b, -1 );

	if ( !is_dir_a && is_dir_b )
		ret = 1;
	else if ( is_dir_a && !is_dir_b )
		ret = -1;
	else
	{
		g_autofree char *na = g_utf8_collate_key_for_filename ( name_a, -1 );
		g_autofree char *nb = g_utf8_collate_key_for_filename ( name_b, -1 );

		ret = g_strcmp0 ( na, nb );
	}

	return ret;
}

static GtkTreeModel * icon_create_model ( void )
{
	GtkListStore *store = gtk_list_store_new ( NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, GDK_TYPE_PIXBUF );

	gtk_tree_sortable_set_default_sort_func ( GTK_TREE_SORTABLE ( store ), icon_sort_func_az, NULL, NULL );
	gtk_tree_sortable_set_sort_column_id ( GTK_TREE_SORTABLE (store), GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	return GTK_TREE_MODEL ( store );
}

static GtkIconView * icon_view_create ( void )
{
	GtkIconView *icon_view = (GtkIconView *)gtk_icon_view_new ();
	gtk_widget_set_visible ( GTK_WIDGET ( icon_view ), TRUE );

	GtkTreeModel *model = icon_create_model ();
	gtk_icon_view_set_model ( icon_view, model );

	g_object_unref ( model );

	gtk_icon_view_set_item_width    ( icon_view, ITEM_WIDTH );
	gtk_icon_view_set_text_column   ( icon_view, COL_NAME   );
	gtk_icon_view_set_pixbuf_column ( icon_view, COL_PIXBUF );

	gtk_icon_view_set_selection_mode ( icon_view, GTK_SELECTION_MULTIPLE );

	return icon_view;
}

static inline GIcon * icon_emblemed_icon ( const char *name_1, const char *name_2, GIcon *gicon )
{
	GIcon *e_icon = g_themed_icon_new ( name_1 );
	GEmblem *emblem  = g_emblem_new ( e_icon );

	GIcon *emblemed  = g_emblemed_icon_new ( gicon, emblem );

	if ( name_2 )
	{
		GIcon *e_icon_2 = g_themed_icon_new ( name_2 );
		GEmblem *emblem_2  = g_emblem_new ( e_icon_2 );

		g_emblemed_icon_add_emblem ( G_EMBLEMED_ICON ( emblemed ), emblem_2 );

		g_object_unref ( e_icon_2 );
		g_object_unref ( emblem_2 );
	}

	g_object_unref ( e_icon );
	g_object_unref ( emblem );

	return emblemed;
}

static inline GdkPixbuf * icon_image_get_pixbuf ( const char *path, gboolean is_link, uint16_t icon_size, GFile *file )
{
	GdkPixbuf *pixbuf = NULL;

	if ( is_link )
	{
		GIcon *gicon = g_file_icon_new ( file ), *emblemed = icon_emblemed_icon ( "emblem-symbolic-link", NULL, gicon );
		GtkIconInfo *icon_info = gtk_icon_theme_lookup_by_gicon ( gtk_icon_theme_get_default (), emblemed, icon_size, GTK_ICON_LOOKUP_FORCE_SIZE );

		if ( icon_info ) pixbuf = gtk_icon_info_load_icon ( icon_info, NULL );

		if ( gicon ) g_object_unref ( gicon );
		if ( emblemed ) g_object_unref ( emblemed );
		if ( icon_info ) g_object_unref ( icon_info );
	}
	else
		pixbuf = gdk_pixbuf_new_from_file_at_size ( path, icon_size, icon_size, NULL );

	return pixbuf;
}

static inline GtkIconInfo * icon_get_icon_info ( const char *content_type, gboolean is_link, uint16_t icon_size, GFileInfo *finfo )
{
	GtkIconInfo *icon_info = NULL;

	GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();
	GIcon *unknown = NULL, *emblemed = NULL, *gicon = g_file_info_get_icon ( finfo );

	if ( gicon )
	{
		if ( is_link ) emblemed = icon_emblemed_icon ( "emblem-symbolic-link", NULL, gicon );

		icon_info = gtk_icon_theme_lookup_by_gicon ( icon_theme, ( is_link ) ? emblemed : gicon, icon_size, GTK_ICON_LOOKUP_FORCE_REGULAR );
	}

	if ( !icon_info && is_link )
	{
		unknown = g_themed_icon_new ( "unknown" );

		if ( emblemed ) g_object_unref ( emblemed );

		if ( content_type && g_str_has_prefix ( content_type, "inode/symlink" ) ) 
			emblemed = icon_emblemed_icon ( "dialog-error", "emblem-symbolic-link", unknown );
		else
			emblemed = icon_emblemed_icon ( "emblem-symbolic-link", NULL, unknown );

		icon_info = gtk_icon_theme_lookup_by_gicon ( icon_theme, emblemed, icon_size, GTK_ICON_LOOKUP_FORCE_REGULAR );
	}

	if ( unknown ) g_object_unref ( unknown );
	if ( emblemed ) g_object_unref ( emblemed );

	return icon_info;
}

static GdkPixbuf * icon_get_pixbuf ( const char *path, gboolean is_link, uint16_t icon_size )
{
	GdkPixbuf *pixbuf = NULL;

	GFile *file = g_file_new_for_path ( path );
	GFileInfo *finfo = g_file_query_info ( file, "*", 0, NULL, NULL );

	gboolean is_dir = g_file_test ( path, G_FILE_TEST_IS_DIR );
	const char *content_type = ( finfo ) ? g_file_info_get_content_type ( finfo ) : NULL;

	if ( content_type && g_str_has_prefix ( content_type, "image" ) ) pixbuf = icon_image_get_pixbuf ( path, is_link, icon_size, file );

	if ( finfo && !pixbuf )
	{
		GtkIconInfo *icon_info = icon_get_icon_info ( content_type, is_link, icon_size, finfo );

		if ( icon_info ) pixbuf = gtk_icon_info_load_icon ( icon_info, NULL );

		if ( icon_info ) g_object_unref ( icon_info );
	}

	if ( finfo ) g_object_unref ( finfo );
	if ( file  ) g_object_unref ( file  );

	if ( !pixbuf ) pixbuf = gtk_icon_theme_load_icon ( gtk_icon_theme_get_default (), ( is_dir ) ? "folder" : "unknown", icon_size, GTK_ICON_LOOKUP_FORCE_REGULAR, NULL );

	return pixbuf;
}

static void icon_set_pixbuf ( uint16_t icon_size, GtkTreeIter iter, GtkTreeModel *model )
{
	gboolean is_link = FALSE;
	g_autofree char *path = NULL;
	gtk_tree_model_get ( model, &iter, COL_PATH, &path, COL_IS_LINK, &is_link, -1 );

	if ( g_file_test ( path, G_FILE_TEST_EXISTS ) )
	{
		GdkPixbuf *pixbuf = icon_get_pixbuf ( path, is_link, icon_size );

		if ( pixbuf ) gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_IS_PIXBUF, TRUE, COL_PIXBUF, pixbuf, -1 );

		if ( pixbuf ) g_object_unref ( pixbuf );
	}
}

static gpointer icon_update_pixbuf_thread ( ImageWin *win )
{
	GtkTreeModel *model = win->model_t;

	gboolean break_t = FALSE;
	uint64_t num = 0, end = win->end_t;
	uint16_t icon_size = win->icon_size;
	uint8_t mod = win->mod_t, limit = win->limit_t;

	GtkTreeIter iter;
	gboolean valid = FALSE;

	for ( valid = gtk_tree_model_get_iter_first ( model, &iter ); valid; valid = gtk_tree_model_iter_next ( model, &iter ) )
	{
		G_LOCK ( done_th );
			if ( win->break_t ) break_t = TRUE;
		G_UNLOCK ( done_th );

		if ( break_t ) return NULL;

		if ( ( !limit && num > end ) || ( limit && num < end ) ) { num++; continue; }

		if (  mod && !( num % 2 ) ) { num++; continue; }
		if ( !mod &&  ( num % 2 ) ) { num++; continue; }

		gboolean is_pb = FALSE;
		gtk_tree_model_get ( model, &iter, COL_IS_PIXBUF, &is_pb, -1 );

		if ( !is_pb ) icon_set_pixbuf ( icon_size, iter, model );

		num++;
	}

	G_LOCK ( done_th );

	if ( !mod && !limit ) win->done_t_0 = TRUE;
	if (  mod && !limit ) win->done_t_1 = TRUE;
	if ( !mod &&  limit ) win->done_t_2 = TRUE;
	if (  mod &&  limit ) win->done_t_3 = TRUE;

	G_UNLOCK ( done_th );

	return NULL;
}

static gboolean icon_update_pixbuf_timeout ( ImageWin *win )
{
	if ( !GTK_IS_WIDGET ( win->icon_view ) ) return FALSE;

	if ( win->break_t ) { g_usleep ( 100000 ); g_object_unref ( win->model_t ); win->model_t = NULL; return FALSE; }

	if ( win->done_t_0 && win->done_t_1 && win->done_t_2 && win->done_t_3 )
	{
		GtkTreeModel *model_old = gtk_icon_view_get_model ( win->icon_view );

		gtk_list_store_clear ( GTK_LIST_STORE ( model_old ) );

		gtk_icon_view_set_model ( win->icon_view, win->model_t );

		g_object_unref ( win->model_t );
		win->model_t = NULL;

		return FALSE;
	}

	return TRUE;
}

static void icon_update_pixbuf_all ( uint nums, ImageWin *win )
{
	win->break_t = FALSE;

	win->mod_t = 0; win->limit_t = 0; win->done_t_0 = FALSE; win->end_t = nums / 2;
	GThread *thread_0_0 = g_thread_new ( NULL, (GThreadFunc)icon_update_pixbuf_thread, win );

	g_usleep ( 20000 );
	win->mod_t = 1; win->limit_t = 0; win->done_t_1 = FALSE;
	GThread *thread_1_0 = g_thread_new ( NULL, (GThreadFunc)icon_update_pixbuf_thread, win );

	g_usleep ( 20000 );
	win->mod_t = 0; win->limit_t = 1; win->done_t_2 = FALSE;
	GThread *thread_0_1 = g_thread_new ( NULL, (GThreadFunc)icon_update_pixbuf_thread, win );

	g_usleep ( 20000 );
	win->mod_t = 1; win->limit_t = 1; win->done_t_3 = FALSE;
	GThread *thread_1_1 = g_thread_new ( NULL, (GThreadFunc)icon_update_pixbuf_thread, win );

	g_thread_unref ( thread_0_0 );
	g_thread_unref ( thread_1_0 );
	g_thread_unref ( thread_0_1 );
	g_thread_unref ( thread_1_1 );

	g_timeout_add ( 50, (GSourceFunc)icon_update_pixbuf_timeout, win );
}

static uint16_t icon_get_vis_items ( ImageWin *win )
{
	int w = gtk_widget_get_allocated_width  ( GTK_WIDGET ( win->icon_view ) );
	int h = gtk_widget_get_allocated_height ( GTK_WIDGET ( win->icon_view ) );

	int item_col_s = gtk_icon_view_get_column_spacing ( win->icon_view );
	int item_width = gtk_icon_view_get_item_width ( win->icon_view );

	uint16_t ch = (uint16_t)( h / ( item_width - 20 ) );
	uint16_t cw = (uint16_t)( w / ( item_width + item_col_s ) );

	uint16_t items = (uint16_t)( cw * ch );

	items *= 2;

	return items;
}

static void icon_model_set_iter ( const char *path, const char *name, gboolean is_dir, gboolean is_slk, gboolean is_pbf, GdkPixbuf *pixbuf, GtkTreeModel *model )
{
	GtkTreeIter iter;
	gtk_list_store_append ( GTK_LIST_STORE ( model ), &iter );

	gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter,
		COL_PATH, path,
		COL_NAME, name,
		COL_IS_DIR, is_dir,
		COL_IS_LINK, is_slk,
		COL_IS_PIXBUF, is_pbf,
		COL_PIXBUF, pixbuf,
		-1 );
}

static int icon_sort_func_list ( gconstpointer a, gconstpointer b )
{
	int ret = 1;

	gboolean is_dir_a = g_file_test ( a, G_FILE_TEST_IS_DIR );
	gboolean is_dir_b = g_file_test ( b, G_FILE_TEST_IS_DIR );
	
	if ( !is_dir_a && is_dir_b )
		ret = 1;
	else if ( is_dir_a && !is_dir_b )
		ret = -1;
	else
	{
		g_autofree char *na = g_utf8_collate_key_for_filename ( a, -1 );
		g_autofree char *nb = g_utf8_collate_key_for_filename ( b, -1 );

		ret = g_strcmp0 ( na, nb );
	}

	return ret;
}

static void icon_open_dir ( const char *path_dir, ImageWin *win )
{
	g_return_if_fail ( path_dir != NULL );

	GDir *dir = g_dir_open ( path_dir, 0, NULL );

	if ( !dir ) { dialog_message ( "", g_strerror ( errno ), GTK_MESSAGE_WARNING, GTK_WINDOW ( win ) ); return; }

	GList *list = NULL;
	const char *name = NULL;

	while ( ( name = g_dir_read_name (dir) ) ) list = g_list_append ( list, g_build_filename ( path_dir, name, NULL ) );

	g_dir_close ( dir );

	GList *list_sort = g_list_sort ( list, icon_sort_func_list );

	win->model_t = ( win->preview ) ? icon_create_model () : NULL;

	uint nums = 0;
	uint16_t items = icon_get_vis_items ( win );
	GtkIconTheme *itheme = gtk_icon_theme_get_default ();
	GtkTreeModel *model  = gtk_icon_view_get_model ( win->icon_view );

	gtk_list_store_clear ( GTK_LIST_STORE ( model ) );

	while ( list_sort != NULL )
	{
		char *path = (char *)list_sort->data;
		char *name = g_path_get_basename ( path );
		char *display_name = g_filename_to_utf8 ( name, -1, NULL, NULL, NULL );

		if ( name[0] != '.' )
		{
			gboolean is_dir = g_file_test ( path, G_FILE_TEST_IS_DIR );
			gboolean is_slk = g_file_test ( path, G_FILE_TEST_IS_SYMLINK );

			if ( win->preview )
			{
				if ( nums < items )
				{
					GdkPixbuf *pixbuf = icon_get_pixbuf ( path, is_slk, win->icon_size );

					icon_model_set_iter ( path, display_name, is_dir, is_slk, TRUE, pixbuf, model );
					icon_model_set_iter ( path, display_name, is_dir, is_slk, TRUE, pixbuf, win->model_t );

					if ( pixbuf ) g_object_unref ( pixbuf );
				}
				else
					icon_model_set_iter ( path, display_name, is_dir, is_slk, FALSE, NULL, win->model_t );
			}
			else
			{
				GdkPixbuf *pixbuf = gtk_icon_theme_load_icon ( itheme, ( is_dir ) ? "folder" : "text-x-preview", win->icon_size, GTK_ICON_LOOKUP_FORCE_REGULAR, NULL );

				icon_model_set_iter ( path, display_name, is_dir, is_slk, TRUE, pixbuf, model );

				if ( pixbuf ) g_object_unref ( pixbuf );
			}

			nums++;
		}

		free ( name );
		free ( display_name );

		list_sort = list_sort->next;
	}

	g_list_free_full ( list, (GDestroyNotify) free );

	if ( win->preview && nums >= items ) icon_update_pixbuf_all ( nums, win ); else { if ( win->model_t ) g_object_unref ( win->model_t ); win->model_t = NULL; }

	gtk_icon_view_scroll_to_path ( win->icon_view, gtk_tree_path_new_first (), FALSE, 0, 0 );
}

static gboolean icon_open_dir_timeout ( ImageWin *win )
{
	if ( !GTK_IS_WIDGET ( win->icon_view ) ) return FALSE;

	if ( win->model_t == NULL )
	{
		g_autofree char *path = g_file_get_path ( win->dir );

		if ( path ) icon_open_dir ( path, win );

		return FALSE;
	}

	return TRUE;
}

static void icon_open_dir_tm ( ImageWin *win )
{
	win->break_t = TRUE;

	g_timeout_add ( 80, (GSourceFunc)icon_open_dir_timeout, win );
}

static void icon_set_dir ( GFile *file, ImageWin *win )
{
	g_autofree char *path = ( file ) ? g_file_get_path ( file ) : NULL;

	if ( path )
	{
		if ( win->dir ) g_object_unref ( win->dir );

		win->dir = g_file_parse_name ( path );
	}

	gtk_widget_set_visible ( GTK_WIDGET ( win->swin_img ), FALSE );
	gtk_widget_set_visible ( GTK_WIDGET ( win->swin_prw ), TRUE  );

	gtk_label_set_text ( win->bar_label, " " );

	icon_open_dir_tm ( win );
}

static void icon_item_activated ( GtkIconView *icon_view, GtkTreePath *tree_path, ImageWin *win )
{
	g_autofree char *path = NULL;

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_icon_view_get_model ( icon_view );

	gtk_tree_model_get_iter ( model, &iter, tree_path );
	gtk_tree_model_get ( model, &iter, COL_PATH, &path, -1 );

	GFile *file = g_file_parse_name ( path );

	win_set_dir_file ( file, win );

	if ( file ) g_object_unref ( file );
}

static gboolean icon_press_event ( UNUSED GtkIconView *icon_view, GdkEventButton *event, ImageWin *win )
{
	if ( event->button == GDK_BUTTON_MIDDLE ) image_win_up ( win );

	return GDK_EVENT_PROPAGATE;
}

static void win_set_dir_file ( GFile *file, ImageWin *win )
{
	if ( !file ) return;

	GFileType ftype = g_file_query_file_type ( file, G_FILE_QUERY_INFO_NONE, NULL );

	if ( ftype == G_FILE_TYPE_DIRECTORY )
		icon_set_dir ( file, win );
	else
		image_set_file ( file, win );
}

static void image_win_arg ( GFile *file, ImageWin *win )
{
	if ( !file ) win->dir = g_file_parse_name ( g_get_home_dir () );

	if ( file ) win_set_dir_file ( file, win ); else win_set_dir_file ( win->dir, win );
}

static void image_win_signal_drop ( UNUSED GtkWindow *window, GdkDragContext *ct, UNUSED int x, UNUSED int y, GtkSelectionData *sd, UNUSED uint i, guint32 t, ImageWin *win )
{
	char **uris = gtk_selection_data_get_uris ( sd );

	GFile *file = g_file_parse_name ( uris[0] );

	win_set_dir_file ( file, win );

	if ( file ) g_object_unref ( file );

	gtk_drag_finish ( ct, TRUE, FALSE, t );

	g_strfreev ( uris );
}

static void image_win_destroy ( UNUSED GtkWindow *window, ImageWin *win )
{
	gtk_icon_view_unselect_all ( win->icon_view );
}

static void image_win_create ( ImageWin *win )
{
	GtkWindow *window = GTK_WINDOW ( win );
	gtk_window_set_title ( window, "Image-Gtk" );
	gtk_window_set_default_size ( window, 900, 500 );
	gtk_window_set_icon_name ( window, "image" );
	g_signal_connect ( window, "destroy", G_CALLBACK ( image_win_destroy ), win );

	gtk_drag_dest_set ( GTK_WIDGET ( window ), GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY );
	gtk_drag_dest_add_uri_targets  ( GTK_WIDGET ( window ) );
	g_signal_connect ( window, "drag-data-received", G_CALLBACK ( image_win_signal_drop ), win );

	GtkBox *main_vbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL,   0 );
	gtk_widget_set_visible ( GTK_WIDGET ( main_vbox ), TRUE );

	gtk_widget_set_events ( GTK_WIDGET ( window ), GDK_SCROLL_MASK |  GDK_STRUCTURE_MASK );
	g_signal_connect ( window, "scroll-event",    G_CALLBACK ( image_win_scroll_event ), win );
	g_signal_connect ( window, "configure-event", G_CALLBACK ( image_win_config_event ), win );

	win->swin_img = (GtkScrolledWindow *)gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( win->swin_img, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );

	gtk_widget_set_events ( GTK_WIDGET ( win->swin_img ), GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK );
	g_signal_connect ( win->swin_img, "button-press-event",   G_CALLBACK ( image_win_press_event   ), win );
	g_signal_connect ( win->swin_img, "button-release-event", G_CALLBACK ( image_win_release_event ), win );
	g_signal_connect ( win->swin_img, "motion-notify-event",  G_CALLBACK ( image_win_notify_event  ), win );

	win->bar_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );

	win->popover_time = image_win_pp_timeout ( win );

	image_win_bar_create ( win->bar_box, win );

	gtk_widget_set_visible ( GTK_WIDGET ( win->bar_box ), TRUE );
	gtk_box_pack_start ( main_vbox, GTK_WIDGET ( win->bar_box ), FALSE, FALSE, 0 );

	win->adjv = gtk_scrolled_window_get_vadjustment ( win->swin_img );
	win->adjh = gtk_scrolled_window_get_hadjustment ( win->swin_img );

	win->image = (GtkImage *)gtk_image_new ();
	gtk_widget_set_visible ( GTK_WIDGET ( win->image ), TRUE );

	gtk_container_add ( GTK_CONTAINER ( win->swin_img ), GTK_WIDGET ( win->image ) );

	gtk_widget_set_visible ( GTK_WIDGET ( win->swin_img ), FALSE );
	gtk_box_pack_start ( main_vbox, GTK_WIDGET ( win->swin_img ), TRUE, TRUE, 0 );

	win->swin_prw = (GtkScrolledWindow *)gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( win->swin_prw, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );

	win->icon_view = icon_view_create ();
	g_signal_connect ( win->icon_view, "item-activated", G_CALLBACK ( icon_item_activated ), win );
	g_signal_connect ( win->icon_view, "button-press-event", G_CALLBACK ( icon_press_event ), win );

	gtk_container_add ( GTK_CONTAINER ( win->swin_prw ), GTK_WIDGET ( win->icon_view ) );

	gtk_widget_set_visible ( GTK_WIDGET ( win->swin_prw ), FALSE );
	gtk_box_pack_start ( main_vbox, GTK_WIDGET ( win->swin_prw ), TRUE, TRUE, 0 );

	gtk_container_add ( GTK_CONTAINER ( window ), GTK_WIDGET ( main_vbox ) );

	gtk_window_present ( GTK_WINDOW ( win ) );
}

static void image_win_init ( ImageWin *win )
{
	win->file = NULL;

	win->config   = TRUE;
	win->original = FALSE;

	win->pb_type_lr = POR;
	win->pb_type_hv = POR;

	win->timeout  = 5;
	win->src_play = 0;

	win->cursor = gdk_cursor_new_for_display ( gdk_display_get_default (), GDK_FLEUR );

	win->dir = NULL;

	win->model_t = NULL;

	win->preview = TRUE;
	win->icon_size = 48;

	image_win_create ( win );
}

static void image_win_finalize ( GObject *object )
{
	ImageWin *win = IMAGE_WIN ( object );

	if ( win->dir  ) g_object_unref ( win->dir  );

	G_OBJECT_CLASS ( image_win_parent_class )->finalize ( object );
}

static void image_win_class_init ( ImageWinClass *class )
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);

	oclass->finalize = image_win_finalize;
}

ImageWin * image_win_new ( GFile *file, ImageApp *app )
{
	ImageWin *win = g_object_new ( IMAGE_TYPE_WIN, "application", app, NULL );

	image_win_arg ( file, win );

	return win;
}
