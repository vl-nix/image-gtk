/*
* Copyright 2022 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#pragma once

#include "image-app.h"

#define IMAGE_TYPE_WIN image_win_get_type ()

G_DECLARE_FINAL_TYPE ( ImageWin, image_win, IMAGE, WIN, GtkWindow )

ImageWin * image_win_new ( GFile *, ImageApp * );
