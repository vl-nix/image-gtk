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

enum p_num
{
	PHR, PVT, PLT, PRT
};

#define PREVIEW_TYPE_BOX preview_get_type ()

G_DECLARE_FINAL_TYPE ( Preview, preview, PREVIEW, BOX, GtkBox )

Preview * preview_new ( void );

