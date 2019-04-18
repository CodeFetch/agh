/*
 * So here we are, for the second time: after the HDD breakage.
 * Subsequently, it got rewritten: the previous one wasn't so great.
*/

#include <glib.h>
#include <libmm-glib.h>
#include "agh_modem.h"
#include "agh.h"
#include "agh_logging.h"
#include "agh_ubus.h"
#include "agh_handlers.h"
#include "agh_mm_handler.h"
#include "agh_modem_config.h"
#include "agh_mm_helpers.h"

/* Log messages from AGH_LOG_DOMAIN_MODEM domain. */
#define AGH_LOG_DOMAIN_MODEM "MM"

/* Logging macros. */
#define agh_log_mm_dbg(message, ...) agh_log_dbg(AGH_LOG_DOMAIN_MODEM, message, ##__VA_ARGS__)
#define agh_log_mm_crit(message, ...) agh_log_crit(AGH_LOG_DOMAIN_MODEM, message, ##__VA_ARGS__)

struct agh_comm *agh_mm_aghcomm;

static MMObject *agh_mm_get_mmobject(struct agh_state *mstate, MMModem *modem) {
	GList *l;
	GList *modems;
	struct agh_mm_state *mmstate;
	MMObject *found;

	found = NULL;

	if (!mstate || !mstate->mmstate || !mstate->mmstate->manager || !modem) {
		agh_log_mm_crit("AGH state, AGH MM state or manager object where not present");
		goto out;
	}

	mmstate = mstate->mmstate;

	modems = g_dbus_object_manager_get_objects(G_DBUS_OBJECT_MANAGER(mmstate->manager));

	if (!modems) {
		agh_log_mm_dbg("no modems list");
		goto out;
	}

	for (l = modems; l; l = g_list_next(l)) {
		if (!g_strcmp0(mm_modem_get_path(modem), mm_object_get_path(MM_OBJECT(l->data)))) {
			found = MM_OBJECT(l->data);
			modems = g_list_remove(modems, l->data);
			break;
		}
	}

	g_list_free_full(modems, g_object_unref);

out:
	return found;
}

static gchar *agh_mm_sms_info_string(MMSms *sms) {
	MMSmsPduType pdu_type;
	gchar *data = NULL;
	const guint8 *databin = NULL;
	gsize databin_size;
	gchar *validity = NULL;
	gchar *class = NULL;
	const gchar *delivery_report = NULL;
	gchar *message_reference = NULL;
	const gchar *delivery_state = NULL;
	GString *output = NULL;

	databin = mm_sms_get_data(sms, &databin_size);

	if (databin)
		data = mm_utils_bin2hexstr(databin, databin_size);

	if (mm_sms_get_validity_type(sms) == MM_SMS_VALIDITY_TYPE_RELATIVE)
		validity = g_strdup_printf("relative_validity=%u", mm_sms_get_validity_relative(sms));

	if (mm_sms_get_class(sms) >= 0)
		class = g_strdup_printf ("class=%d", mm_sms_get_class (sms));

	pdu_type = mm_sms_get_pdu_type(sms);
	if (pdu_type == MM_SMS_PDU_TYPE_SUBMIT)
		delivery_report = mm_sms_get_delivery_report_request(sms) ? "requested" : "not requested";

	if (mm_sms_get_message_reference(sms) != 0)
		message_reference = g_strdup_printf("message_reference=%u", mm_sms_get_message_reference(sms));

	if (mm_sms_get_delivery_state(sms) != MM_SMS_DELIVERY_STATE_UNKNOWN)
		delivery_state = mm_sms_delivery_state_get_string_extended(mm_sms_get_delivery_state(sms));

	output = g_string_new("SMS_DATA: ");
	if (data)
		g_string_append_printf(output, "hexdata=%s; ",data);

	if (validity)
		g_string_append_printf(output, "%s; ",validity);

	if (class)
		g_string_append_printf(output, "%s; ",class);

	if (delivery_report)
		g_string_append_printf(output, "delivery_report=%s; ",delivery_report);

	if (message_reference)
		g_string_append_printf(output, "%s; ",message_reference);

	if (delivery_state)
		g_string_append_printf(output, "delivery_state=%s; ",delivery_state);

	g_free(data);
	g_free(validity);
	g_free(class);
	g_free(message_reference);

	return g_string_free(output, FALSE);
}

gint agh_mm_report_sms(struct agh_comm *comm, MMSms *sms) {
	gint retval;
	struct agh_cmd *ev;
	gchar *atext;
	const gchar *smstext;
	const gchar *number;

	retval = 0;

	if (!comm || !sms) {
		agh_log_mm_crit("no AGH COMM, or NULL MMSms object");
		retval = 25;
		goto out;
	}

	ev = agh_cmd_event_alloc(&retval);
	if (!ev) {
		agh_log_mm_crit("AGH event allocation failure while reporting SMS (code=%" G_GINT16_FORMAT")",retval);
		retval = 26;
		goto out;
	}

	agh_cmd_answer_set_status(ev, AGH_CMD_ANSWER_STATUS_OK);
	agh_cmd_answer_addtext(ev, "SMS", TRUE);

	number = mm_sms_get_number(sms);
	if (number) {
		atext = g_str_to_ascii(number, "C");
		agh_cmd_answer_addtext(ev, atext, FALSE);
	}

	agh_cmd_answer_addtext(ev, mm_sms_state_get_string(mm_sms_get_state(sms)), TRUE);

	smstext = mm_sms_get_text(sms);
	if (smstext) {
		atext = g_str_to_ascii(smstext, "C");
		agh_cmd_answer_addtext(ev, atext, FALSE);
	}

	agh_cmd_answer_addtext(ev, mm_sms_storage_get_string(mm_sms_get_storage(sms)), TRUE);

	agh_cmd_answer_addtext(ev, mm_sms_get_smsc(sms), TRUE);

	agh_cmd_answer_addtext(ev, mm_sms_get_timestamp(sms), TRUE);

	agh_cmd_answer_addtext(ev, mm_sms_get_discharge_timestamp(sms), TRUE);

	agh_cmd_answer_addtext(ev, agh_mm_sms_info_string(sms), FALSE);

	retval = agh_cmd_emit_event(comm, ev);

out:
	if (retval) {
		agh_log_mm_crit("event for SMS could not be emitted (code=%" G_GINT16_FORMAT")",retval);
	}

	return retval;
}

static void agh_mm_modem_sms_deleted(MMModemMessaging *messaging, const gchar *sms_path) {
	agh_log_mm_dbg("SMS deleted at %s",sms_path);
	agh_mm_report_event(agh_mm_aghcomm, "SMS_DELETED", agh_mm_modem_to_index(sms_path), "--");
	return;
}

static void agh_mm_modem_sms_added(MMModemMessaging *messaging, const gchar *sms_path, gboolean received) {
	agh_log_mm_dbg("SMS added at %s",sms_path);

	if (received)
		agh_mm_report_event(agh_mm_aghcomm, "SMS_ADDED", agh_mm_modem_to_index(sms_path), "++");

	return;
}

static gint agh_modem_set_handler_ext(struct agh_state *mstate) {
	struct agh_handler *agh_mm_handler;
	gint retval;

	retval = 1;

	if ( !(agh_mm_handler = agh_new_handler("agh_mm_handler")) )
		goto out;

	agh_handler_set_handle(agh_mm_handler, agh_mm_cmd_handle);
	agh_handler_enable(agh_mm_handler, TRUE);

	if (agh_handler_register(mstate->agh_handlers, agh_mm_handler))
		goto out;

	retval = 0;

out:
	if (retval) {
		g_clear_pointer(&agh_mm_handler, agh_handler_dealloc);
	}

	return retval;;
}

static struct agh_mm_iptypes_family_table {
	char *name;
	gint type;
} agh_mm_iptypes_family_static_table[] = {
	{"none", MM_BEARER_IP_FAMILY_NONE},
	{"IPV4", MM_BEARER_IP_FAMILY_IPV4},
	{"IPV6", MM_BEARER_IP_FAMILY_IPV6},
	{"IPV4V6", MM_BEARER_IP_FAMILY_IPV4V6},
	{"any", MM_BEARER_IP_FAMILY_ANY},
	{NULL,}
};

static gchar *agh_mm_call_outside_build_message_add_json_fragment(const gchar *name, const gchar *value, gboolean last) {
	gchar *res;

	res = NULL;

	if (!name || !strlen(name))
		return res;

	if (!value || !strlen(value))
		value = "unknown";

	if (!last)
		res = g_strdup_printf("\"%s\":\"%s\",",name,value);
	else
		res = g_strdup_printf("\"%s\":\"%s\"",name,value);

	return res;
}

