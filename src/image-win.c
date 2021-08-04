/*
* Copyright 2021 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "image-win.h"
#include "preview.h"
#include "icon.h"
#include "bar.h"

struct _ImageWin
{
	GtkWindow parent_instance;

	GdkCursor *cursor;
	GtkAdjustment *adjv;
	GtkAdjustment *adjh;
	GtkScrolledWindow *swin;

	Bar *bar;
	Icon *icon;
	Preview *preview;

	char *path;

	int win_width; 
	int win_height;

	uint src_play;
	uint16_t timeout;

	double av_val;
	double ah_val;

	gboolean confg;
	gboolean release;
	gboolean press_event;
};

G_DEFINE_TYPE ( ImageWin, image_win, GTK_TYPE_WINDOW )

typedef void ( *fp ) ( ImageWin * );

static void image_win_fit ( ImageWin * );
static void image_win_org ( ImageWin * );
static void image_win_run_play ( ImageWin * );

static void image_win_changed_timeout ( GtkSpinButton *button, ImageWin *win )
{
	gtk_spin_button_update ( button );
	win->timeout = (uint16_t)gtk_spin_button_get_value_as_int ( button );
}

static GtkSpinButton * image_win_create_spinbutton ( uint16_t val, uint16_t min, uint16_t max, uint16_t step, const char *text )
{
	GtkSpinButton *spinbutton = (GtkSpinButton *)gtk_spin_button_new_with_range ( min, max, step );
	gtk_spin_button_set_value ( spinbutton, val );

	gtk_entry_set_icon_from_icon_name ( GTK_ENTRY ( spinbutton ), GTK_ENTRY_ICON_PRIMARY, "sleep" );
	gtk_entry_set_icon_tooltip_text   ( GTK_ENTRY ( spinbutton ), GTK_ENTRY_ICON_PRIMARY, text );

	return spinbutton;
}

static void image_win_timeout_destroy ( G_GNUC_UNUSED GtkWindow *window, ImageWin *win )
{
	image_win_run_play ( win );
}

static void image_win_timeout ( ImageWin *win )
{
	GtkWindow *window = (GtkWindow *)gtk_window_new ( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_title ( window, "" );
	gtk_window_set_modal ( window, TRUE );
	gtk_window_set_transient_for ( window, GTK_WINDOW ( win ) );
	gtk_window_set_icon_name ( window, "sleep" );
	gtk_window_set_default_size ( window, 200, -1 );
	gtk_window_set_position ( window, GTK_WIN_POS_CENTER_ON_PARENT );
	g_signal_connect ( window, "destroy", G_CALLBACK ( image_win_timeout_destroy ), win );

	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL,  10 );
	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 5 );

	gtk_widget_set_visible ( GTK_WIDGET ( v_box ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	GtkSpinButton *spinbutton = image_win_create_spinbutton ( win->timeout, 1, 3600, 1, "Seconds" );
	g_signal_connect ( spinbutton, "changed", G_CALLBACK ( image_win_changed_timeout ), win );

	gtk_widget_set_visible ( GTK_WIDGET ( spinbutton ), TRUE );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( spinbutton ), TRUE, TRUE, 0 );

	GtkButton *button = (GtkButton *)gtk_button_new_from_icon_name ( "window-close", GTK_ICON_SIZE_MENU );
	g_signal_connect_swapped ( button, "clicked", G_CALLBACK ( gtk_widget_destroy ), window );

	gtk_widget_set_visible ( GTK_WIDGET ( button ), TRUE );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( button ), FALSE, FALSE, 0 );

	gtk_box_pack_end ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	gtk_container_set_border_width ( GTK_CONTAINER ( v_box ), 10 );
	gtk_container_add   ( GTK_CONTAINER ( window ), GTK_WIDGET ( v_box ) );

	gtk_window_present ( window );

	gtk_widget_set_opacity ( GTK_WIDGET ( window ), gtk_widget_get_opacity ( GTK_WIDGET ( win ) ) );
}

static void image_win_set_path ( const char *path, ImageWin *win )
{
	if ( !path ) return;

	if ( win->path ) free ( win->path );
	win->path = g_strdup ( path );

	if ( path && g_file_test ( path, G_FILE_TEST_IS_DIR ) )
	{
		gtk_widget_set_visible ( GTK_WIDGET ( win->icon ), TRUE );
		gtk_widget_set_visible ( GTK_WIDGET ( win->swin ), FALSE );

		g_signal_emit_by_name ( win->bar, "bar-set-label", " " );
		g_signal_emit_by_name ( win->icon, "icon-set-path", win->path );

		gtk_window_set_title ( GTK_WINDOW ( win ), "Image-Gtk" );
	}
	else
	{
		gtk_widget_set_visible ( GTK_WIDGET ( win->icon ), FALSE );
		gtk_widget_set_visible ( GTK_WIDGET ( win->swin ), TRUE );

        	image_win_fit ( win );
	}
}

static void image_win_set_file ( gboolean org, ImageWin *win )
{
	int w = gtk_widget_get_allocated_width  ( GTK_WIDGET ( win ) );
	int h = gtk_widget_get_allocated_height ( GTK_WIDGET ( win ) );
	int bar_height = gtk_widget_get_allocated_height ( GTK_WIDGET ( win->bar ) );

	g_signal_emit_by_name ( win->preview, "preview-set-file", win->path, w, h - bar_height, org );
}

static int _sort_func_list ( gconstpointer a, gconstpointer b )
{
	return g_utf8_collate ( a, b );
}

static void image_list_sort ( GList *list, gboolean reverse, ImageWin *win )
{
	uint n = 0;
	gboolean found = FALSE;

	char *data = NULL, *first = NULL;

	GList *list_sort = g_list_sort ( list, _sort_func_list );

	if ( reverse ) list_sort = g_list_reverse ( list_sort );

	while ( list_sort != NULL )
	{
		data = (char *)list_sort->data;

		if ( !n ) first = data;

		if ( !win->path || found ) break;		

		if ( win->path && g_str_has_suffix ( win->path, data ) ) found = TRUE;

		n++;
		list_sort = list_sort->next;
	}

	if ( win->path ) free ( win->path );

	if ( found )
	{		
		if ( list_sort != NULL )
			win->path = g_strdup ( data );
		else
			win->path = g_strdup ( first );
	}
	else
	{
		win->path = g_strdup ( first );
	}

	GdkWindowState state = gdk_window_get_state ( gtk_widget_get_window ( GTK_WIDGET ( win ) ) );

	if ( state & GDK_WINDOW_STATE_FULLSCREEN ) image_win_org ( win ); else image_win_fit ( win );
}

static void image_win_dir ( const char *dir_path, gboolean reverse, ImageWin *win )
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

	if ( list ) image_list_sort ( list, reverse, win );

	g_list_free_full ( list, (GDestroyNotify) g_free );
}

static void image_win_fit ( ImageWin *win )
{
	g_return_if_fail ( win->path != NULL );

	image_win_set_file ( FALSE, win );
}

static void image_win_org ( ImageWin *win )
{
	g_return_if_fail ( win->path != NULL );

	image_win_set_file ( TRUE, win );
}

static void image_win_back ( ImageWin *win )
{
	g_return_if_fail ( win->path != NULL );

	g_autofree char *path = g_path_get_dirname ( win->path );

	image_win_dir ( path, TRUE, win );
}

static void image_win_forward ( ImageWin *win )
{
	g_return_if_fail ( win->path != NULL );

	g_autofree char *path = g_path_get_dirname ( win->path );

	image_win_dir ( path, FALSE, win );
}

static gboolean image_win_upd_play ( ImageWin *win )
{
	gboolean vis = gtk_widget_get_visible ( GTK_WIDGET ( win->icon ) );

	if ( vis )
	{
		g_signal_emit_by_name ( win->bar, "bar-set-stop" );
		win->src_play = 0;

		return FALSE;
	}

	image_win_forward ( win );

	return TRUE;
}

static void image_win_stop ( ImageWin *win )
{
	if ( win->src_play ) g_source_remove ( win->src_play );

	win->src_play = 0;
}

static void image_win_run_play ( ImageWin *win )
{
	if ( !win->path ) return;

	if ( win->src_play ) image_win_stop ( win );

	win->src_play = g_timeout_add_seconds ( win->timeout, (GSourceFunc)image_win_upd_play, win );
}

static void image_win_play ( ImageWin *win )
{
	image_win_timeout ( win );
}

static void image_win_left ( ImageWin *win )
{
	g_signal_emit_by_name ( win->preview, "preview-set-vhlr", PLT );
}

static void image_win_right ( ImageWin *win )
{
	g_signal_emit_by_name ( win->preview, "preview-set-vhlr", PRT );
}

static void image_win_vertical ( ImageWin *win )
{
	g_signal_emit_by_name ( win->preview, "preview-set-vhlr", PVT );
}

static void image_win_horizont ( ImageWin *win )
{
	g_signal_emit_by_name ( win->preview, "preview-set-vhlr", PHR );
}

static void image_win_out ( ImageWin *win )
{
	g_return_if_fail ( win->path != NULL );

	g_signal_emit_by_name ( win->preview, "preview-plus-minus", win->path, FALSE );
}

static void image_win_in ( ImageWin *win )
{
	g_return_if_fail ( win->path != NULL );

	g_signal_emit_by_name ( win->preview, "preview-plus-minus", win->path, TRUE );
}

static void image_win_up ( ImageWin *win )
{
	g_return_if_fail ( win->path != NULL );

	g_autofree char *path = g_path_get_dirname ( win->path );

	image_win_set_path ( path, win );
}

static gboolean image_win_upd_stop ( ImageWin *win )
{
	g_signal_emit_by_name ( win->bar, "bar-set-stop" );

	return FALSE;
}

static void image_win_icon_plus ( ImageWin *win )
{
	g_return_if_fail ( win->path != NULL );

	g_signal_emit_by_name ( win->icon, "icon-set-size", win->path, TRUE );
}

static void image_win_icon_minus ( ImageWin *win )
{
	g_signal_emit_by_name ( win->icon, "icon-set-size",  win->path, FALSE );
}

/*
static void image_win_pref ( ImageWin *win )
{
	g_message ( "%s:: ", __func__ );
}
*/

