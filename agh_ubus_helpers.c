/*
 * Much of the code in this file comes from the ubus software; but it may be present here in a modified form, or in a completely different one.
 * So, any bugs in this code should be reported to me, not the original author. Function names have been changed.
 *
 * The code in here is subject to LGPL, Version 2.1, as published by the Free Software Foundation.
*/
#define SYSLOG_NAMES
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <glib.h>
#include <syslog.h>
#include <libubox/blobmsg_json.h>
#include "agh_ubus_helpers.h"
#include "agh_ubus.h"
#include "commands.h"



const struct blobmsg_policy log_policy[] = {
	[LOG_MSG] = { .name = "msg", .type = BLOBMSG_TYPE_STRING },
	[LOG_ID] = { .name = "id", .type = BLOBMSG_TYPE_INT32 },
	[LOG_PRIO] = { .name = "priority", .type = BLOBMSG_TYPE_INT32 },
	[LOG_SOURCE] = { .name = "source", .type = BLOBMSG_TYPE_INT32 },
	[LOG_TIME] = { .name = "time", .type = BLOBMSG_TYPE_INT64 },
};

const gchar *agh_ubus_helper_format_type(gpointer priv, struct blob_attr *attr) {
	static const char * const attr_types[] = {
		[BLOBMSG_TYPE_INT8] = "\"Boolean\"",
		[BLOBMSG_TYPE_INT32] = "\"Integer\"",
		[BLOBMSG_TYPE_STRING] = "\"String\"",
		[BLOBMSG_TYPE_ARRAY] = "\"Array\"",
		[BLOBMSG_TYPE_TABLE] = "\"Table\"",
	};
	const gchar *type;
	guint typeid;

	type = NULL;

	if (blob_id(attr) != BLOBMSG_TYPE_INT32)
		return NULL;

	typeid = blobmsg_get_u32(attr);

	if (typeid < ARRAY_SIZE(attr_types))
		type = attr_types[typeid];

	if (!type)
		type = "\"(unknown)\"";

	return type;
}

/* The name and much of the code in this function has been inspired by ubus cli.c, function "receive_list_result". */
void agh_ubus_handler_list_receive_results(struct ubus_context *ctx, struct ubus_object_data *obj, gpointer data) {
	struct command *cmd = data;

	guint rem;
	struct blob_attr *cur;
	gchar *tmp;

	cur = NULL;
	tmp = NULL;
	rem = 0;

	cmd_answer_set_status(cmd, CMD_ANSWER_STATUS_OK);
	cmd_answer_peektext(cmd, g_strdup_printf("\n\"OBJECT=%s, ID=@%08x\"\n", obj->path, obj->id));

	if (!obj->signature) {
		g_print("%s: no signature fo %s\n",__FUNCTION__,obj->path);
		return;
	}

	blob_for_each_attr(cur, obj->signature, rem) {
		tmp = blobmsg_format_json_with_cb(cur, false, agh_ubus_helper_format_type, NULL, -1);

		if (tmp) {
			cmd_answer_peektext(cmd, g_strdup_printf("%s\n", tmp));
			g_free(tmp);
			tmp = NULL;
		}

	}

	cmd_answer_set_data(cmd, TRUE);

	return;
}

/* Lots of code in here comes from getcodetext, in the logread.c file. */
const char* agh_ubus_logstream_getcodetext(int value, CODE *codetable) {
	CODE *i;

	if (value >= 0)
		for (i = codetable; i->c_val != -1; i++)
			if (i->c_val == value)
				return (i->c_name);

	return "<unknown>";
}

/* Lots of code in here comes from log_notify, found in logread.c (ubox package); LGPL as well. */
gint agh_ubus_logstream_parse_log(struct blob_attr *msg, gchar **destptr) {
	struct blob_attr *tb[__LOG_MAX];

	/* Why + 128 ? */
	char buf[AGH_UBUS_LOGSTREAM_LOG_LINE_SIZE + 128];
	char buf_ts[32];
	char *c;
	uint32_t p;
	time_t t;
	gchar *msg_str;

	uint32_t t_ms = 0;
	int ret = 0;

	if (*destptr)
		return 1;

	blobmsg_parse(log_policy, ARRAY_SIZE(log_policy), tb, blob_data(msg), blob_len(msg));
	if (!tb[LOG_ID] || !tb[LOG_PRIO] || !tb[LOG_SOURCE] || !tb[LOG_TIME] || !tb[LOG_MSG])
		return 1;

	msg_str = blobmsg_get_string(tb[LOG_MSG]);

	t = blobmsg_get_u64(tb[LOG_TIME]) / 1000;
	t_ms = blobmsg_get_u64(tb[LOG_TIME]) % 1000;
	snprintf(buf_ts, sizeof(buf_ts), "[%lu.%03u] ", (unsigned long)t, t_ms);

	c = ctime(&t);

	p = blobmsg_get_u32(tb[LOG_PRIO]);
	c[strlen(c) - 1] = '\0';
	snprintf(buf, sizeof(buf), "%s %s%s.%s%s %s\n", c, buf_ts, agh_ubus_logstream_getcodetext(LOG_FAC(p) << 3, facilitynames), agh_ubus_logstream_getcodetext(LOG_PRI(p), prioritynames), (blobmsg_get_u32(tb[LOG_SOURCE])) ? ("") : (" kernel:"), msg_str);
	*destptr = g_strdup_printf("\n%s", buf);

	return ret;
}