static gchar *agh_mm_call_outside_build_message(MMBearer *b) {
	GString *s;
	gchar *numeric_quantity_string_tmp;
	MMBearerIpConfig *ipv4_config;
	MMBearerIpConfig *ipv6_config;
	MMBearerIpMethod method;
	MMBearerProperties *props;
	const gchar **dns_tmp;
	guint dns_counter;
	gchar *str_tmp;
	gchar *dns_str_tmp;
	struct agh_mm_iptypes_family_table *family;
	int type;

	s = g_string_new("{");
	numeric_quantity_string_tmp = NULL;
	ipv4_config = NULL;
	ipv6_config = NULL;
	props = NULL;
	dns_tmp = NULL;
	dns_counter = 1;
	str_tmp = NULL;
	dns_str_tmp = NULL;

	/* bearer path */
	str_tmp = agh_mm_call_outside_build_message_add_json_fragment("BEARER_PATH", mm_bearer_get_path(b), FALSE);
	g_string_append(s, str_tmp);
	g_free(str_tmp);

	/* interface */
	str_tmp = agh_mm_call_outside_build_message_add_json_fragment("BEARER_INTERFACE", mm_bearer_get_interface(b), FALSE);
	g_string_append(s, str_tmp);
	g_free(str_tmp);

	numeric_quantity_string_tmp = g_strdup_printf("%" G_GUINT16_FORMAT"",mm_bearer_get_ip_timeout(b));
	str_tmp = agh_mm_call_outside_build_message_add_json_fragment("BEARER_IP_TIMEOUT", numeric_quantity_string_tmp, FALSE);
	g_string_append(s, str_tmp);
	g_free(numeric_quantity_string_tmp);
	g_free(str_tmp);

	props = mm_bearer_get_properties(b);
	type = mm_bearer_properties_get_ip_type(props);
	for (family = agh_mm_iptypes_family_static_table; family->name; family++)
		if (family->type == type)
			break;
	str_tmp = agh_mm_call_outside_build_message_add_json_fragment("BEARER_IP_FAMILY", family->name, FALSE);
	g_string_append(s, str_tmp);
	g_free(str_tmp);

	ipv4_config = mm_bearer_get_ipv4_config(b);
	ipv6_config = mm_bearer_get_ipv6_config(b);

	g_object_unref(props);
	props = NULL;

	if (ipv4_config) {
		method = mm_bearer_ip_config_get_method(ipv4_config);
		switch(method) {
			case MM_BEARER_IP_METHOD_UNKNOWN:
				str_tmp = agh_mm_call_outside_build_message_add_json_fragment("BEARER_IP_METHOD", "unknown", FALSE);
				g_string_append(s, str_tmp);
				g_free(str_tmp);
				break;
			case MM_BEARER_IP_METHOD_PPP:
				str_tmp = agh_mm_call_outside_build_message_add_json_fragment("BEARER_IP_METHOD", "ppp", FALSE);
				g_string_append(s, str_tmp);
				g_free(str_tmp);
				break;
			case MM_BEARER_IP_METHOD_STATIC:
				str_tmp = agh_mm_call_outside_build_message_add_json_fragment("BEARER_IP_METHOD", "static", FALSE);
				g_string_append(s, str_tmp);
				g_free(str_tmp);
				break;
			case MM_BEARER_IP_METHOD_DHCP:
				str_tmp = agh_mm_call_outside_build_message_add_json_fragment("BEARER_IP_METHOD", "DHCP", FALSE);
				g_string_append(s, str_tmp);
				g_free(str_tmp);
				break;
		}

		/* IP address */
		str_tmp = agh_mm_call_outside_build_message_add_json_fragment("BEARER_IP_ADDRESS", mm_bearer_ip_config_get_address(ipv4_config), FALSE);
		g_string_append(s, str_tmp);
		g_free(str_tmp);

		/* prefix */
		numeric_quantity_string_tmp = g_strdup_printf("%" G_GUINT16_FORMAT"",mm_bearer_ip_config_get_prefix(ipv4_config));
		str_tmp = agh_mm_call_outside_build_message_add_json_fragment("BEARER_IP_PREFIX", numeric_quantity_string_tmp, FALSE);
		g_string_append(s, str_tmp);
		g_free(numeric_quantity_string_tmp);
		g_free(str_tmp);

		/* playground */
		dns_tmp = mm_bearer_ip_config_get_dns(ipv4_config);
		if (dns_tmp) {
			while (*dns_tmp) {
				dns_str_tmp = g_strdup_printf("BEARER_IP_DNS_%" G_GUINT16_FORMAT"",dns_counter);
				str_tmp = agh_mm_call_outside_build_message_add_json_fragment(dns_str_tmp, *dns_tmp, FALSE);
				g_string_append(s, str_tmp);
				dns_counter++;
				dns_tmp++;
				g_free(str_tmp);
				g_free(dns_str_tmp);
			}
			dns_tmp = NULL;
			dns_counter = 1;
		}
		/* end of playground */

		str_tmp = agh_mm_call_outside_build_message_add_json_fragment("BEARER_IP_GATEWAY", mm_bearer_ip_config_get_gateway(ipv4_config), FALSE);
		g_string_append(s, str_tmp);
		g_free(str_tmp);

		numeric_quantity_string_tmp = g_strdup_printf("%" G_GUINT16_FORMAT"",mm_bearer_ip_config_get_mtu(ipv4_config));
		str_tmp = agh_mm_call_outside_build_message_add_json_fragment("BEARER_IP_MTU", numeric_quantity_string_tmp, FALSE);
		g_string_append(s, str_tmp);
		g_free(str_tmp);
		g_free(numeric_quantity_string_tmp);

		g_object_unref(ipv4_config);
		ipv4_config = NULL;
	}

	if (ipv6_config) {
		method = mm_bearer_ip_config_get_method(ipv6_config);
		switch(method) {
			case MM_BEARER_IP_METHOD_UNKNOWN:
				str_tmp = agh_mm_call_outside_build_message_add_json_fragment("BEARER_IPV6_METHOD", "unknown", FALSE);
				g_string_append(s, str_tmp);
				g_free(str_tmp);
				break;
			case MM_BEARER_IP_METHOD_PPP:
				str_tmp = agh_mm_call_outside_build_message_add_json_fragment("BEARER_IPV6_METHOD", "ppp", FALSE);
				g_string_append(s, str_tmp);
				g_free(str_tmp);
				break;
			case MM_BEARER_IP_METHOD_STATIC:
				str_tmp = agh_mm_call_outside_build_message_add_json_fragment("BEARER_IPV6_METHOD", "static", FALSE);
				g_string_append(s, str_tmp);
				g_free(str_tmp);
				break;
			case MM_BEARER_IP_METHOD_DHCP:
				str_tmp = agh_mm_call_outside_build_message_add_json_fragment("BEARER_IPV6_METHOD", "DHCP", FALSE);
				g_string_append(s, str_tmp);
				g_free(str_tmp);
				break;
		}

		/* IP address */
		str_tmp = agh_mm_call_outside_build_message_add_json_fragment("BEARER_IPV6_ADDRESS", mm_bearer_ip_config_get_address(ipv6_config), FALSE);
		g_string_append(s, str_tmp);
		g_free(str_tmp);

		/* prefix */
		numeric_quantity_string_tmp = g_strdup_printf("%" G_GUINT16_FORMAT"",mm_bearer_ip_config_get_prefix(ipv6_config));
		str_tmp = agh_mm_call_outside_build_message_add_json_fragment("BEARER_IPV6_PREFIX", numeric_quantity_string_tmp, FALSE);
		g_string_append(s, str_tmp);
		g_free(numeric_quantity_string_tmp);
		g_free(str_tmp);

		/* playground */
		dns_tmp = mm_bearer_ip_config_get_dns(ipv6_config);
		if (dns_tmp) {
			while (*dns_tmp) {
				dns_str_tmp = g_strdup_printf("BEARER_IP_DNSV6_%" G_GUINT16_FORMAT"",dns_counter);
				str_tmp = agh_mm_call_outside_build_message_add_json_fragment(dns_str_tmp, *dns_tmp, FALSE);
				g_string_append(s, str_tmp);
				dns_counter++;
				dns_tmp++;
				g_free(str_tmp);
				g_free(dns_str_tmp);
			}
			dns_tmp = NULL;
			dns_counter = 1;
		}
		/* end of playground */

		str_tmp = agh_mm_call_outside_build_message_add_json_fragment("BEARER_IPV6_GATEWAY", mm_bearer_ip_config_get_gateway(ipv6_config), FALSE);
		g_string_append(s, str_tmp);
		g_free(str_tmp);

		numeric_quantity_string_tmp = g_strdup_printf("%" G_GUINT16_FORMAT"",mm_bearer_ip_config_get_mtu(ipv6_config));
		str_tmp = agh_mm_call_outside_build_message_add_json_fragment("BEARER_IPV6_MTU", numeric_quantity_string_tmp, FALSE);
		g_string_append(s, str_tmp);
		g_free(numeric_quantity_string_tmp);
		g_free(str_tmp);

		g_object_unref(ipv6_config);
		ipv6_config = NULL;
	}

	g_string_append(s, "}");

	return g_string_free(s, FALSE);
}

