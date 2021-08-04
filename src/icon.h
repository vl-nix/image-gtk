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

#define ICON_TYPE_BOX icon_get_type ()

G_DECLARE_FINAL_TYPE ( Icon, icon, ICON, BOX, GtkBox )

Icon * icon_new ( void );

