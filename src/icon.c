/*
* Copyright 2021 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "icon.h"

#include <errno.h>

#define ICON_SIZE 128
#define ITEM_WIDTH 80

enum cols
{
	COL_PATH,
	COL_NAME,
	COL_IS_DIR,
	COL_IS_LINK,
	COL_IS_PIXBUF,
	COL_PIXBUF,
	NUM_COLS
};

struct _Icon
{
	GtkBox parent_instance;

	GtkIconView *icon_view;

	uint16_t size;
};

G_DEFINE_TYPE ( Icon, icon, GTK_TYPE_BOX )

static void icon_message_dialog ( const char *f_error, const char *file_or_info, GtkMessageType mesg_type, GtkWindow *window )
{
	GtkMessageDialog *dialog = ( GtkMessageDialog *)gtk_message_dialog_new ( window, GTK_DIALOG_MODAL, mesg_type, 
		GTK_BUTTONS_CLOSE, "%s\n%s", f_error, file_or_info );

	gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), "system-file-manager" );

	gtk_dialog_run     ( GTK_DIALOG ( dialog ) );
	gtk_widget_destroy ( GTK_WIDGET ( dialog ) );
}

static gboolean link_exists ( const char *path )
{
	gboolean link_exists = FALSE;

	g_autofree char *link = g_file_read_link ( path, NULL );

	if ( !link ) return FALSE;

	if ( !g_path_is_absolute ( link ) )
	{
		g_autofree char *dirname = g_path_get_dirname ( path );
		g_autofree char *real = g_build_filename ( dirname, link, NULL );

		link_exists = g_file_test ( real, G_FILE_TEST_EXISTS );
	}
	else
		link_exists = g_file_test ( link, G_FILE_TEST_EXISTS );

	return link_exists;
}

static GIcon * emblemed_icon ( const char *name, GIcon *gicon )
{
	GIcon *e_icon = g_themed_icon_new ( name );
	GEmblem *emblem  = g_emblem_new ( e_icon );

	GIcon *emblemed  = g_emblemed_icon_new ( gicon, emblem );

	g_object_unref ( e_icon );
	g_object_unref ( emblem );

	return emblemed;
}

static gboolean get_icon_names ( GIcon *icon )
{
	const char **names = g_themed_icon_get_names ( G_THEMED_ICON ( icon ) );

	gboolean has_icon = gtk_icon_theme_has_icon ( gtk_icon_theme_get_default (), names[0] );

	if ( !has_icon && names[1] != NULL ) has_icon = gtk_icon_theme_has_icon ( gtk_icon_theme_get_default (), names[1] );

	return has_icon;
}

static GdkPixbuf * file_get_pixbuf ( const char *path, gboolean is_dir, gboolean is_link, gboolean preview, int icon_size )
{
	g_return_val_if_fail ( path != NULL, NULL );

	if ( !g_file_test ( path, G_FILE_TEST_EXISTS ) ) return NULL;

	GdkPixbuf *pixbuf = NULL;

	GtkIconTheme *itheme = gtk_icon_theme_get_default ();

	GFile *file = g_file_new_for_path ( path );
	GFileInfo *finfo = g_file_query_info ( file, "*", 0, NULL, NULL );

	const char *mime_type = ( finfo ) ? g_file_info_get_content_type ( finfo ) : NULL;

	if ( mime_type && preview && g_str_has_prefix ( mime_type, "image" ) )
		pixbuf = gdk_pixbuf_new_from_file_at_size ( path, icon_size, icon_size, NULL );

	if ( pixbuf == NULL )
	{
		gboolean is_link_exist = ( is_link ) ? link_exists ( path ) : FALSE;

		GIcon *emblemed = NULL, *icon_set = ( finfo ) ? g_file_info_get_icon ( finfo ) : NULL;

		gboolean has_icon = ( icon_set ) ? get_icon_names ( icon_set ) : FALSE;

		if ( !has_icon ) icon_set = g_themed_icon_new ( ( is_dir ) ? "folder" : "unknown" );

		if ( is_link ) emblemed = emblemed_icon ( ( !is_link_exist ) ? "error" : "emblem-symbolic-link", icon_set );

		GtkIconInfo *icon_info = gtk_icon_theme_lookup_by_gicon ( itheme, ( is_link ) ? emblemed : icon_set, icon_size, GTK_ICON_LOOKUP_FORCE_SIZE );

		pixbuf = ( icon_info ) ? gtk_icon_info_load_icon ( icon_info, NULL ) : NULL;

		if ( emblemed ) g_object_unref ( emblemed );
		if ( !has_icon && icon_set ) g_object_unref ( icon_set );
		if ( icon_info ) g_object_unref ( icon_info );
	}

	if ( finfo ) g_object_unref ( finfo );
	if ( file  ) g_object_unref ( file  );

	return pixbuf;
}

static void icon_set_pixbuf ( GtkTreeIter iter, GtkTreeModel *model, Icon *icon )
{
	g_autofree char *path = NULL;
	gboolean is_dir = FALSE, is_link = FALSE;
	gtk_tree_model_get ( model, &iter, COL_IS_DIR, &is_dir, COL_IS_LINK, &is_link, COL_PATH, &path, -1 );

	if ( g_file_test ( path, G_FILE_TEST_EXISTS ) )
	{
		GdkPixbuf *pixbuf = file_get_pixbuf ( path, is_dir, is_link, TRUE, icon->size );

		if ( pixbuf ) gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_IS_PIXBUF, TRUE, COL_PIXBUF, pixbuf, -1 );

		if ( pixbuf ) g_object_unref ( pixbuf );
	}
}

static void icon_update_pixbuf ( Icon *icon )
{
	GtkTreeIter iter;
	GtkTreePath *start_path, *end_path;
	GtkTreeModel *model = gtk_icon_view_get_model ( icon->icon_view );

	if ( !gtk_icon_view_get_visible_range ( icon->icon_view, &start_path, &end_path ) ) return;

	gboolean valid = FALSE;
	int *ind_s = gtk_tree_path_get_indices ( start_path );
	int *ind_e = gtk_tree_path_get_indices ( end_path   );

	int w = gtk_widget_get_allocated_width  ( GTK_WIDGET ( icon->icon_view ) );
	int cw = (int)( ( w / ( ITEM_WIDTH * 1.8 ) ) + 1 );
	int num = 0, item_nums = ( ind_e[0] - ind_s[0] ); item_nums += cw; // ( cw * 2 );

	for ( valid = gtk_tree_model_get_iter ( model, &iter, start_path ); valid; valid = gtk_tree_model_iter_next ( model, &iter ) )
	{
		if ( num > item_nums ) break;

		gboolean is_pb = FALSE;
		gtk_tree_model_get ( model, &iter, COL_IS_PIXBUF, &is_pb, -1 );

		if ( !is_pb ) icon_set_pixbuf ( iter, model, icon );

		num++;
	}

	gtk_tree_path_free ( end_path );
	gtk_tree_path_free ( start_path );
}

static void icon_update_scrool ( Icon *icon )
{
	icon_update_pixbuf ( icon );
}

static void icon_open_location ( const char *path_dir, Icon *icon )
{
	g_return_if_fail ( path_dir != NULL );

	GDir *dir = g_dir_open ( path_dir, 0, NULL );

	if ( !dir )
	{
		GtkWidget *window = gtk_widget_get_toplevel ( GTK_WIDGET ( icon->icon_view ) );

		int s_errno = errno;
		icon_message_dialog ( "", g_strerror ( s_errno ), GTK_MESSAGE_WARNING, GTK_WINDOW ( window ) );

		return;
	}

	int w = gtk_widget_get_allocated_width  ( GTK_WIDGET ( icon->icon_view ) );
	int h = gtk_widget_get_allocated_height ( GTK_WIDGET ( icon->icon_view ) );

	uint16_t cw = (uint16_t)( ( w / ( ITEM_WIDTH * 1.8 ) ) + 1 );
	uint16_t ch = (uint16_t)( ( h / ( ITEM_WIDTH -  20 ) ) + 1 );
	uint16_t num = 0, item_nums = (uint16_t)(cw * ch * 2);

	const char *name = NULL;
	const char *icon_name_d = "folder";
	const char *icon_name_f = "text-x-preview"; // text-x-generic
	gboolean valid = FALSE;
	GtkIconTheme *itheme = gtk_icon_theme_get_default ();

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_icon_view_get_model ( icon->icon_view );
	gtk_list_store_clear ( GTK_LIST_STORE ( model ) );

	while ( ( name = g_dir_read_name (dir) ) )
	{
		if ( name[0] != '.' )
		{
			char *path = g_build_filename ( path_dir, name, NULL );
			char *display_name = g_filename_to_utf8 ( name, -1, NULL, NULL, NULL );

			gboolean is_dir = g_file_test ( path, G_FILE_TEST_IS_DIR );
			gboolean is_slk = g_file_test ( path, G_FILE_TEST_IS_SYMLINK );

			GdkPixbuf *pixbuf = gtk_icon_theme_load_icon ( itheme, ( is_dir ) ? icon_name_d : icon_name_f, icon->size, GTK_ICON_LOOKUP_FORCE_SIZE, NULL );

				gtk_list_store_append ( GTK_LIST_STORE ( model ), &iter );

				gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter,
					COL_PATH, path,
					COL_NAME, display_name,
					COL_IS_DIR, is_dir,
					COL_IS_LINK, is_slk,
					COL_IS_PIXBUF, FALSE,
					COL_PIXBUF, pixbuf,
					-1 );

			free ( path );
			free ( display_name );

			if ( pixbuf ) g_object_unref ( pixbuf );
		}
	}

	g_dir_close ( dir );

	for ( valid = gtk_tree_model_get_iter_first ( model, &iter ); valid; valid = gtk_tree_model_iter_next ( model, &iter ) )
	{
		if ( num > item_nums ) break;

		char *path = NULL;
		gboolean is_dir = FALSE, is_link = FALSE;
		gtk_tree_model_get ( model, &iter, COL_IS_DIR, &is_dir, COL_IS_LINK, &is_link, COL_PATH, &path, -1 );

		GdkPixbuf *pixbuf = file_get_pixbuf ( path, is_dir, is_link, TRUE, icon->size );

		if ( pixbuf ) gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_PIXBUF, pixbuf, COL_IS_PIXBUF, TRUE, -1 );

		num++;
		free ( path );
		if ( pixbuf ) g_object_unref ( pixbuf );
	}

	gtk_icon_view_scroll_to_path ( icon->icon_view, gtk_tree_path_new_first (), FALSE, 0, 0 );
}

static void icon_item_activated ( GtkIconView *icon_view, GtkTreePath *tree_path, Icon *icon )
{
	g_autofree char *path = NULL;

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_icon_view_get_model ( icon_view );

	gboolean is_dir;
	gtk_tree_model_get_iter ( model, &iter, tree_path );
	gtk_tree_model_get ( model, &iter, COL_PATH, &path, COL_IS_DIR, &is_dir, -1 );

	g_signal_emit_by_name ( icon, "image-set-path", path );
}

static gboolean icon_scroll_event ( G_GNUC_UNUSED GtkScrolledWindow *scw, G_GNUC_UNUSED GdkEventScroll *evscroll, Icon *icon )
{
	icon_update_scrool ( icon );

	return FALSE;
}

static void icon_scroll_changed ( G_GNUC_UNUSED GtkAdjustment *adj, Icon *icon )
{
	icon_update_scrool ( icon );
}

static int icon_view_sort_func_a_z ( GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, G_GNUC_UNUSED gpointer data )
{
	gboolean is_dir_a, is_dir_b;

	g_autofree char *name_a = NULL;
	g_autofree char *name_b = NULL;

	int ret = 1;

	gtk_tree_model_get ( model, a, COL_IS_DIR, &is_dir_a, COL_NAME, &name_a, -1 );
	gtk_tree_model_get ( model, b, COL_IS_DIR, &is_dir_b, COL_NAME, &name_b, -1 );

	if ( !is_dir_a && is_dir_b )
		ret = 1;
	else if ( is_dir_a && !is_dir_b )
		ret = -1;
	else
		ret = g_utf8_collate ( name_a, name_b );

	return ret;
}

static GtkListStore * icon_view_create_store ( void )
{
	GtkListStore *store = gtk_list_store_new ( NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, GDK_TYPE_PIXBUF );

	gtk_tree_sortable_set_default_sort_func ( GTK_TREE_SORTABLE (store), icon_view_sort_func_a_z, NULL, NULL );
	gtk_tree_sortable_set_sort_column_id ( GTK_TREE_SORTABLE (store), GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	return store;
}

static GtkIconView * icon_view_create ( void )
{
	GtkListStore *store = icon_view_create_store ();

	GtkIconView *icon_view = (GtkIconView *)gtk_icon_view_new ();

	gtk_icon_view_set_model ( icon_view, GTK_TREE_MODEL ( store ) );

	g_object_unref (store);

	gtk_icon_view_set_item_width    ( icon_view, ITEM_WIDTH );
	gtk_icon_view_set_text_column   ( icon_view, COL_NAME   );
	gtk_icon_view_set_pixbuf_column ( icon_view, COL_PIXBUF );

	return icon_view;
}

static void icon_create ( Icon *icon )
{
	GtkScrolledWindow *sw = (GtkScrolledWindow *)gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( sw, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_widget_set_visible ( GTK_WIDGET ( sw ), TRUE );

	icon->icon_view = icon_view_create ();
	gtk_widget_set_visible ( GTK_WIDGET ( icon->icon_view ), TRUE );
	g_signal_connect ( icon->icon_view, "item-activated", G_CALLBACK ( icon_item_activated ), icon );

	// icon_open_location ( g_get_home_dir (), icon );

	gtk_container_add ( GTK_CONTAINER ( sw ), GTK_WIDGET ( icon->icon_view ) );

	gtk_box_pack_start ( GTK_BOX ( icon ), GTK_WIDGET (sw), TRUE, TRUE, 0 );

	GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment ( sw );
	g_signal_connect ( adj, "value-changed", G_CALLBACK ( icon_scroll_changed ), icon );

	g_signal_connect ( sw, "scroll-event",  G_CALLBACK ( icon_scroll_event ), icon );
}

static void icon_signal_name_set_path ( Icon *icon, const char *path )
{
	icon_open_location ( path, icon );
}

static void icon_signal_name_set_size ( Icon *icon, const char *path, gboolean pm )
{
	// uint8_t step = 32;
	// uint16_t sz[] = { 32, 64, 128, 256, 512 };

	if (  pm ) icon->size = (uint16_t)( icon->size + 32 );
	if ( !pm ) icon->size = (uint16_t)( icon->size - 32 );

	if (  pm && icon->size >= 512 ) icon->size = 512;
	if ( !pm && icon->size <=  32 ) icon->size =  32;

	icon_open_location ( path, icon );
}

static void icon_init ( Icon *icon )
{
	GtkBox *box = GTK_BOX ( icon );
	gtk_orientable_set_orientation ( GTK_ORIENTABLE ( box ), GTK_ORIENTATION_HORIZONTAL );

	gtk_box_set_spacing ( box, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( box ), TRUE );

	icon->size = ICON_SIZE;

	icon_create ( icon );
	g_signal_connect ( icon, "icon-set-path", G_CALLBACK ( icon_signal_name_set_path ), NULL );
	g_signal_connect ( icon, "icon-set-size", G_CALLBACK ( icon_signal_name_set_size ), NULL );
}

static void icon_finalize ( GObject *object )
{
	G_OBJECT_CLASS ( icon_parent_class )->finalize ( object );
}

static void icon_class_init ( IconClass *class )
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);

	oclass->finalize = icon_finalize;

	g_signal_new ( "image-set-path", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING );
	g_signal_new ( "icon-set-path",  G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING );
	g_signal_new ( "icon-set-size",  G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_BOOLEAN );
}

Icon * icon_new ( void )
{
	return g_object_new ( ICON_TYPE_BOX, NULL );
}