static void image_win_signal_name_bar ( G_GNUC_UNUSED Bar *bar, uint8_t num, ImageWin *win )
{
	if ( num == BUP ) image_win_up   ( win );

	gboolean vis = gtk_widget_get_visible ( GTK_WIDGET ( win->icon ) );

	if ( num == BPL && vis ) { image_win_icon_plus  ( win ); return; }
	if ( num == BMN && vis ) { image_win_icon_minus ( win ); return; }

	if ( num == BST && vis ) { g_timeout_add_seconds ( 1, (GSourceFunc)image_win_upd_stop, win ); return; }

	if ( !win->path || vis ) return;

	win->press_event = FALSE;

	fp funcs[] = { image_win_up, image_win_back, image_win_forward, image_win_play, image_win_stop, image_win_left, image_win_right, 
		image_win_vertical, image_win_horizont, /*image_win_pref, */image_win_fit, image_win_org, image_win_out, image_win_in };

	if ( funcs[num] ) funcs[num] ( win );
}

static void image_win_signal_drop ( G_GNUC_UNUSED GtkWindow *window, GdkDragContext *ct, G_GNUC_UNUSED int x, G_GNUC_UNUSED int y,
        GtkSelectionData *s_data, G_GNUC_UNUSED uint info, guint32 time, ImageWin *win )
{
	char **uris = gtk_selection_data_get_uris ( s_data );

        g_autofree char *path = g_filename_from_uri ( uris[0], NULL, NULL );

	image_win_set_path ( path, win );

	g_strfreev ( uris );

	gtk_drag_finish ( ct, TRUE, FALSE, time );
}

