/*
* Copyright 2021 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "bar.h"

struct _Bar
{
	GtkBox parent_instance;

	GtkLabel *label;
	GtkButton *buttons[BAL];
};

G_DEFINE_TYPE ( Bar, bar, GTK_TYPE_BOX )

const char *icons[BAL] = 
{
	[BUP] = "up", 
	[BCK] = "back", 
	[BFW] = "forward", 
	[BST] = "media-playback-start", 
	[BSP] = "media-playback-stop", 
	[BLT] = "object-rotate-left", 
	[BRT] = "object-rotate-right", 
	[BVR] = "object-flip-vertical", 
	[BHR] = "object-flip-horizontal", 

	/*[BPR] = "gtk-preferences", */
	[BFT] = "zoom-fit-best", 
	[BOR] = "zoom-original", 
	[BMN] = "zoom-out", 
	[BPL] = "zoom-in"
};

static void bar_signal_name_set_label ( Bar *bar, const char *text )
{
	gtk_label_set_text ( bar->label, text );
}

static void bar_signal_all_buttons ( GtkButton *button, Bar *bar )
{
	const char *name  = gtk_widget_get_name ( GTK_WIDGET ( button ) );

	uint8_t num = ( uint8_t )( atoi ( name ) );

	g_signal_emit_by_name ( bar, "bar-click-num", num );
}

static void bar_signal_name_set_stop ( Bar *bar )
{
	gtk_widget_set_visible ( GTK_WIDGET ( bar->buttons[3] ), TRUE  );
	gtk_widget_set_visible ( GTK_WIDGET ( bar->buttons[4] ), FALSE );
}

static void bar_signal_hide_play_stop ( GtkButton *button_h, GtkButton *button_s )
{
	gtk_widget_set_visible ( GTK_WIDGET ( button_s ), TRUE  );
	gtk_widget_set_visible ( GTK_WIDGET ( button_h ), FALSE );
}

static void bar_create ( Bar *bar )
{
	bar->label = (GtkLabel *)gtk_label_new ( "" );
	gtk_widget_set_visible (  GTK_WIDGET ( bar->label ), TRUE );

	enum b_num c = BUP; for ( c = BUP; c < BFT; c++ )
	{
		bar->buttons[c] = (GtkButton *)gtk_button_new_from_icon_name ( icons[c], GTK_ICON_SIZE_MENU );

		gtk_button_set_relief ( bar->buttons[c], GTK_RELIEF_NONE );
		gtk_widget_set_visible ( GTK_WIDGET ( bar->buttons[c] ), ( c == 4 ) ? FALSE : TRUE );
		gtk_box_pack_start ( GTK_BOX ( bar ), GTK_WIDGET ( bar->buttons[c] ), FALSE, FALSE, 0 );

		char buf[20];
		sprintf ( buf, "%d", c );
		gtk_widget_set_name ( GTK_WIDGET ( bar->buttons[c] ), buf );

		g_signal_connect ( bar->buttons[c], "clicked", G_CALLBACK ( bar_signal_all_buttons ), bar );
	}

	g_signal_connect ( bar->buttons[3], "clicked", G_CALLBACK ( bar_signal_hide_play_stop ), bar->buttons[4] );
	g_signal_connect ( bar->buttons[4], "clicked", G_CALLBACK ( bar_signal_hide_play_stop ), bar->buttons[3] );

	gtk_box_pack_start ( GTK_BOX ( bar ), GTK_WIDGET ( bar->label ), FALSE, FALSE, 50 );

	for ( c = BFT; c < BAL; c++ )
	{
		bar->buttons[c] = (GtkButton *)gtk_button_new_from_icon_name ( icons[c], GTK_ICON_SIZE_MENU );

		gtk_button_set_relief ( bar->buttons[c], GTK_RELIEF_NONE );
		gtk_widget_set_visible ( GTK_WIDGET ( bar->buttons[c] ), TRUE );
		gtk_box_pack_end ( GTK_BOX ( bar ), GTK_WIDGET ( bar->buttons[c] ), FALSE, FALSE, 0 );

		char buf[20];
		sprintf ( buf, "%d", c );
		gtk_widget_set_name ( GTK_WIDGET ( bar->buttons[c] ), buf );

		g_signal_connect ( bar->buttons[c], "clicked", G_CALLBACK ( bar_signal_all_buttons ), bar );
	}
}

static void bar_init ( Bar *bar )
{
	GtkBox *box = GTK_BOX ( bar );
	gtk_orientable_set_orientation ( GTK_ORIENTABLE ( box ), GTK_ORIENTATION_HORIZONTAL );

	gtk_box_set_spacing ( box, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( box ), TRUE );

	bar_create ( bar );

	g_signal_connect ( bar, "bar-set-stop",  G_CALLBACK ( bar_signal_name_set_stop  ), NULL );
	g_signal_connect ( bar, "bar-set-label", G_CALLBACK ( bar_signal_name_set_label ), NULL );
}

static void bar_finalize ( GObject *object )
{
	G_OBJECT_CLASS ( bar_parent_class )->finalize ( object );
}

static void bar_class_init ( BarClass *class )
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);

	oclass->finalize = bar_finalize;

	g_signal_new ( "bar-set-stop",  G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0 );
	g_signal_new ( "bar-click-num", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT );
	g_signal_new ( "bar-set-label", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING );
}

Bar * bar_new ( void )
{
	return g_object_new ( BAR_TYPE_BOX, NULL );
}

