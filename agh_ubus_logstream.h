/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __agh_ubus_logstream_h__
#define __agh_ubus_logstream_h__
#include "agh_ubus.h"
#include <glib.h>

#define AGH_UBUS_LOGSTREAM_CHECK_INTERVAL 400

/* logstream log messages event name */
#define AGH_UBUS_LOGSTREAM_LOG_EVENTs_NAME "SYSTEM_LOG_MESSAGE"

struct agh_ubus_logstream_ctx {
	guint logstream_channel_tag;
	GIOChannel *logstream_channel;
	GSource *logstream_reconnect;
	guint logstream_reconnect_tag;
	guint logstream_state;
	struct ubus_request *current_req;
	gint fd;
	struct blob_buf *b;
	GError *gerr;
	guint logwatcher_id;
	GSource *logwatcher;
	GString *cmsg;
};

gint agh_ubus_logstream_init(struct agh_ubus_ctx *uctx);
gint agh_ubus_logstream_deinit(struct agh_ubus_ctx *uctx);

#endif
