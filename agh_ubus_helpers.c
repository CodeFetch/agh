/*
 * Much of the code in this file comes from the ubus software; but it may be present here in a modified form, or in a completely different one.
 * So, any bugs in this code should be reported to me, not the original author. Function names have been changed.
 *
 * The code in here is subject to LGPL, Version 2.1, as published by the Free Software Foundation.
*/

#include <glib.h>
#include <libubox/blobmsg_json.h>

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
