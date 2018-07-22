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
gboolean agh_ubus_logstream_statemachine(gpointer data);
void agh_ubus_logstream_fd_cb(struct ubus_request *req, int fd);

void agh_ubus_logstream_channel_init(struct agh_ubus_logstream_ctx *lctx, GMainContext *gmctx);
void agh_ubus_logstream_channel_deinit(struct agh_ubus_logstream_ctx *lctx);
gboolean agh_ubus_logstream_channel_io(GIOChannel *channel, GIOCondition condition, gpointer data);
void agh_ubus_logstream_incoming_message(struct agh_ubus_logstream_ctx *lctx);

#endif