static gint agh_mm_call_outside_helper(struct agh_state *mstate, MMBearer *b) {
	gchar *ubus_call_bearers_info_message;
	gchar *ubus_message;
	gint status;
	gchar *callee_output;

	ubus_message = NULL;
	status = 0;
	ubus_call_bearers_info_message = NULL;

	if (!mstate || !b) {
		agh_log_mm_crit("missing AGH state or bearer is NULL");
		status = 39;
		goto out;
	}

	if (!mstate->uctx) {
		agh_log_mm_crit("no ubus context");
		status = 40;
		goto out;
	}

	if (agh_ubus_connection_state != AGH_UBUS_STATE_CONNECTED) {
		agh_log_mm_crit("we are not connected to ubus");
		status = 41;
		goto out;
	}

	ubus_call_bearers_info_message = agh_mm_call_outside_build_message(b);
	if (!ubus_call_bearers_info_message) {
		agh_log_mm_crit("no info message to pass");
		status = 42;
		goto out;
	}

	ubus_message = g_strdup_printf("{\"command\":\"/opt/bearer_setup_helper.sh\",\"env\":%s}", ubus_call_bearers_info_message);

	status = agh_ubus_call(mstate->uctx, "file", "exec", ubus_message);
	if (status) {
		agh_log_mm_crit("ubus call failure (code=%" G_GINT16_FORMAT") - %s",status,ubus_strerror(status));
	}
	else {
		callee_output = agh_ubus_get_call_result(TRUE);

		if (callee_output) {
			agh_mm_report_event(mstate->comm, "agh_mm_call_outside_helper", agh_mm_modem_to_index(mm_bearer_get_path(b)), callee_output);
			agh_log_mm_dbg("from call: %s",callee_output);
			g_free(callee_output);
		}
	}

out:
	g_free(ubus_message);
	g_free(ubus_call_bearers_info_message);
	ubus_call_bearers_info_message = NULL;
	return status;
}

static void agh_mm_bearer_update_outside(MMBearer *b, GParamSpec *pspec, gpointer user_data) {
	struct agh_state *mstate = user_data;
	gint call_outside_error;

	if (!b) {
		agh_log_mm_crit("called with a NULL GObject? I did not think this was possible");
		return;
	}

	call_outside_error = agh_mm_call_outside_helper(mstate, b);
	if (call_outside_error) {
		agh_log_mm_crit("failure from agh_mm_call_outside_helper (code=%" G_GINT16_FORMAT")",call_outside_error);
	}

	switch(mm_bearer_get_connected(b)) {
		case TRUE:
			agh_log_mm_dbg("we are connected!");
			break;
		case FALSE:
			agh_log_mm_dbg("we are NOT connected...");
			g_object_unref(b);
			break;
	}

	return;
}

static gint agh_mm_bearer_signal(struct agh_state *mstate, MMBearer *b) {
	gint retval;
	gulong signal_id;

	retval = 0;

	if (!mstate || !mstate->mmstate || !mstate->mmstate->manager || !b) {
		agh_log_mm_crit("missing context when connecting signals to bearer");
		retval = 71;
		goto out;
	}

	signal_id = g_signal_connect(b, "notify::connected", G_CALLBACK(agh_mm_bearer_update_outside), mstate);
	if (!signal_id) {
		agh_log_mm_crit("failure while connecting signals to bearer");
		retval = 72;
		goto out;
	}
	agh_log_mm_dbg("signals connected to bearer %s",mm_bearer_get_path(b));

out:
	return retval;
}

static void agh_mm_connect_bearer_finish(MMBearer *b, GAsyncResult *res, gpointer user_data) {
	struct agh_state *mstate = user_data;

	if (!mstate || !mstate->mmstate) {
		agh_log_mm_crit("missing context");

		if (b)
			g_object_unref(b);

	}

	mstate->mmstate->global_bearer_connecting_lock = FALSE;
	switch(mm_bearer_connect_finish(b, res, &mstate->mmstate->current_gerror)) {
		case TRUE:
			agh_log_mm_dbg("bearer successfully connected");
			break;
		case FALSE:
			agh_log_mm_crit("failed to connect bearer");
			agh_modem_report_gerror_message(&mstate->mmstate->current_gerror, NULL);
			g_object_unref(b);
			break;
	}

	return;
}

static void agh_mm_modem_connect_bearer(gpointer data, gpointer user_data) {
	MMBearer *b = MM_BEARER(data);
	struct agh_state *mstate = user_data;
	const gchar *bpath;

	if (!b) {
		agh_log_mm_crit("can not connect a NULL bearer");
		return;
	}

	if (mm_bearer_get_connected(b))
		return;

	bpath = mm_bearer_get_path(b);

	agh_log_mm_dbg("requesting for bearer %s to be connected", bpath);
	agh_mm_bearer_signal(mstate, g_object_ref(b));
	mm_bearer_connect(b, NULL, (GAsyncReadyCallback)agh_mm_connect_bearer_finish, mstate);
	mstate->mmstate->global_bearer_connecting_lock = TRUE;
	return;
}

static gint agh_mm_modem_bearers(struct agh_state *mstate, MMModem *modem, GAsyncReadyCallback cb) {
	gint retval;

	retval = 0;

	if (!modem || !cb) {
		agh_log_mm_crit("NULL modem object, or callback");
		retval = 41;
		goto out;
	}

	mm_modem_list_bearers(modem, NULL, (GAsyncReadyCallback)cb, mstate);

out:
	return retval;
}

static void agh_mm_modem_connect_bearers(GObject *o, GAsyncResult *res, gpointer user_data) {
	struct agh_state *mstate = user_data;
	GList *current_bearers;
	GList *l;
	MMModem *modem = MM_MODEM(o);

	current_bearers = mm_modem_list_bearers_finish(modem, res, &mstate->mmstate->current_gerror);
	if (!current_bearers) {
		agh_log_mm_crit("problem when checking bearers");
		agh_modem_report_gerror_message(&mstate->mmstate->current_gerror, NULL);
		goto out;
	}

	g_list_foreach(current_bearers, agh_mm_modem_connect_bearer, mstate);

	g_list_free_full(current_bearers, g_object_unref);

out:
	return;
}

static gint agh_mm_modem_delete_all_bearers_sync(struct agh_state *mstate, MMModem *modem) {
	GList *current_bearers;
	GList *l;
	MMBearer *b;
	gint retval;

	retval = 0;

	current_bearers = mm_modem_list_bearers_sync(modem, NULL, &mstate->mmstate->current_gerror);
	if (!current_bearers) {
		agh_log_mm_crit("problem when requesting bearers list for deletion");
		agh_modem_report_gerror_message(&mstate->mmstate->current_gerror, NULL);
		retval = 15;
		goto out;
	}

	for (l = current_bearers; l; l = g_list_next(l)) {
		b = MM_BEARER(l->data);
		if (b) {
			if (!mm_modem_delete_bearer_sync(modem, mm_bearer_get_path(b), NULL, &mstate->mmstate->current_gerror)) {
				agh_modem_report_gerror_message(&mstate->mmstate->current_gerror, NULL);
				retval = 81;
			}
			else {
				agh_log_mm_dbg("bearer delete OK");
			}
		}
	}

	g_list_free_full(current_bearers, g_object_unref);

out:
	return retval;
}

static gint agh_mm_checker_get_modem(struct agh_state *mstate, MMObject *modem) {
	gint retval;
	MMModem *m;
	MMModemState state;

	retval = 0;

	m = mm_object_get_modem(modem);
	if (!m) {
		agh_log_mm_crit("failure obtaining modem object");
		retval = 74;
		goto out;
	}

	state = mm_modem_get_state(m);

	if (state == MM_MODEM_STATE_REGISTERED || state == MM_MODEM_STATE_CONNECTED) {
		retval = agh_mm_modem_bearers(mstate, m, agh_mm_modem_connect_bearers);
		if (retval) {
			agh_log_mm_crit("failure from agh_mm_modem_bearers (code=%" G_GINT16_FORMAT")",retval);
			goto out;
		}
	}

out:
	if (m)
		g_object_unref(m);

	return retval;
}

static gboolean agh_mm_checker(gpointer data) {
	struct agh_state *mstate = data;
	GList *modems;
	GList *l;
	gint retval;

	/* agh_log_mm_dbg("tick"); */

	if (!mstate->mmstate || !mstate->mmstate->manager) {
		agh_log_mm_crit("the checker is not supposed to run now; please take a look at this");
		mstate->mmstate->bearers_check = NULL;
		mstate->mmstate->bearers_check_tag = 0;
		return FALSE;
	}

	if (mstate->mmstate->global_bearer_connecting_lock) {
		agh_log_mm_crit("skipping check due to connecting lock");
		return TRUE;
	}

	modems = g_dbus_object_manager_get_objects(G_DBUS_OBJECT_MANAGER(mstate->mmstate->manager));
	if (!modems) {
		agh_log_mm_dbg("no more modems; see you next time!");
		mstate->mmstate->bearers_check = NULL;
		mstate->mmstate->bearers_check_tag = 0;
		return FALSE;
	}

	for (l = modems; l; l = g_list_next(l)) {
		retval = agh_mm_checker_get_modem(mstate, (MMObject *)(l->data));
		if (retval) {
			agh_log_mm_crit("got failure from agh_mm_checker_get_modem (code=%" G_GINT16_FORMAT")",retval);
		}
	}

	g_list_free_full(modems, g_object_unref);

	return TRUE;
}

