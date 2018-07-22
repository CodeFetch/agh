/*
 * see agh_ubus_helpers.c for important Copyright and License informations.
*/

#ifndef __agh_ubus_helpers_h__
#define __agh_ubus_helpers_h__
#include <libubus.h>

/* This comes from LOG_LINE_SIZE, defined in ubox/log/syslog.h in the ubox package. Name changed to be less generic and avoid collisions */
#define AGH_UBUS_LOGSTREAM_LOG_LINE_SIZE 1024

enum {
	LOG_MSG,
	LOG_ID,
	LOG_PRIO,
	LOG_SOURCE,
	LOG_TIME,
	__LOG_MAX
};

const gchar *agh_ubus_helper_format_type(gpointer priv, struct blob_attr *attr);
void agh_ubus_handler_list_receive_results(struct ubus_context *ctx, struct ubus_object_data *obj, gpointer data);
gint agh_ubus_logstream_parse_log(struct blob_attr *msg, gchar **destptr);

#endif
