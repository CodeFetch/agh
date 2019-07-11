/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __agh_mm_handlers_h__
#define __agh_mm_handlers_h__
#include "agh_commands.h"

struct agh_message *agh_mm_cmd_handle(struct agh_handler *h, struct agh_message *m);

#endif
