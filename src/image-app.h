/*
* Copyright 2022 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#pragma once

#include <gtk/gtk.h>

#define IMAGE_TYPE_APP image_app_get_type ()

G_DECLARE_FINAL_TYPE ( ImageApp, image_app, IMAGE, APP, GtkApplication )

ImageApp * image_app_new ( void );