static gboolean image_win_fullscreen ( GtkWindow *window )
{
	GdkWindowState state = gdk_window_get_state ( gtk_widget_get_window ( GTK_WIDGET ( window ) ) );

	if ( state & GDK_WINDOW_STATE_FULLSCREEN )
		{ gtk_window_unfullscreen ( window ); return FALSE; }
	else
		{ gtk_window_fullscreen   ( window ); return TRUE;  }

	return TRUE;
}

static gboolean image_win_pe_update ( ImageWin *win )
{
	image_win_fit ( win );

	return FALSE;
}

static gboolean image_win_press_event ( GtkWindow *window, GdkEventButton *event, ImageWin *win )
{
	gboolean vis = gtk_widget_get_visible ( GTK_WIDGET ( win->icon ) );

	if ( !win->path || vis ) return FALSE;

	if ( event->button == 1 )
	{
		win->release = FALSE;

		win->ah_val = gtk_adjustment_get_value ( win->adjh ) + event->x;
		win->av_val = gtk_adjustment_get_value ( win->adjv ) + event->y;

		if ( !win->press_event ) { win->press_event = TRUE; return TRUE; }

		if ( event->type == GDK_2BUTTON_PRESS )
		{
			gboolean set = image_win_fullscreen ( GTK_WINDOW ( window ) );

			gtk_widget_set_visible ( GTK_WIDGET ( win->bar ), !set );

			( set ) ? image_win_org ( win ) : g_timeout_add ( 250, (GSourceFunc)image_win_pe_update, win );
		}
	}

	if ( event->button == 2 )
	{
		gboolean set = gtk_widget_get_visible ( GTK_WIDGET ( win->bar ) );

		gtk_widget_set_visible ( GTK_WIDGET ( win->bar ), !set );
	}


	return TRUE;
}

