/*
* Copyright 2021 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#pragma once

#include <gtk/gtk.h>

enum b_num
{
	BUP, BCK, BFW, BST, BSP, BLT, BRT, BVR, BHR, /*BPR, */BFT, BOR, BMN, BPL, BAL
};

#define BAR_TYPE_BOX bar_get_type ()

G_DECLARE_FINAL_TYPE ( Bar, bar, BAR, BOX, GtkBox )

Bar * bar_new ( void );

