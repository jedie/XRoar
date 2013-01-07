/*  Copyright 2003-2013 Ciaran Anscomb
 *
 *  This file is part of XRoar.
 *
 *  XRoar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  XRoar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XRoar.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>

#include "module.h"

static char *filereq_noop(const char **extensions);
FileReqModule filereq_null_module = {
	.common = { .name = "null", .description = "No file requester" },
	.load_filename = filereq_noop, .save_filename = filereq_noop
};

static FileReqModule *null_filereq_module_list[] = {
	&filereq_null_module, NULL
};

extern VideoModule video_null_module;
static VideoModule *null_video_module_list[] = {
	&video_null_module,
	NULL
};

KeyboardModule keyboard_null_module = {
	.common = { .name = "null", .description = "No keyboard" },
};

static KeyboardModule *null_keyboard_module_list[] = {
	&keyboard_null_module,
	NULL
};

UIModule ui_null_module = {
	.common = { .name = "null", .description = "No UI" },
	.filereq_module_list = null_filereq_module_list,
	.video_module_list = null_video_module_list,
	.keyboard_module_list = null_keyboard_module_list,
};

/* */

static char *filereq_noop(const char **extensions) {
	(void)extensions;
	return NULL;
}