static gboolean image_win_release_event ( GtkWindow *window, G_GNUC_UNUSED GdkEventButton *event, ImageWin *win )
{
	gboolean vis = gtk_widget_get_visible ( GTK_WIDGET ( win->icon ) );

	if ( vis ) return FALSE;

	win->release = TRUE;

	gdk_window_set_cursor ( gtk_widget_get_window ( GTK_WIDGET ( window ) ), NULL );

	return FALSE;
}

static gboolean image_win_notify_event ( GtkWindow *window, GdkEventMotion *event, ImageWin *win )
{
	gboolean vis = gtk_widget_get_visible ( GTK_WIDGET ( win->icon ) );

	if ( vis ) return FALSE;

	if ( event->state & GDK_BUTTON1_MASK )
	{
		if ( win->release ) return FALSE;

		gdk_window_set_cursor ( gtk_widget_get_window ( GTK_WIDGET ( window ) ), win->cursor );

		gtk_adjustment_set_value ( win->adjh, win->ah_val - event->x );
		gtk_adjustment_set_value ( win->adjv, win->av_val - event->y );
	}

	return FALSE;
}

static gboolean image_win_scroll_event ( G_GNUC_UNUSED GtkWindow *window, GdkEventScroll *evscroll, ImageWin *win )
{
	gboolean vis = gtk_widget_get_visible ( GTK_WIDGET ( win->icon ) );

	if ( !win->path || vis ) return FALSE;

	if ( evscroll->direction == GDK_SCROLL_UP   ) image_win_forward ( win );
	if ( evscroll->direction == GDK_SCROLL_DOWN ) image_win_back ( win );

	return TRUE;
}

static gboolean image_win_cnfg_update ( ImageWin *win )
{
	GdkWindowState state = gdk_window_get_state ( gtk_widget_get_window ( GTK_WIDGET ( win ) ) );

	if ( !( state & GDK_WINDOW_STATE_FULLSCREEN ) ) image_win_fit ( win );

	win->confg = TRUE;

	return FALSE;
}

static gboolean image_win_config_event ( G_GNUC_UNUSED GtkWindow *window, G_GNUC_UNUSED GdkEventConfigure *event, ImageWin *win )
{
	if ( !win->path || !win->confg ) return FALSE;

	gboolean vis = gtk_widget_get_visible ( GTK_WIDGET ( win->icon ) );

	if ( vis ) return FALSE;

	if ( win->win_width == event->width && win->win_height == event->height ) return FALSE;

	g_timeout_add ( 100, (GSourceFunc)image_win_cnfg_update, win );

	win->confg = FALSE;

	win->win_width = event->width, win->win_height = event->height;

	return TRUE;
}

static gboolean image_win_key_event ( G_GNUC_UNUSED GtkWindow *window, GdkEventKey *event, ImageWin *win )
{
	gboolean vis = gtk_widget_get_visible ( GTK_WIDGET ( win->icon ) );

	if ( !win->path || vis ) return TRUE;

	if ( event->keyval == GDK_KEY_space ) { image_win_forward  ( win ); return TRUE; }
	if ( event->keyval == GDK_KEY_BackSpace ) { image_win_back ( win ); return TRUE; }

	if ( event->keyval == GDK_KEY_KP_1 || event->keyval == GDK_KEY_1 ) { image_win_org ( win ); return TRUE; }
	if ( event->keyval == GDK_KEY_KP_0 || event->keyval == GDK_KEY_0 ) { image_win_fit ( win ); return TRUE; }

	return FALSE;
}

static void image_win_signal_name_set_path ( G_GNUC_UNUSED Icon *icon, const char *path, ImageWin *win )
{
	image_win_set_path ( path, win );
}