static gint agh_mm_start_bearer_checker(struct agh_state *mstate) {
	gint retval;
	struct agh_mm_state *mmstate;

	retval = 0;

	if (!mstate || !mstate->mmstate) {
		agh_log_mm_crit("no AGH or AGH MM state");
		retval = 70;
		goto out;
	}

	mmstate = mstate->mmstate;

	if (!mmstate->bearers_check) {
		agh_log_mm_dbg("activating bearers checker");
		mmstate->bearers_check = g_timeout_source_new(45000);
		g_source_set_callback(mmstate->bearers_check, agh_mm_checker, mstate, NULL);
		mmstate->bearers_check_tag = g_source_attach(mmstate->bearers_check, mstate->ctx);

		g_source_unref(mmstate->bearers_check);

		if (!mmstate->bearers_check_tag) {
			agh_log_mm_crit("failed to attach checker to GMainContext");
			mmstate->bearers_check = NULL;
			retval = 71;
			goto out;
		}

	}
	else {
		agh_log_mm_dbg("bearers checker already active");
	}

out:
	return retval;
}

static void agh_mm_connect_bearer(GObject *o, GAsyncResult *res, gpointer user_data) {
	MMModem *modem = MM_MODEM(o);
	struct agh_state *mstate = user_data;
	MMBearer *b;

	b = mm_modem_create_bearer_finish(modem, res, &mstate->mmstate->current_gerror);
	if (!b) {
		agh_log_mm_crit("bearer creation failed for modem %s",mm_modem_get_path(modem));
		agh_modem_report_gerror_message(&mstate->mmstate->current_gerror, NULL);
		goto out;
	}

	agh_mm_start_bearer_checker(mstate);

out:
	if (b)
		g_object_unref(b);

	return;
}

static gint agh_mm_create_bearers_from_list(struct agh_state *mstate, MMModem *modem, GList *bearers) {
	GList *l;
	struct uci_section *bsec;
	gint retval;
	gint status;

	retval = 0;

	if (!bearers) {
		agh_log_mm_crit("bearers list was empty");
		retval = 20;
		goto out;
	}

	l = bearers;

	for (l = bearers; l; l = g_list_next(l)) {
		bsec = l->data;
		status = agh_mm_config_build_bearer(mstate, modem, bsec, agh_mm_connect_bearer);
		if (status)
			agh_log_mm_crit("failure while building bearer (code=%" G_GINT16_FORMAT")",status);

	}

out:
	return retval;
}

static struct uci_section *agh_mm_select_system_profile(struct agh_state *mstate, MMSim *sim) {
	struct uci_section *section;
	const gchar *operator_id;
	struct uci_element *e;
	struct uci_section *current_section;
	struct uci_option *opt;

	section = NULL;

	if (!mstate || !mstate->mmstate || !mstate->mmstate->mctx || !sim) {
		agh_log_mm_crit("missing context / data");
		return section;
	}

	operator_id = mm_sim_get_operator_identifier(sim);
	if (!operator_id) {
		agh_log_mm_crit("can not obtain operator id");
		return section;
	}

	uci_foreach_element(&mstate->mmstate->uci_package->sections, e) {
		current_section = uci_to_section(e);
		if (!g_strcmp0(current_section->type, AGH_MM_SECTION_BEARER_NAME)) {
			opt = uci_lookup_option(mstate->mmstate->mctx, current_section, AGH_MM_SECTION_BEARER_OPTION_OPERATOR_ID);
			if (opt && opt->type == UCI_TYPE_STRING) {
				if (!g_strcmp0(opt->v.string, operator_id)) {
					section = current_section;
					break;
				}
			}
		}
	}

	return section;
}

static struct uci_section *agh_mm_config_search_system_profiles(struct agh_state *mstate, const gchar *path, MMSim *sim) {
	struct agh_mm_state *mmstate;
	struct uci_section *profile;
	gint retval;

	profile = NULL;

	if (!path || !sim || !mstate || !mstate->mmstate) {
		agh_log_mm_crit("missing context");
		goto out;
	}

	mmstate = mstate->mmstate;

	retval = agh_modem_validate_config(mmstate, path, "sys_connection_settings");
	if (retval) {
		agh_modem_report_gerror_message(&mmstate->current_gerror, NULL);
		goto out;
	}

	profile = agh_mm_select_system_profile(mstate, sim);

out:
	return profile;
}

static gboolean agh_mm_modem_check_connected(struct agh_state *mstate, struct uci_section *modem_section) {
	struct uci_option *connect_opt;
	gboolean should_connect;

	should_connect = TRUE;

	if (!mstate || !mstate->mmstate || !mstate->mmstate->mctx || !modem_section) {
		agh_log_mm_crit("missing context");
		goto out;
	}

	connect_opt = uci_lookup_option(mstate->mmstate->mctx, modem_section, AGH_MM_SECTION_MODEM_OPTION_CONNECT);

	switch(agh_mm_config_get_boolean(connect_opt)) {
		case 0:
			agh_log_mm_dbg("this modem will not be connected because of configuration data");
			should_connect = FALSE;
			break;
		case 1:
			break;
		default:
			agh_log_mm_crit("invalid option");
			break;
	}

out:
	return should_connect;
}

static void agh_mm_add_and_connect_bearers_from_config_check_sim(MMModem *modem, GAsyncResult *res, struct agh_state *mstate) {
	MMSim *sim;
	struct uci_section *sim_section;
	struct uci_section *modem_section;
	GList *bearers_to_build;
	struct uci_section *default_bearer;
	struct uci_section *system_profile_bearer;
	gint retval;
	const gchar **bearer_paths;

	bearers_to_build = NULL;
	sim = NULL;

	sim = mm_modem_get_sim_finish(modem, res, &mstate->mmstate->current_gerror);
	if (!sim) {
		agh_log_mm_crit("unable to get SIM for modem %s while checking for defined bearers",mm_modem_get_path(modem));
		agh_modem_report_gerror_message(&mstate->mmstate->current_gerror, NULL);
		goto out;
	}

	bearer_paths = (const gchar **)mm_modem_get_bearer_paths(modem);
	if (bearer_paths && bearer_paths[0]) {
		agh_log_mm_dbg("not creating bearers for this modem due to already present ones");
		goto out;
	}

	if (!mstate->mmstate->uci_package || (mstate->mmstate->uci_package && g_strcmp0(mstate->mmstate->uci_package->e.name, "agh_modem"))) {
		retval = agh_modem_validate_config(mstate->mmstate, NULL, "agh_modem");
		if (retval) {
			agh_modem_report_gerror_message(&mstate->mmstate->current_gerror, NULL);
			goto out;
		}
	}

	/* do we have a config section for this SIM? */
	sim_section = agh_mm_config_get_sim_section(mstate, modem, sim);

	/* and for this modem? */
	modem_section = agh_mm_config_get_modem_section(mstate, modem);

	if (!agh_mm_modem_check_connected(mstate, modem_section))
		goto out;

	bearers_to_build = agh_mm_config_get_referenced_sections(mstate, sim_section, AGH_MM_SECTION_SIMCARD_OPTION_BEARERSLIST);
	if (!bearers_to_build) {
		agh_log_mm_dbg("no defined bearers found for current SIM card, trying with modem");

		bearers_to_build = agh_mm_config_get_referenced_sections(mstate, modem_section, AGH_MM_SECTION_MODEM_OPTION_BEARERSLIST);
	}

	if (bearers_to_build) {
		agh_log_mm_crit("got %" G_GUINT16_FORMAT" bearers to build",g_list_length(bearers_to_build));
		agh_mm_create_bearers_from_list(mstate, modem, bearers_to_build);
	}
	else {
		agh_log_mm_crit("no connection settings found, trying with default bearer");
		default_bearer = agh_mm_get_default_bearer(mstate);
		if (default_bearer) {
			retval = agh_mm_config_build_bearer(mstate, modem, default_bearer, agh_mm_connect_bearer);
			if (retval)
				agh_log_mm_crit("failure while building default bearer (code=%" G_GINT16_FORMAT")",retval);
		}
		else {
			agh_log_mm_dbg("no default bearer, searching in system profiles");
			system_profile_bearer = agh_mm_config_search_system_profiles(mstate, "/usr/share", sim);
			if (system_profile_bearer) {
				retval = agh_mm_config_build_bearer(mstate, modem, system_profile_bearer, agh_mm_connect_bearer);
				if (retval)
					agh_log_mm_crit("failure while building bearer from a system profile (code=%" G_GINT16_FORMAT")",retval);
			}
		}
	}

out:
	if (bearers_to_build)
		g_list_free(bearers_to_build);

	if (sim)
		g_object_unref(sim);

	return;
}

static gint agh_mm_add_and_connect_bearers_from_config(struct agh_state *mstate, MMModem *modem) {
	gint retval;

	retval = 0;

	if (!modem) {
		agh_log_mm_crit("NULL modem");
		retval = 61;
		goto out;
	}

	agh_log_mm_dbg("obtaining SIM object to check for bearers");

	mm_modem_get_sim(modem, NULL, (GAsyncReadyCallback)agh_mm_add_and_connect_bearers_from_config_check_sim, mstate);

out:
	return retval;
}

static void agh_mm_modem_delete_bearer_finish(MMModem *modem, GAsyncResult *res, gpointer user_data) {
	GError *current_gerror;

	current_gerror = NULL;

	switch(mm_modem_delete_bearer_finish(modem, res, &current_gerror)) {
		case TRUE:
			agh_log_mm_crit("bearer deleted successfully");
			break;
		case FALSE:
			agh_log_mm_crit("can not delete bearer");
			agh_modem_report_gerror_message(&current_gerror, NULL);
			break;
	}

	return;
}

static void agh_mm_modem_delete_bearer(gpointer data, gpointer user_data) {
	MMBearer *b = MM_BEARER(data);
	MMModem *modem = MM_MODEM(user_data);
	const gchar *bpath;

	if (!b) {
		agh_log_mm_crit("found a NULL bearer in list!");
		return;
	}

	bpath = mm_bearer_get_path(b);
	agh_log_mm_dbg("requesting for bearer %s to be deleted", bpath);
	mm_modem_delete_bearer(modem, bpath, NULL, (GAsyncReadyCallback)agh_mm_modem_delete_bearer_finish, b);

	return;
}

static void agh_mm_modem_delete_bearers(GObject *o, GAsyncResult *res, gpointer user_data) {
	struct agh_state *mstate = user_data;
	GList *current_bearers;
	GList *l;
	MMModem *modem = MM_MODEM(o);

	current_bearers = mm_modem_list_bearers_finish(modem, res, &mstate->mmstate->current_gerror);
	if (!current_bearers) {
		agh_log_mm_crit("problem when deleting bearers");
		agh_modem_report_gerror_message(&mstate->mmstate->current_gerror, NULL);
		goto out;
	}

	g_list_foreach(current_bearers, agh_mm_modem_delete_bearer, modem);

	g_list_free_full(current_bearers, g_object_unref);

out:
	return;
}

static gint agh_mm_modem_messaging_deinit(struct agh_state *mstate, MMModem *modem) {
	gint retval;
	guint num_handlers;
	MMObject *mmobject;
	MMModemMessaging *messaging;

	retval = 0;
	mmobject = NULL;
	messaging = NULL;

	if (!mstate || !mstate->mmstate || !mstate->mmstate->manager || !modem) {
		agh_log_mm_crit("messaging deinit can not take place due to missing context or NULL MMModem object passed");
		retval = 20;
		goto out;
	}

	mmobject = agh_mm_get_mmobject(mstate, modem);
	if (!mmobject) {
		agh_log_mm_dbg("unable to get an MMObject for this modem during deinit (%s)",mm_modem_get_path(modem));
		retval = 27;
		goto out;
	}

	messaging = mm_object_get_modem_messaging(mmobject);
	if (!messaging) {
		agh_log_mm_crit("can not get MMModemMessaging object durning deinit");
		retval = 29;
		goto out;
	}

	num_handlers = g_signal_handlers_disconnect_by_func(messaging, agh_mm_modem_sms_added, mstate);
	num_handlers = num_handlers + g_signal_handlers_disconnect_by_func(messaging, agh_mm_modem_sms_deleted, mstate);
	if (!num_handlers) {
		agh_log_mm_crit("no handlers matched during messaging signal disconnect (MMModemMessaging)");
	}
	else
		agh_log_mm_crit("%" G_GINT16_FORMAT" handlers matched during messaging signal disconnect (MMModemMessaging)",num_handlers);

out:
	if (messaging)
		g_object_unref(messaging);

	if (mmobject)
		g_object_unref(mmobject);

	return retval;
}

static gint agh_mm_modem_messaging_init(struct agh_state *mstate, MMModem *modem) {
	MMModemMessaging *messaging;
	gulong signal_id;
	gint retval;
	MMObject *mmobject;

	retval = 0;
	messaging = NULL;
	mmobject = NULL;

	if (!mstate || !mstate->mmstate || !mstate->mmstate->manager || !modem) {
		agh_log_mm_crit("messagin init can not take place due to missing context or NULL MMModem object passed");
		retval = 10;
		goto out;
	}

	mmobject = agh_mm_get_mmobject(mstate, modem);
	if (!mmobject) {
		agh_log_mm_dbg("unable to get an MMObject for this modem (%s)",mm_modem_get_path(modem));
		retval = 17;
		goto out;
	}

	messaging = mm_object_get_modem_messaging(mmobject);
	if (!messaging) {
		agh_log_mm_dbg("no messaging object for %s",mm_modem_get_path(modem));
		retval = 18;
		goto out;
	}

	signal_id = g_signal_connect(messaging, "added", G_CALLBACK(agh_mm_modem_sms_added), mstate);
	if (!signal_id) {
		agh_log_mm_crit("unable to connect added message signal to %s",mm_modem_get_path(modem));
		retval = 19;
		goto out;
	}

	signal_id = g_signal_connect(messaging, "deleted", G_CALLBACK(agh_mm_modem_sms_deleted), mstate);
	if (!signal_id) {
		agh_log_mm_crit("unable to connect deleted message signal to %s",mm_modem_get_path(modem));
		retval = 20;
		goto out;
	}

	agh_log_mm_dbg("messaging OK for %s",mm_modem_get_path(modem));

out:
	if (messaging)
		g_object_unref(messaging);

	if (mmobject)
		g_object_unref(mmobject);

	return retval;
}

static gint agh_mm_modem_signals(struct agh_state *mstate, MMModem *modem, MMModemState oldstate, MMModemState currentstate) {
	if (currentstate < MM_MODEM_STATE_REGISTERED) {
		agh_mm_modem_messaging_deinit(mstate, modem);
	}
	if ((currentstate == MM_MODEM_STATE_REGISTERED) && (oldstate < currentstate)) {
		agh_mm_modem_messaging_init(mstate, modem);
	}

	return 0;
}

static void agh_mm_modem_enable_finish(MMModem *modem, GAsyncResult *res, struct agh_state *mstate) {
	switch(mm_modem_enable_finish(modem, res, &mstate->mmstate->current_gerror)) {
		case TRUE:
			agh_log_mm_dbg("OK");
			break;
		case FALSE:
			agh_log_mm_crit("can not enable modem");
			agh_modem_report_gerror_message(&mstate->mmstate->current_gerror, NULL);
			break;
	}

	return;
}

static gint agh_mm_modem_enable(struct agh_state *mstate, MMModem *modem) {
	gint retval;
	struct uci_section *modem_section;
	struct uci_option *enable_opt;
	gboolean should_enable;

	retval = 0;
	should_enable = TRUE;

	if (!mstate->mmstate->uci_package || (mstate->mmstate->uci_package && g_strcmp0(mstate->mmstate->uci_package->e.name, "agh_modem"))) {
		retval = agh_modem_validate_config(mstate->mmstate, NULL, "agh_modem");
		if (retval) {
			agh_modem_report_gerror_message(&mstate->mmstate->current_gerror, NULL);
		}
	}

	modem_section = agh_mm_config_get_modem_section(mstate, modem);
	if (!modem_section) {
		agh_log_mm_crit("unable to to find a valid configuration section for this modem");
	}
	else {

		enable_opt = uci_lookup_option(mstate->mmstate->mctx, modem_section, AGH_MM_SECTION_MODEM_OPTION_ENABLE);

		switch(agh_mm_config_get_boolean(enable_opt)) {
			case 0:
				agh_log_mm_dbg("modem %s will not be enabled because of configuration",mm_modem_get_path(modem));
				should_enable = FALSE;
				break;
			case 1:
				agh_log_mm_crit("will try to enable modem");
				break;
			default:
				agh_log_mm_crit("invalid option");
				retval = -40;
		}
	}

	if (should_enable)
		mm_modem_enable(modem, NULL, (GAsyncReadyCallback)agh_mm_modem_enable_finish, mstate);

	return retval;
}

static void agh_mm_sim_pin_unlock_finish(MMSim *sim, GAsyncResult *res, struct agh_state *mstate) {
	switch(mm_sim_send_pin_finish(sim, res, &mstate->mmstate->current_gerror)) {
		case TRUE:
			agh_log_mm_dbg("unlock was successful! See you!");
			break;
		case FALSE:
			agh_log_mm_crit("unlock failed!");
			agh_modem_report_gerror_message(&mstate->mmstate->current_gerror, NULL);
			break;
	}

	return;
}