static void image_win_signal_name_set_label ( G_GNUC_UNUSED Preview *preview, const char *text, ImageWin *win )
{
	g_signal_emit_by_name ( win->bar, "bar-set-label", text );
}

static void image_win_create ( ImageWin *win )
{
	GtkWindow *window = GTK_WINDOW ( win );
	gtk_window_set_title ( window, "Image-Gtk" );
	gtk_window_set_default_size ( window, 900, 500 );
	gtk_window_set_icon_name ( window, "terminal" );

	gtk_widget_set_events ( GTK_WIDGET ( window ), GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_KEY_PRESS_MASK | GDK_SCROLL_MASK );
	g_signal_connect ( window, "button-press-event",   G_CALLBACK ( image_win_press_event   ), win );
	g_signal_connect ( window, "button-release-event", G_CALLBACK ( image_win_release_event ), win );
	g_signal_connect ( window, "motion-notify-event",  G_CALLBACK ( image_win_notify_event  ), win );
	g_signal_connect ( window, "scroll-event",         G_CALLBACK ( image_win_scroll_event  ), win );
	g_signal_connect ( window, "configure-event",      G_CALLBACK ( image_win_config_event  ), win );
	g_signal_connect ( window, "key-press-event",      G_CALLBACK ( image_win_key_event     ), win );

	gtk_drag_dest_set ( GTK_WIDGET ( window ), GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY );
	gtk_drag_dest_add_uri_targets  ( GTK_WIDGET ( window ) );
	g_signal_connect ( window, "drag-data-received", G_CALLBACK ( image_win_signal_drop ), win );

	GtkBox *main_vbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL,   0 );
	gtk_widget_set_visible ( GTK_WIDGET ( main_vbox ), TRUE );

	win->bar = bar_new ();
	g_signal_connect ( win->bar, "bar-click-num", G_CALLBACK ( image_win_signal_name_bar ), win );

	win->swin = (GtkScrolledWindow *)gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( win->swin, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_widget_set_visible ( GTK_WIDGET ( win->swin ), TRUE );

	win->preview = preview_new ();
	g_signal_connect ( win->preview, "preview-set-label", G_CALLBACK ( image_win_signal_name_set_label ), win );

	win->icon = icon_new ();
	g_signal_connect ( win->icon, "image-set-path", G_CALLBACK ( image_win_signal_name_set_path ), win );
	gtk_widget_set_visible ( GTK_WIDGET ( win->icon ), FALSE );

	win->adjv = gtk_scrolled_window_get_vadjustment ( win->swin );
	win->adjh = gtk_scrolled_window_get_hadjustment ( win->swin );

	gtk_box_pack_start ( main_vbox, GTK_WIDGET ( win->bar ), FALSE, FALSE, 0 );

	gtk_container_add ( GTK_CONTAINER ( win->swin ), GTK_WIDGET ( win->preview ) );

	gtk_box_pack_start ( main_vbox, GTK_WIDGET ( win->swin ), TRUE, TRUE, 0 );
	gtk_box_pack_start ( main_vbox, GTK_WIDGET ( win->icon ), TRUE, TRUE, 0 );

	gtk_container_add ( GTK_CONTAINER ( window ), GTK_WIDGET ( main_vbox ) );

	gtk_window_present ( GTK_WINDOW ( win ) );
}

static void image_win_init ( ImageWin *win )
{
	win->path = NULL;
	win->confg = TRUE;
	win->release = FALSE;
	win->press_event = FALSE;

	win->timeout  = 5;
	win->src_play = 0;
	win->win_width  = 0;
	win->win_height = 0;

	win->cursor = gdk_cursor_new_for_display ( gdk_display_get_default (), GDK_FLEUR );

	image_win_create ( win );
}

static void image_win_finalize ( GObject *object )
{
	ImageWin *win = IMAGE_WIN ( object );

	free ( win->path );

	g_object_unref ( win->cursor );

	if ( win->src_play ) g_source_remove ( win->src_play );

	G_OBJECT_CLASS ( image_win_parent_class )->finalize ( object );
}

static void image_win_class_init ( ImageWinClass *class )
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);

	oclass->finalize = image_win_finalize;
}

ImageWin * image_win_new ( const char *path, ImageApp *app )
{
	ImageWin *win = g_object_new ( IMAGE_TYPE_WIN, "application", app, NULL );

	image_win_set_path ( ( path ) ? path : g_get_home_dir (), win );

	return win;
}