static void agh_mm_sim_pin_unlock_stage1(MMModem *modem, GAsyncResult *res, struct agh_state *mstate) {
	MMSim *sim;
	struct agh_mm_state *mmstate = mstate->mmstate;
	struct uci_section *sim_section;
	struct uci_option *pin_option;
	guint left_pin_retries;
	MMUnlockRetries *retries;
	gint error_code;

	retries = NULL;

	sim = mm_modem_get_sim_finish(modem, res, &mmstate->current_gerror);
	if (!sim) {
		agh_log_mm_crit("unable to get SIM for modem %s",mm_modem_get_path(modem));
		agh_modem_report_gerror_message(&mmstate->current_gerror, NULL);
		goto out;
	}

	if (!mstate->mmstate->uci_package || (mstate->mmstate->uci_package && g_strcmp0(mstate->mmstate->uci_package->e.name, "agh_modem"))) {
		error_code = agh_modem_validate_config(mstate->mmstate, NULL, "agh_modem");
		if (error_code) {
			agh_modem_report_gerror_message(&mstate->mmstate->current_gerror, NULL);
			agh_log_mm_crit("failure when trying to reload config for SIM unlock via PIN (code=%" G_GINT16_FORMAT")",error_code);
			goto out;
		}
	}

	sim_section = agh_mm_config_get_sim_section(mstate, modem, sim);
	if (!sim_section) {
		agh_log_mm_crit("no configuration data found for this SIM card, can not unlock");
		goto out;
	}

	retries = mm_modem_get_unlock_retries(modem);
	if (!retries) {
		agh_log_mm_crit("unable to get unlock retries for this modem");
		goto out;
	}

	left_pin_retries = mm_unlock_retries_get(retries, MM_MODEM_LOCK_SIM_PIN);
	if (left_pin_retries == MM_UNLOCK_RETRIES_UNKNOWN) {
		agh_log_mm_crit("unable to retrieve retries left (we got MM_UNLOCK_RETRIES_UNKNOWN from mm_unlock_retries_get), %" G_GUINT16_FORMAT"", left_pin_retries);
		goto out;
	}

	if (left_pin_retries <= 2) {
		agh_log_mm_crit("insufficient number of retries left (%" G_GUINT16_FORMAT"); consider manual intervention",left_pin_retries);
		goto out;
	}

	pin_option = uci_lookup_option(mstate->mmstate->mctx, sim_section, AGH_MM_SECTION_SIMCARD_OPTION_PIN_CODE);
	if (!pin_option) {
		agh_log_mm_crit("this can not happen, but... we could not get the PIN-related UCI option");
		goto out;
	}

	if (pin_option->type != UCI_TYPE_STRING) {
		agh_log_mm_crit("the PIN code should be an UCI string");
		goto out;
	}

	agh_log_mm_crit("attempting to unlock modem via SIM PIN");
	mm_sim_send_pin(sim, pin_option->v.string, NULL, (GAsyncReadyCallback)agh_mm_sim_pin_unlock_finish, mstate);

out:
	if (sim)
		g_object_unref(sim);

	if (retries)
		g_object_unref(retries);

	return;
}

static gint agh_mm_sim_pin_unlock(struct agh_state *mstate, MMModem *modem) {
	gint retval;

	retval = 0;

	if (!mstate || !mstate->mmstate || !modem) {
		agh_log_mm_crit("missing needed context");
		retval = 22;
		goto out;
	}

	mm_modem_get_sim(modem, NULL, (GAsyncReadyCallback)agh_mm_sim_pin_unlock_stage1, mstate);

out:
	return retval;
}

static gint agh_mm_modem_unlock(struct agh_state *mstate, MMModem *modem) {
	gint retval;
	MMModemLock lock;

	retval = 0;
	lock = mm_modem_get_unlock_required(modem);

	agh_mm_report_event(mstate->comm, AGH_MM_MODEM_EVENT_NAME, agh_mm_modem_to_index(mm_modem_get_path(modem)), mm_modem_lock_get_string(lock));
	agh_log_mm_crit("modem %s is locked (%s)",mm_modem_get_path(modem), mm_modem_lock_get_string(lock));

	switch(lock) {
		case MM_MODEM_LOCK_NONE:
			agh_log_mm_crit("oops, we where called on an unlocked modem");
			retval = 19;
			break;
		case MM_MODEM_LOCK_UNKNOWN:
		case MM_MODEM_LOCK_SIM_PIN2:
		case MM_MODEM_LOCK_SIM_PUK:
		case MM_MODEM_LOCK_SIM_PUK2:
		case MM_MODEM_LOCK_PH_SP_PIN:
		case MM_MODEM_LOCK_PH_SP_PUK:
		case MM_MODEM_LOCK_PH_NET_PIN:
		case MM_MODEM_LOCK_PH_NET_PUK:
		case MM_MODEM_LOCK_PH_SIM_PIN:
		case MM_MODEM_LOCK_PH_CORP_PIN:
		case MM_MODEM_LOCK_PH_CORP_PUK:
		case MM_MODEM_LOCK_PH_FSIM_PIN:
		case MM_MODEM_LOCK_PH_FSIM_PUK:
		case MM_MODEM_LOCK_PH_NETSUB_PIN:
		case MM_MODEM_LOCK_PH_NETSUB_PUK:
			agh_log_mm_crit("do not know how to handle this lock");
			retval = 20;
			break;
		case MM_MODEM_LOCK_SIM_PIN:
			retval = agh_mm_sim_pin_unlock(mstate, modem);
			break;
	}

	return retval;
}

static void agh_mm_statechange(MMModem *modem, MMModemState oldstate, MMModemState newstate, MMModemStateChangeReason reason, gpointer user_data) {
	struct agh_state *mstate = user_data;
	gint retval;

	retval = 0;

	agh_mm_report_event(mstate->comm, AGH_MM_MODEM_EVENT_NAME, agh_mm_modem_to_index(mm_modem_get_path(modem)), mm_modem_state_get_string(oldstate));
	agh_mm_report_event(mstate->comm, AGH_MM_MODEM_EVENT_NAME, agh_mm_modem_to_index(mm_modem_get_path(modem)), mm_modem_state_get_string(newstate));
	agh_mm_report_event(mstate->comm, AGH_MM_MODEM_EVENT_NAME, agh_mm_modem_to_index(mm_modem_get_path(modem)), agh_mm_get_statechange_reason_string(reason));

	retval = agh_mm_modem_signals(mstate, modem, oldstate, newstate);
	if (retval)
		agh_log_mm_crit("failure from agh_mm_modem_signals (code=%" G_GINT16_FORMAT")",retval);

	switch(newstate) {
		case MM_MODEM_STATE_FAILED:
			agh_mm_report_event(mstate->comm, AGH_MM_MODEM_EVENT_NAME, agh_mm_modem_to_index(mm_modem_get_path(modem)), mm_modem_state_failed_reason_get_string(mm_modem_get_state_failed_reason(modem)));
			agh_log_mm_crit("modem %s failed (%s)",mm_modem_get_path(modem),mm_modem_state_failed_reason_get_string(mm_modem_get_state_failed_reason(modem)));
			break;
		case MM_MODEM_STATE_UNKNOWN:
			agh_log_mm_crit("modem %s is in an unknown state, no action is being taken",mm_modem_get_path(modem));
			break;
		case MM_MODEM_STATE_INITIALIZING:
			agh_log_mm_dbg("modem %s is initializing, please wait...",mm_modem_get_path(modem));
			break;
		case MM_MODEM_STATE_LOCKED:
			agh_log_mm_crit("modem %s is currently locked, will try to unlock it",mm_modem_get_path(modem));
			retval = agh_mm_modem_unlock(mstate, modem);

			if (retval)
				agh_log_mm_crit("failure from agh_mm_modem_unlock (code=%" G_GINT16_FORMAT")",retval);

			break;
		case MM_MODEM_STATE_DISABLED:
			if (mstate->exiting) {
				agh_log_mm_dbg("we are exiting, so this modem will not be enabled");
				break;
			}

			retval = agh_mm_modem_bearers(mstate, modem, agh_mm_modem_delete_bearers);
			if (retval)
				agh_log_mm_crit("failure while deleting bearers (code=%" G_GINT16_FORMAT")",retval);

			retval = agh_mm_modem_enable(mstate, modem);
			if (retval)
				agh_log_mm_crit("failure from agh_mm_modem_enable (code=%" G_GINT16_FORMAT")",retval);
			break;
		case MM_MODEM_STATE_DISABLING:
			agh_log_mm_crit("modem %s is being disabled",mm_modem_get_path(modem));
			break;
		case MM_MODEM_STATE_ENABLING:
			agh_log_mm_crit("modem %s is being enabled",mm_modem_get_path(modem));
			break;
		case MM_MODEM_STATE_ENABLED:
			agh_log_mm_crit("modem %s is enabled, waiting for network registration to happen",mm_modem_get_path(modem));
			break;
		case MM_MODEM_STATE_REGISTERED:
			agh_log_mm_crit("modem %s is registered to network!",mm_modem_get_path(modem));
			if (oldstate < newstate) {
				retval = agh_mm_add_and_connect_bearers_from_config(mstate, modem);
				if (retval) {
					agh_log_mm_crit("failure while adding new bearers from configuration data (code=%" G_GINT16_FORMAT")",retval);
				}
			}
			break;
		case MM_MODEM_STATE_SEARCHING:
			agh_log_mm_crit("modem %s is searching...",mm_modem_get_path(modem));
			break;
		case MM_MODEM_STATE_DISCONNECTING:
			agh_log_mm_crit("modem %s is disconnecting from network",mm_modem_get_path(modem));
			break;
		case MM_MODEM_STATE_CONNECTING:
			agh_log_mm_crit("modem %s is connecting to data network",mm_modem_get_path(modem));
			break;
		case MM_MODEM_STATE_CONNECTED:
			agh_log_mm_crit("modem %s is connected!",mm_modem_get_path(modem));
			break;
	}

	return;
}

static gint agh_mm_handle_modem(struct agh_state *mstate, MMObject *modem) {
	MMModem *m;
	gint retval;
	gulong signal_id;

	retval = 0;

	m = mm_object_get_modem(modem);
	if (!m) {
		agh_log_mm_crit("MMObject not implementing MMModem interface");
		retval = 1;
		goto out;
	}

	signal_id = g_signal_connect(m, "state-changed", G_CALLBACK(agh_mm_statechange), mstate);
	if (!signal_id) {
		agh_log_mm_crit("unable to connect state-changed signal");
		retval = 2;
		goto out;
	}

	agh_mm_statechange(m, MM_MODEM_STATE_UNKNOWN, mm_modem_get_state(m), MM_MODEM_STATE_CHANGE_REASON_UNKNOWN, mstate);

out:
	if (m)
		g_object_unref(m);

	return retval;
}

static gint agh_mm_unhandle_modem(struct agh_state *mstate, MMObject *modem) {
	gint retval;
	gint num_handlers;
	MMModem *m;

	retval = 0;

	m = mm_object_get_modem(modem);
	if (!m) {
		agh_log_mm_crit("can not get modem object durning unhandling");
		retval = 14;
		goto out;
	}

	num_handlers = g_signal_handlers_disconnect_by_func(m, agh_mm_statechange, mstate);
	if (!num_handlers) {
		agh_log_mm_crit("no handlers matched during modem signal disconnect (MMModem)");
	}
	else
		agh_log_mm_crit("%" G_GINT16_FORMAT" handlers matched during modem signal disconnect (MMModem)",num_handlers);

out:
	if (m)
		g_object_unref(m);

	return retval;
}

static void agh_mm_device_added(MMManager *manager, MMObject *modem, gpointer user_data) {
	struct agh_state *mstate = user_data;

	agh_log_mm_dbg("modem added");

	agh_mm_handle_modem(mstate, modem);

	return;
}

static void agh_mm_device_removed(MMManager *manager, MMObject *modem, gpointer user_data) {
	struct agh_state *mstate = user_data;

	agh_log_mm_dbg("modem removed");

	agh_mm_unhandle_modem(mstate, modem);

	return;
}

static gint agh_mm_mngr_deinit(struct agh_state *mstate) {
	struct agh_mm_state *mmstate;
	gint retval;

	retval = 0;

	if (!mstate || !mstate->mmstate) {
		agh_log_mm_crit("No AGH state or AGH MM state");
		retval = 20;
		goto out;
	}

	mmstate = mstate->mmstate;

	if (mmstate->manager) {
		agh_log_mm_crit("disconnecting manager signals");

		if (mmstate->manager_signal_modem_added_id) {
			g_signal_handler_disconnect(mmstate->manager, mmstate->manager_signal_modem_added_id);
			mmstate->manager_signal_modem_added_id = 0;
		}

		if (mmstate->manager_signal_modem_removed_id) {
			g_signal_handler_disconnect(mmstate->manager, mmstate->manager_signal_modem_removed_id);
			mmstate->manager_signal_modem_removed_id = 0;
		}

		agh_log_mm_dbg("unreferencing manager object");
		g_object_unref(mmstate->manager);
		mmstate->manager = NULL;
	}

	if (mmstate->bearers_check) {
		agh_log_mm_crit("deactivating checker");
		g_source_destroy(mmstate->bearers_check);
		mmstate->bearers_check = NULL;
		mmstate->bearers_check_tag = 0;
	}

	if (mmstate->current_cmd) {
		agh_log_mm_crit("current_cmd ptr was still present");
		agh_cmd_free(mmstate->current_cmd);
		mmstate->current_cmd = NULL;
	}

out:
	return retval;
}

static gint agh_mm_handle_present_modems(struct agh_state *mstate) {
	GList *l;
	GList *modems;
	gint retval;
	struct agh_mm_state *mmstate;

	retval = 0;

	if (!mstate || !mstate->mmstate || !mstate->mmstate->manager) {
		agh_log_mm_crit("AGH state, AGH MM state or manager object where not present");
		retval = 20;
		goto out;
	}

	mmstate = mstate->mmstate;

	modems = g_dbus_object_manager_get_objects(G_DBUS_OBJECT_MANAGER(mmstate->manager));

	if (!modems) {
		agh_log_mm_dbg("seems no modems have been found (yet?)");
		goto out;
	}

	for (l = modems; l; l = g_list_next(l)) {
		retval = agh_mm_handle_modem(mstate, (MMObject *)(l->data));
		if (retval) {
			agh_log_mm_crit("got failure from agh_mm_handle_modem (code=%" G_GINT16_FORMAT")",retval);
		}
	}

	retval = 0;

	g_list_free_full(modems, g_object_unref);

out:
	return retval;
}

static gint agh_mm_disable_all_modems_sync(struct agh_state *mstate) {
	GList *l;
	GList *modems;
	gint retval;
	struct agh_mm_state *mmstate;
	MMModem *m;

	retval = 0;

	if (!mstate || !mstate->mmstate || !mstate->mmstate->manager) {
		agh_log_mm_crit("AGH state, AGH MM state or manager object where not present");
		retval = 20;
		goto out;
	}

	mmstate = mstate->mmstate;

	modems = g_dbus_object_manager_get_objects(G_DBUS_OBJECT_MANAGER(mmstate->manager));

	if (!modems) {
		agh_log_mm_dbg("seems no modems have been found)");
		retval = 21;
		goto out;
	}

	for (l = modems; l; l = g_list_next(l)) {
		m = mm_object_get_modem(MM_OBJECT(l->data));
		if (m) {
			g_dbus_proxy_set_default_timeout(G_DBUS_PROXY(m), 45 * 1000);
			if (!mm_modem_disable_sync(m, NULL, &mmstate->current_gerror)) {
				agh_log_mm_crit("problem while disabling modem");
				agh_modem_report_gerror_message(&mmstate->current_gerror, NULL);
				retval = 83;
			}
			else {
				agh_log_mm_dbg("modem %s is now disabled",mm_modem_get_path(m));
				retval = agh_mm_modem_delete_all_bearers_sync(mstate, m);
				if (retval) {
					agh_log_mm_crit("got failure from agh_mm_modem_delete_all_bearers_sync (code=%" G_GINT16_FORMAT")",retval);
				}
			}

			g_object_unref(m);
		}
	}

	g_list_free_full(modems, g_object_unref);

out:
	return retval;
}

static void agh_mm_bootstrap(GDBusConnection *connection, GAsyncResult *res, struct agh_state *mstate) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	gint error;

	error = 0;

	if (!mstate || !mstate->mmstate) {
		agh_log_mm_crit("missing context while finishing manager object retrieval");
		error++;
		goto out;
	}

	mmstate->manager = mm_manager_new_finish(res, &mmstate->current_gerror);
	if (!mmstate->manager) {
		agh_modem_report_gerror_message(&mmstate->current_gerror, NULL);
		error++;
		goto out;
	}

	mmstate->manager_signal_modem_added_id = g_signal_connect(mmstate->manager, "object-added", G_CALLBACK(agh_mm_device_added), mstate);
	if (!mmstate->manager_signal_modem_added_id) {
		agh_log_mm_crit("can not connect object-added signal");
		error++;
		goto out;
	}

	mmstate->manager_signal_modem_removed_id = g_signal_connect(mmstate->manager, "object-removed", G_CALLBACK(agh_mm_device_removed), mstate);
	if (!mmstate->manager_signal_modem_removed_id) {
		agh_log_mm_crit("can not connect object-removed signal");
		error++;
		goto out;
	}

	error = agh_mm_handle_present_modems(mstate);
	if (error) {
		agh_log_mm_crit("got failure from agh_mm_handle_present_modems (code=%" G_GINT16_FORMAT")",error);
		goto out;
	}

out:
	if (error)
		agh_mm_mngr_deinit(mstate);

	return;
}

static gint agh_mm_mngr_init(struct agh_state *mstate) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	gint retval;

	retval = 0;

	if (!mmstate) {
		agh_log_mm_crit("no AGH MM state context");
		retval = 40;
		goto out;
	}

	mm_manager_new(mmstate->dbus_connection, G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START, NULL, (GAsyncReadyCallback)agh_mm_bootstrap, mstate);

out:
	return retval;
}

static void agh_mm_start(GDBusConnection *connection, const gchar *name, const gchar *name_owner, gpointer user_data) {
	struct agh_state *mstate = user_data;
	gint retv;

	agh_log_mm_dbg("MM (%s) is now present in the D-Bus system bus (%s)!",name,name_owner);

	retv = agh_mm_mngr_init(mstate);
	if (retv)
		agh_log_mm_crit("manager init will not take place (error code=%" G_GINT16_FORMAT")",retv);

	return;
}

static void agh_mm_stop(GDBusConnection *connection, const gchar *name, gpointer user_data) {
	struct agh_state *mstate = user_data;

	agh_log_mm_dbg("MM (%s) disappeared from the D-Bus system bus",name);

	agh_mm_mngr_deinit(mstate);

	return;
}

static gint agh_mm_watch_deinit(struct agh_state *mstate) {
	gint retval;
	struct agh_mm_state *mmstate;

	retval = 0;

	if (!mstate || !mstate->mmstate) {
		agh_log_mm_crit("no AGH or MM state");
		retval = 15;
		goto out;
	}

	mmstate = mstate->mmstate;

	agh_mm_mngr_deinit(mstate);

	if (mmstate->watch_id) {
		g_bus_unwatch_name(mmstate->watch_id);
		mmstate->watch_id = 0;
		agh_log_mm_crit("no longer watching for MM");
	}

	if (mmstate->dbus_connection) {
		g_object_unref(mmstate->dbus_connection);
		mmstate->dbus_connection = NULL;
		agh_log_mm_crit("D-Bus connection object unreferenced");
	}

	mstate->mainloop_needed--;

out:
	return retval;
}

static gint agh_mm_watch_init(struct agh_state *mstate) {
	gint retval;
	struct agh_mm_state *mmstate;

	retval = 0;

	if (!mstate || !mstate->mmstate) {
		agh_log_mm_crit("N AGH or MM context found");
		retval = 12;
		goto out;
	}

	mmstate = mstate->mmstate;

	mmstate->dbus_connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &mmstate->current_gerror);
	if (!mmstate->dbus_connection) {
		agh_modem_report_gerror_message(&mmstate->current_gerror, NULL);
		retval = 13;
		goto out;
	}

	mstate->mainloop_needed++;

	mmstate->watch_id = g_bus_watch_name_on_connection(mmstate->dbus_connection, AGH_MM_ModemManager_DBUS_NAME, G_BUS_NAME_WATCHER_FLAGS_NONE, agh_mm_start, agh_mm_stop, mstate, NULL);
	if (!mmstate->watch_id) {
		agh_log_mm_crit("failure while starting to watch for ModemManager");
		retval = 14;
		goto out;
	}

out:
	if (retval)
		agh_mm_watch_deinit(mstate);

	return retval;
}

gint agh_mm_deinit(struct agh_state *mstate) {
	struct agh_mm_state *mmstate;
	gint ret;

	ret = 0;

	if (!mstate || !mstate->mmstate) {
		agh_log_mm_crit("no AGH ( / mm ) context found");
		ret = 1;
		goto out;
	}

	mmstate = mstate->mmstate;

	agh_mm_disable_all_modems_sync(mstate);

	if (mmstate->watch_id)
		agh_mm_watch_deinit(mstate);

	mmstate->global_bearer_connecting_lock = FALSE;

	agh_mm_aghcomm = NULL;

	if (mmstate->mctx) {
		uci_unload(mmstate->mctx, mmstate->uci_package);
		uci_free_context(mmstate->mctx);
		mmstate->mctx = NULL;
		mmstate->uci_package = NULL;
	}

	g_free(mmstate);
	mstate->mmstate = NULL;

out:
	return ret;
}

static gint agh_mm_init_ready(struct agh_state *mstate) {
	struct agh_mm_state *mmstate;
	gint ret;

	ret = 0;
	mmstate = NULL;

	if (!mstate || mstate->mmstate) {
		agh_log_mm_crit("no AGH state or MM context already present");
		ret = -11;
		goto out;
	}

	mmstate = g_try_malloc0(sizeof(*mmstate));
	if (!mmstate) {
		agh_log_mm_crit("can not allocate AGH MM state structure");
		ret = -10;
		goto out;
	}

	mstate->mmstate = mmstate;

	ret = agh_modem_validate_config(mmstate, NULL, "agh_modem");
	if (ret) {
		agh_modem_report_gerror_message(&mmstate->current_gerror, NULL);
		goto out;
	}

	ret = agh_mm_watch_init(mstate);
	if (ret) {
		agh_modem_report_gerror_message(&mmstate->current_gerror, NULL);
		goto out;
	}

	ret = agh_modem_set_handler_ext(mstate);
	if (ret) {
		agh_log_mm_crit("got failure from agh_modem_set_handler_ext (code=%" G_GINT16_FORMAT")",ret);
		goto out;
	}

	agh_mm_aghcomm = mstate->comm;

out:

	if (ret)
		agh_mm_deinit(mstate);

	return ret;
}

static gboolean agh_mm_init_check(gpointer data) {
	struct agh_state *mstate = data;
	gint report_retval;

	if (agh_ubus_connection_state == AGH_UBUS_STATE_CONNECTED) {
		agh_log_mm_dbg("ubus connection is present, proceeding with init");
		mstate->ubus_wait_src = NULL;
		mstate->ubus_wait_src_tag = 0;

		report_retval = agh_mm_init_ready(mstate);
		if (report_retval)
			agh_log_mm_crit("failure from agh_mm_init_ready (code=%" G_GINT16_FORMAT")", report_retval);

		return FALSE;
	}

	return TRUE;
}

gint agh_mm_init(struct agh_state *mstate) {
	gint retval;

	retval = 0;

	if (!mstate || mstate->ubus_wait_src || mstate->ubus_wait_src_tag) {
		agh_log_mm_crit("missing or unexpected AGH state");
		retval = 15;
		goto out;
	}

	mstate->ubus_wait_src = g_timeout_source_new(5000);
	g_source_set_callback(mstate->ubus_wait_src, agh_mm_init_check, mstate, NULL);

	/* attach timeout GSource to GMainContext */
	mstate->ubus_wait_src_tag = g_source_attach(mstate->ubus_wait_src, mstate->ctx);
	g_source_unref(mstate->ubus_wait_src);
	if (!mstate->ubus_wait_src_tag) {
		agh_log_mm_crit("unable to attach timeout GSource to GMainContext");
		retval = 14;
		mstate->ubus_wait_src = NULL;
		goto out;
	}

out:
	return retval;
}

gint agh_modem_report_gerror_message(GError **error, struct agh_comm *comm) {
	gint retval;

	retval = 0;

	if (!error || !*error) {
		agh_log_mm_crit("asked to report a NULL GError, or one pointing to a NULL pointer");
		retval = 1;
		goto out;
	}

	agh_log_mm_crit("(GError report) - %s",(*error)->message ? (*error)->message : "(no error message)");

	if (comm)
		agh_mm_report_event(comm, "GError", NULL, (*error)->message);

	g_error_free(*error);
	*error = NULL;

out:
	return retval;
}

void agh_mm_testwait(gint secs) {
	agh_log_mm_crit("plug/unplug the modem, or do what you feel like");
	g_usleep(secs*G_USEC_PER_SEC);
	return;
}

gint agh_mm_report_event(struct agh_comm *comm, const gchar *evname, gchar *evpath, const gchar *evtext) {
	gint retval;
	struct agh_cmd *ev;

	retval = 0;

	if (!comm || !evname || !evtext) {
		agh_log_mm_crit("no AGH COMM, or NULL event name / event text");
		retval = 25;
		goto out;
	}

	ev = agh_cmd_event_alloc(&retval);
	if (!ev) {
		agh_log_mm_crit("AGH event allocation failure (code=%" G_GINT16_FORMAT")",retval);
		retval = 26;
		goto out;
	}

	agh_cmd_answer_set_status(ev, AGH_CMD_ANSWER_STATUS_OK);
	agh_cmd_answer_addtext(ev, evname, TRUE);

	if (evpath)
		agh_cmd_answer_addtext(ev, evpath, FALSE);

	agh_cmd_answer_addtext(ev, evtext, TRUE);

	retval = agh_cmd_emit_event(comm, ev);

out:
	if (retval) {
		agh_log_mm_crit("event could not be emitted (code=%" G_GINT16_FORMAT")",retval);
		g_free(evpath);
	}

	return retval;
}
