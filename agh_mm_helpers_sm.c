#include "agh_mm_helpers_sm.h"

/* Function prototypes. */
static void agh_mm_sm_sim_unlock_sim_for_pin_send_ready(MMModem *modem, GAsyncResult *res, struct agh_state *mstate);
static void agh_mm_sm_report_error(struct agh_state *mstate, gchar *message);
static GList *agh_mm_sm_build_simlist(struct agh_state *mstate, struct uci_section *section);
static void agh_mm_sm_sim_unlock_send_pin_res(MMSim *sim, GAsyncResult *res, struct agh_state *mstate);
static void agh_mm_sm_report(struct agh_state *mstate, guint status, gchar *eventname, gchar *mmarker, gchar *name, gchar *reason, gboolean is_data);
static gint agh_mm_sm_build_bearer_set_iptype(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props);
static gint agh_mm_sm_build_bearer_set_apn(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props);
static gint agh_mm_sm_build_bearer_set_auth_method(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props);
static gint agh_mm_sm_build_bearer_set_user(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props);
static gint agh_mm_sm_build_bearer_set_pass(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props);
static gint agh_mm_sm_build_bearer_set_roaming_allowed(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props);
static gint agh_mm_sm_build_bearer_set_number(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props);
static gint agh_mm_sm_build_bearer_set_rm_protocol(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props);
static void agh_mm_sm_connect_bearer(MMModem *modem, GAsyncResult *res, gpointer user_data);
static void agh_mm_sm_bearer_connected_changed(MMBearer *b, GParamSpec *pspec, gpointer user_data);
static void agh_mm_sm_bearer_connected_notify_outside(MMBearer *b, GParamSpec *pspec, gpointer user_data);
static void agh_mm_sm_keep_trying_to_connect(MMBearer *b, GAsyncResult *res, gpointer user_data);
static void agh_mm_sm_keep_trying_to_connect_from_signal(MMBearer *b, GAsyncResult *res, gpointer user_data);
static void agh_mm_sm_apply_general_modem_defaults(struct agh_state *mstate, MMModem *modem);
static void agh_mm_sm_general_init_propschanges(struct agh_state *mstate, struct uci_option *opt, MMModem *modem);
static void agh_mm_sm_properties_changed(MMModem *modem, GVariant *changed_props, GStrv inv_props, gpointer user_data);
static void agh_mm_sm_call_outside_helper(struct agh_state *mstate, MMBearer *b);
static gchar *agh_mm_sm_call_outside_build_message(struct agh_state *mstate, MMBearer *b);
static gchar *agh_mm_sm_call_outside_build_message_add_element(const gchar *name, const gchar *value, gboolean last);

void agh_mm_report_failed_reason(struct agh_state *mstate, MMModem *modem) {
	struct command *event;
	gchar *modem_idx;

	event = NULL;
	modem_idx = agh_mm_modem_to_index(mm_modem_get_path(modem));

	event = cmd_event_prepare();
	cmd_answer_set_status(event, CMD_ANSWER_STATUS_FAIL);
	cmd_answer_addtext(event, AGH_MM_MODEM_EVENT_NAME);
	cmd_answer_addtext(event, modem_idx);
	g_free(modem_idx);
	modem_idx = NULL;
	cmd_answer_addtext(event, AGH_MM_SM_MODEM_INIITSTATE_FAILURE);
	cmd_answer_addtext(event, AGH_MM_VALIDATE_UNKNOWN(mm_modem_state_get_string(mm_modem_get_state(modem))));
	cmd_answer_addtext(event, AGH_MM_VALIDATE_UNKNOWN(mm_modem_state_failed_reason_get_string(mm_modem_get_state_failed_reason(modem))));
	cmd_emit_event(mstate->comm, event);

	return;
}

void agh_mm_report_locked_reason(struct agh_state *mstate, MMModem *modem) {
	struct command *event;
	gchar *modem_idx;

	event = NULL;
	modem_idx = agh_mm_modem_to_index(mm_modem_get_path(modem));

	event = cmd_event_prepare();
	cmd_answer_set_status(event, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(event, AGH_MM_MODEM_EVENT_NAME);
	cmd_answer_addtext(event, modem_idx);
	g_free(modem_idx);
	modem_idx = NULL;
	cmd_answer_addtext(event, "locknotice");
	cmd_answer_addtext(event, AGH_MM_VALIDATE_UNKNOWN(mm_modem_state_get_string(mm_modem_get_state(modem))));
	cmd_answer_addtext(event, AGH_MM_VALIDATE_UNKNOWN(mm_modem_lock_get_string(mm_modem_get_unlock_required(modem))));
	cmd_emit_event(mstate->comm, event);

	return;
}

void agh_mm_sm_sim_unlock(struct agh_state *mstate, MMModem *modem, MMModemLock l) {
	switch(l) {
		case MM_MODEM_LOCK_SIM_PIN:
			mm_modem_get_sim(modem, NULL, (GAsyncReadyCallback)agh_mm_sm_sim_unlock_sim_for_pin_send_ready, mstate);
			break;
		default:
			break;
	}

	return;
}

static void agh_mm_sm_sim_unlock_sim_for_pin_send_ready(MMModem *modem, GAsyncResult *res, struct agh_state *mstate) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	MMSim *sim;
	struct uci_section *section;
	struct uci_option *pin_option;

	sim = NULL;
	section = NULL;
	pin_option = NULL;

	sim = mm_modem_get_sim_finish(modem, res, &mmstate->gerror);
	if (!sim) {
		agh_mm_sm_report_error(mstate, NULL);
		return;
	}

	section = agh_mm_sm_get_sim_section(mstate, modem, sim);
	if (section) {
		pin_option = uci_lookup_option(mstate->mmstate->mctx, section, AGH_MM_SECTION_SIMCARD_OPTION_PIN_CODE);

		if (pin_option && (pin_option->type == UCI_TYPE_STRING)) {
			mm_sim_send_pin(sim, pin_option->v.string, NULL, (GAsyncReadyCallback)agh_mm_sm_sim_unlock_send_pin_res, mstate);
		}
	}

	g_object_unref(sim);
	sim = NULL;
	return;
}

static void agh_mm_sm_report_error(struct agh_state *mstate, gchar *message) {
	struct command *event;
	struct agh_mm_state *mmstate = mstate->mmstate;

	event = NULL;

	if ((!message) && (!mmstate->gerror))
		return;

	event = cmd_event_prepare();
	cmd_answer_set_status(event, CMD_ANSWER_STATUS_FAIL);
	cmd_answer_addtext(event, AGH_MM_SM_GENERIC_FAILURE);

	if (message)
		cmd_answer_addtext(event, message);
	else if (mmstate->gerror) {
		if (mmstate->gerror->message)
			cmd_answer_addtext(event, mmstate->gerror->message);
		else
			cmd_answer_addtext(event, "unknown");

		g_error_free(mmstate->gerror);
		mmstate->gerror = NULL;
	}
	else
		cmd_answer_addtext(event, "unknown");

	cmd_emit_event(mstate->comm, event);

	return;
}

struct uci_section *agh_mm_sm_get_sim_section(struct agh_state *mstate, MMModem *modem, MMSim *sim) {
	struct uci_section *modem_section;
	struct uci_section *sim_section;
	GList *simlist;
	GList *process_simlist;
	const gchar *sim_id;
	struct uci_option *opt;
	struct uci_section *res_section;

	modem_section = NULL;
	sim_section = NULL;
	simlist = NULL;
	process_simlist = NULL;
	sim_id = NULL;
	opt = NULL;
	res_section = NULL;

	modem_section = agh_mm_sm_get_modem_section(mstate, modem);

	simlist = agh_mm_sm_build_simlist(mstate, modem_section);

	if (!simlist)
		return NULL;

	sim_id = mm_sim_get_identifier(sim);
	if (!sim_id) {
		g_list_free(simlist);
		g_print("%s: can not match against a NULL sim ID\n",__FUNCTION__);
		agh_mm_sm_report_failure_modem(mstate, modem, AGH_MM_SM_MODEM_INIITSTATE_FAILURE_NO_SIM_ID);
		return NULL;
	}

	g_print("%s: sim id is %s\n",__FUNCTION__,sim_id);
	for (process_simlist = simlist; process_simlist; process_simlist = g_list_next(simlist)) {
		sim_section = process_simlist->data;
		opt = uci_lookup_option(mstate->mmstate->mctx, sim_section, AGH_MM_SECTION_SIMCARD_OPTION_SIM_ID);

		/* should exist and be a string */
		if (!g_strcmp0(opt->v.string, sim_id)) {
			res_section = sim_section;
			break;
		}
	}

	g_list_free(simlist);
	return res_section;
}

struct uci_section *agh_mm_sm_get_modem_section(struct agh_state *mstate, MMModem *modem) {
	struct uci_section *section;
	const gchar *equipment_id;
	struct uci_element *e;
	struct uci_option *opt;

	section = NULL;
	equipment_id = NULL;
	e = NULL;
	opt = NULL;

	equipment_id = mm_modem_get_equipment_identifier(modem);
	if (!equipment_id) {
		agh_mm_sm_report_error(mstate, "modem with no equipment identifier?");
		return NULL;
	}

	uci_foreach_element(&mstate->mmstate->package->sections, e) {
		section = uci_to_section(e);

		if (!g_strcmp0(section->type, AGH_MM_SECTION_MODEM_NAME)) {
			opt = uci_lookup_option(mstate->mmstate->mctx, section, AGH_MM_SECTION_MODEM_OPTION_EQUIPMENT_ID);

			/* validation should guarantee this option is present, and is a string */
			if (!g_strcmp0(opt->v.string, equipment_id))
				return section;

		}

	}

	return NULL;
}

static GList *agh_mm_sm_build_simlist(struct agh_state *mstate, struct uci_section *section) {
	GList *l;
	struct uci_option *opt;
	struct uci_element *e;
	struct uci_section *sim_section;

	l = NULL;
	opt = NULL;
	sim_section = NULL;
	e = NULL;

	/* Validation guarantees that, if a SIM has been "referenced" in a modem section, then it should exist in the config. */
	if ((section) && (!g_strcmp0(section->type, AGH_MM_SECTION_MODEM_NAME))) {
		opt = uci_lookup_option(mstate->mmstate->mctx, section, AGH_MM_SECTION_MODEM_OPTION_SIMLIST_NAME);
		if ((opt) && (opt->type == UCI_TYPE_LIST)) {
			uci_foreach_element(&opt->v.list, e) {
				sim_section = uci_lookup_section(mstate->mmstate->mctx, mstate->mmstate->package, e->name);

				g_assert(sim_section);
				l = g_list_append(l, sim_section);
			}
		}
		else {
			uci_foreach_element(&mstate->mmstate->package->sections, e) {
				sim_section = uci_to_section(e);
				if (!g_strcmp0(sim_section->type, AGH_MM_SECTION_SIMCARD_NAME)) {
					l = g_list_append(l, sim_section);
				}
			}
		}
	}

	return l;
}

static void agh_mm_sm_sim_unlock_send_pin_res(MMSim *sim, GAsyncResult *res, struct agh_state *mstate) {
	gboolean sres;
	struct command *event;

	sres = FALSE;
	event = NULL;

	sres = mm_sim_send_pin_finish(sim, res, &mstate->mmstate->gerror);

	if (!sres) {
		event = cmd_event_prepare();
		cmd_answer_set_status(event, CMD_ANSWER_STATUS_FAIL);
		cmd_answer_addtext(event, AGH_MM_SIM_EVENT_NAME);
		cmd_answer_addtext(event, mm_sim_get_identifier(sim));
		cmd_answer_addtext(event, "send_pin_failure");

		if (mstate->mmstate->gerror) {
			if (mstate->mmstate->gerror->message)
				cmd_answer_addtext(event, mstate->mmstate->gerror->message);
			else
				cmd_answer_addtext(event, "unknown");

			g_error_free(mstate->mmstate->gerror);
			mstate->mmstate->gerror = NULL;
		}
		else
			cmd_answer_addtext(event, "unknown");

		cmd_emit_event(mstate->comm, event);
	}

	return;
}

void agh_mm_sm_report_failure_modem(struct agh_state *mstate, MMModem *modem, gchar *mmarker) {
	struct command *event;
	gchar *modem_idx;

	event = NULL;
	modem_idx = agh_mm_modem_to_index(mm_modem_get_path(modem));

	event = cmd_event_prepare();
	cmd_answer_set_status(event, CMD_ANSWER_STATUS_FAIL);
	cmd_answer_addtext(event, AGH_MM_MODEM_EVENT_NAME);
	cmd_answer_addtext(event, modem_idx);
	g_free(modem_idx);
	modem_idx = NULL;
	if (mmarker)
		cmd_answer_addtext(event, mmarker);
	else
		cmd_answer_addtext(event, AGH_MM_SM_MODEM_INIITSTATE_FAILURE);

	if (mstate->mmstate->gerror) {
		if (mstate->mmstate->gerror->message)
			cmd_answer_addtext(event, mstate->mmstate->gerror->message);
		else
			cmd_answer_addtext(event, "unknown");

		g_error_free(mstate->mmstate->gerror);
		mstate->mmstate->gerror = NULL;
	}
	else
		cmd_answer_addtext(event, "unknown");

	cmd_emit_event(mstate->comm, event);

	return;
}

GList *agh_mm_sm_get_referenced_sections(struct agh_state *mstate, struct uci_section *section, gchar *section_name) {
	GList *slist;
	struct uci_section *current_section;
	struct uci_option *current_list_option;
	struct uci_element *e;

	slist = NULL;
	current_section = NULL;
	e = NULL;
	current_list_option = NULL;

	if ((!section) || (!section_name))
		return slist;

	current_list_option = uci_lookup_option(mstate->mmstate->mctx, section, section_name);
	if ((current_list_option) && (current_list_option->type == UCI_TYPE_LIST)) {
		uci_foreach_element(&current_list_option->v.list, e) {
			current_section = uci_lookup_section(mstate->mmstate->mctx, mstate->mmstate->package, e->name);
			g_assert(current_section);
			slist = g_list_append(slist, current_section);
		}
	}

	return slist;
}

struct uci_section *agh_mm_sm_get_default_bearer(struct agh_state *mstate) {
	struct uci_section *default_bearer_section;
	struct uci_element *e;
	struct uci_section *current_section;

	default_bearer_section = NULL;
	e = NULL;
	current_section = NULL;

	uci_foreach_element(&mstate->mmstate->package->sections, e) {
		current_section = uci_to_section(e);

		if (!g_strcmp0(current_section->e.name, "default"))
			if (!g_strcmp0(current_section->type, AGH_MM_SECTION_BEARER_NAME)) {
				default_bearer_section = current_section;
				break;
			}
	}

	return default_bearer_section;
}

void agh_mm_sm_build_bearer(struct agh_state *mstate, MMModem *modem, struct uci_section *s) {
	struct agh_mm_state *mmstate = mstate->mmstate;
	struct uci_option *o;
	MMBearerProperties *props;
	gint status;

	o = NULL;
	props = NULL;
	status = 0;

	props = mm_bearer_properties_new();

	/* IP family */
	o = uci_lookup_option(mmstate->mctx, s, AGH_MM_SECTION_BEARER_OPTION_IP_TYPE);
	if (o && (o->type == UCI_TYPE_STRING)) {
		status = agh_mm_sm_build_bearer_set_iptype(mstate, o, props);
		if (status) {
			agh_mm_sm_report(mstate, CMD_ANSWER_STATUS_FAIL, AGH_MM_BEARER_EVENT_NAME, AGH_MM_SM_BEARER_SETUP_FAILURE, s->e.name, "agh_mm_sm_build_bearer_set_iptype", FALSE);
			goto out;
		}

	}

	/* APN */
	o = uci_lookup_option(mmstate->mctx, s, AGH_MM_SECTION_BEARER_OPTION_APN);
	if (o && (o->type == UCI_TYPE_STRING)) {
		status = agh_mm_sm_build_bearer_set_apn(mstate, o, props);
		if (status) {
			agh_mm_sm_report(mstate, CMD_ANSWER_STATUS_FAIL, AGH_MM_BEARER_EVENT_NAME, AGH_MM_SM_BEARER_SETUP_FAILURE, s->e.name, "agh_mm_sm_build_bearer_set_apn", FALSE);
			goto out;
		}

	}

	/* auth type */
	o = uci_lookup_option(mmstate->mctx, s, AGH_MM_SECTION_BEARER_OPTION_AUTH_METHOD);
	if (o && (o->type == UCI_TYPE_STRING)) {
		status = agh_mm_sm_build_bearer_set_auth_method(mstate, o, props);
		if (status) {
			agh_mm_sm_report(mstate, CMD_ANSWER_STATUS_FAIL, AGH_MM_BEARER_EVENT_NAME, AGH_MM_SM_BEARER_SETUP_FAILURE, s->e.name, "agh_mm_sm_build_bearer_set_auth_method", FALSE);
			goto out;
		}

	}

	/* user */
	o = uci_lookup_option(mmstate->mctx, s, AGH_MM_SECTION_BEARER_OPTION_USERNAME);
	if (o && (o->type == UCI_TYPE_STRING)) {
		status = agh_mm_sm_build_bearer_set_user(mstate, o, props);
		if (status) {
			agh_mm_sm_report(mstate, CMD_ANSWER_STATUS_FAIL, AGH_MM_BEARER_EVENT_NAME, AGH_MM_SM_BEARER_SETUP_FAILURE, s->e.name, "agh_mm_sm_build_bearer_set_user", FALSE);
			goto out;
		}

	}

	/* password */
	o = uci_lookup_option(mmstate->mctx, s, AGH_MM_SECTION_BEARER_OPTION_PASSWORD);
	if (o && (o->type == UCI_TYPE_STRING)) {
		status = agh_mm_sm_build_bearer_set_pass(mstate, o, props);
		if (status) {
			agh_mm_sm_report(mstate, CMD_ANSWER_STATUS_FAIL, AGH_MM_BEARER_EVENT_NAME, AGH_MM_SM_BEARER_SETUP_FAILURE, s->e.name, "agh_mm_sm_build_bearer_set_pass", FALSE);
			goto out;
		}

	}

	/* allow roaming? */
	o = uci_lookup_option(mmstate->mctx, s, AGH_MM_SECTION_BEARER_OPTION_ALLOW_ROAMING);
	if (o && (o->type == UCI_TYPE_STRING)) {
		status = agh_mm_sm_build_bearer_set_roaming_allowed(mstate, o, props);
		if (status) {
			agh_mm_sm_report(mstate, CMD_ANSWER_STATUS_FAIL, AGH_MM_BEARER_EVENT_NAME, AGH_MM_SM_BEARER_SETUP_FAILURE, s->e.name, "agh_mm_sm_build_bearer_set_roaming_allowed", FALSE);
			goto out;
		}

	}

	/* number */
	o = uci_lookup_option(mmstate->mctx, s, AGH_MM_SECTION_BEARER_OPTION_NUMBER);
	if (o && (o->type == UCI_TYPE_STRING)) {
		status = agh_mm_sm_build_bearer_set_number(mstate, o, props);
		if (status) {
			agh_mm_sm_report(mstate, CMD_ANSWER_STATUS_FAIL, AGH_MM_BEARER_EVENT_NAME, AGH_MM_SM_BEARER_SETUP_FAILURE, s->e.name, "agh_mm_sm_build_bearer_set_number", FALSE);
			goto out;
		}

	}

	/* rm protocol */
	o = uci_lookup_option(mmstate->mctx, s, AGH_MM_SECTION_BEARER_OPTION_RM_PROTOCOL);
	if (o && (o->type == UCI_TYPE_STRING)) {
		status = agh_mm_sm_build_bearer_set_rm_protocol(mstate, o, props);
		if (status) {
			agh_mm_sm_report(mstate, CMD_ANSWER_STATUS_FAIL, AGH_MM_BEARER_EVENT_NAME, AGH_MM_SM_BEARER_SETUP_FAILURE, s->e.name, "agh_mm_sm_build_bearer_set_rm_protocol", FALSE);
			goto out;
		}

	}

	g_print("%s: connecting %s\n",__FUNCTION__,s->e.name);

	mm_modem_create_bearer(modem, props, NULL, (GAsyncReadyCallback)agh_mm_sm_connect_bearer, mstate);
	g_object_unref(props);
	props = NULL;
	return;

out:
	g_object_unref(props);
	props = NULL;
	return;
}

static void agh_mm_sm_report(struct agh_state *mstate, guint status, gchar *eventname, gchar *mmarker, gchar *name, gchar *reason, gboolean is_data) {
	struct command *event;

	event = NULL;

	event = cmd_event_prepare();
	cmd_answer_set_status(event, status);
	cmd_answer_addtext(event, eventname);

	if (mmarker)
		cmd_answer_addtext(event, mmarker);
	else
		cmd_answer_addtext(event, AGH_MM_SM_MODEM_INIITSTATE_FAILURE);

	cmd_answer_addtext(event, name);
	cmd_answer_addtext(event, reason);

	if (is_data)
		cmd_answer_set_data(event, is_data);

	cmd_emit_event(mstate->comm, event);

	return;
}

static gint agh_mm_sm_build_bearer_set_iptype(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props) {
	MMBearerIpFamily ipf;

	if (!g_strcmp0(o->v.string, "IPV4"))
		ipf = MM_BEARER_IP_FAMILY_IPV4;
	else if (!g_strcmp0(o->v.string, "IPV6"))
		ipf = MM_BEARER_IP_FAMILY_IPV6;
	else if (!g_strcmp0(o->v.string, "IPV4V6"))
		ipf = MM_BEARER_IP_FAMILY_IPV4V6;
	else if (!g_strcmp0(o->v.string, "none"))
		ipf = MM_BEARER_IP_FAMILY_NONE;
	else if (!g_strcmp0(o->v.string, "any"))
		ipf = MM_BEARER_IP_FAMILY_ANY;
	else
		return 1;

	mm_bearer_properties_set_ip_type(props, ipf);

	return 0;
}

static gint agh_mm_sm_build_bearer_set_apn(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props) {
	mm_bearer_properties_set_apn(props, o->v.string);
	return 0;
}

static gint agh_mm_sm_build_bearer_set_auth_method(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props) {
	gint status;
	MMBearerAllowedAuth ah;

	if (!g_strcmp0(o->v.string, "none"))
		ah = MM_BEARER_ALLOWED_AUTH_NONE;
	else if (!g_strcmp0(o->v.string, "PAP"))
		ah = MM_BEARER_ALLOWED_AUTH_PAP;
	else if (!g_strcmp0(o->v.string, "CHAP"))
		ah = MM_BEARER_ALLOWED_AUTH_CHAP;
	else if (!g_strcmp0(o->v.string, "MSCHAP"))
		ah = MM_BEARER_ALLOWED_AUTH_MSCHAP;
	else if (!g_strcmp0(o->v.string, "MSCHAPV2"))
		ah = MM_BEARER_ALLOWED_AUTH_MSCHAPV2;
	else if (!g_strcmp0(o->v.string, "EAP"))
		ah = MM_BEARER_ALLOWED_AUTH_EAP;
	else
		return 1;

	mm_bearer_properties_set_allowed_auth(props, ah);

	return 0;
}

static gint agh_mm_sm_build_bearer_set_user(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props) {
	mm_bearer_properties_set_user(props, o->v.string);
	return 0;
}

static gint agh_mm_sm_build_bearer_set_pass(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props) {
	mm_bearer_properties_set_password(props, o->v.string);
	return 0;
}

static gint agh_mm_sm_build_bearer_set_roaming_allowed(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props) {

	if (!g_strcmp0(o->v.string, "1"))
		mm_bearer_properties_set_allow_roaming(props, TRUE);
	else if (!g_strcmp0(o->v.string, "0"))
		mm_bearer_properties_set_allow_roaming(props, FALSE);
	else
		return 1;

	return 0;
}

static gint agh_mm_sm_build_bearer_set_number(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props) {
	mm_bearer_properties_set_password(props, o->v.string);
	return 0;
}

static gint agh_mm_sm_build_bearer_set_rm_protocol(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props) {
	MMModemCdmaRmProtocol rm_protocol;

	if (!g_strcmp0(o->v.string, "rm_protocol_async"))
		rm_protocol = MM_MODEM_CDMA_RM_PROTOCOL_ASYNC;
	else if (!g_strcmp0(o->v.string, "rm_protocol_packet_relay"))
		rm_protocol = MM_MODEM_CDMA_RM_PROTOCOL_PACKET_RELAY;
	else if (!g_strcmp0(o->v.string, "rm_protocol_packet_network_ppp"))
		rm_protocol = MM_MODEM_CDMA_RM_PROTOCOL_PACKET_NETWORK_PPP;
	else if (!g_strcmp0(o->v.string, "rm_protocol_packet_network_slip"))
		rm_protocol = MM_MODEM_CDMA_RM_PROTOCOL_PACKET_NETWORK_SLIP;
	else if (!g_strcmp0(o->v.string,"rm_protocol_stu_iii"))
		rm_protocol = MM_MODEM_CDMA_RM_PROTOCOL_STU_III;
	else
		return 1;

	mm_bearer_properties_set_rm_protocol(props, rm_protocol);

	return 0;
}

static void agh_mm_sm_connect_bearer(MMModem *modem, GAsyncResult *res, gpointer user_data) {
	struct agh_state *mstate = user_data;
	MMBearer *b;
	struct agh_mm_asyncstate *a;

	b = NULL;
	a = NULL;

	b = mm_modem_create_bearer_finish(modem, res, &mstate->mmstate->gerror);
	if (!b) {
		agh_mm_sm_report_failure_modem(mstate, modem, AGH_MM_SM_BEARER_SETUP_FAILURE);
		return;
	}

	a = g_malloc0(sizeof(struct agh_mm_asyncstate));
	a->mstate = mstate;
	a->modem = modem;
	g_signal_connect(b, "notify::connected", G_CALLBACK(agh_mm_sm_bearer_connected_changed), modem);
	g_signal_connect(b, "notify::connected", G_CALLBACK(agh_mm_sm_bearer_connected_notify_outside), mstate);

	mm_bearer_connect(b, NULL, (GAsyncReadyCallback)agh_mm_sm_keep_trying_to_connect, a);

	return;
}

static void agh_mm_sm_bearer_connected_changed(MMBearer *b, GParamSpec *pspec, gpointer user_data) {
	gboolean f;
	MMModem *modem = user_data;
	MMModemState modem_state;

	f = mm_bearer_get_connected(b);

	if (f)
		g_print("%s: bearer is connected\n",__FUNCTION__);
	else {
		g_print("%s: bearer is disconnected\n",__FUNCTION__);

		modem_state = mm_modem_get_state(modem);
		switch(modem_state) {
			case MM_MODEM_STATE_CONNECTED:
				g_print("%s: trying to reconnect\n",__FUNCTION__);
				mm_bearer_connect(b, NULL, (GAsyncReadyCallback)agh_mm_sm_keep_trying_to_connect_from_signal, modem);
				break;
			default:
				g_print("%s: letting go bearer due to current modem state\n",__FUNCTION__);
				g_object_unref(b);
		}
	}

	return;
}

static void agh_mm_sm_bearer_connected_notify_outside(MMBearer *b, GParamSpec *pspec, gpointer user_data) {
	struct agh_state *mstate = user_data;
	gboolean f;

	agh_mm_sm_call_outside_helper(mstate, b);

	return;
}

static void agh_mm_sm_keep_trying_to_connect(MMBearer *b, GAsyncResult *res, gpointer user_data) {
	struct agh_mm_asyncstate *a = user_data;
	gboolean success;
	gboolean stop_retrying;

	stop_retrying = FALSE;

	success = mm_bearer_connect_finish(b, res, &a->mstate->mmstate->gerror);
	if (!success) {
		stop_retrying = g_error_matches(a->mstate->mmstate->gerror, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD);
		agh_mm_sm_report_failure_modem(a->mstate, a->modem, AGH_MM_SM_BEARER_CONNECT_FAILURE);

		/* retry */
		if (!stop_retrying) {
			mm_bearer_connect(b, NULL, (GAsyncReadyCallback)agh_mm_sm_keep_trying_to_connect, a);
			return;
		}
	}
	g_free(a);
	a = NULL;
	return;
}

static void agh_mm_sm_keep_trying_to_connect_from_signal(MMBearer *b, GAsyncResult *res, gpointer user_data) {
	MMModem *modem = user_data;
	gboolean success;
	GError *gerror;
	gboolean stop_retrying;

	gerror = NULL;
	stop_retrying = FALSE;

	success = mm_bearer_connect_finish(b, res, &gerror);
	if (!success) {

		if (gerror) {
			g_print("%s: can not connect bearer; %s\n",__FUNCTION__,gerror->message ? gerror->message : "unknown error");
			stop_retrying = g_error_matches(gerror, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD);
			g_error_free(gerror);
			gerror = NULL;
		}

		/* retry */
		if (!stop_retrying)
			mm_bearer_connect(b, NULL, (GAsyncReadyCallback)agh_mm_sm_keep_trying_to_connect_from_signal, modem);

		return;
	}
	else
		g_print("%s: we are back\n",__FUNCTION__);

	return;
}

void agh_mm_sm_general_init(struct agh_state *mstate, MMModem *modem) {
	struct uci_section *modem_section;
	struct uci_option *modem_option;

	modem_section = NULL;
	modem_option = NULL;

	modem_section = agh_mm_sm_get_modem_section(mstate, modem);
	if (!modem_section) {
		agh_mm_sm_apply_general_modem_defaults(mstate, modem);
		return;
	}

	modem_option = uci_lookup_option(mstate->mmstate->mctx, modem_section, AGH_MM_SECTION_MODEM_OPTION_REPORT_PROPSCHANGES);
	if (modem_option)
		agh_mm_sm_general_init_propschanges(mstate, modem_option, modem);

	return;
}

static void agh_mm_sm_apply_general_modem_defaults(struct agh_state *mstate, MMModem *modem) {
	return;
}

static void agh_mm_sm_general_init_propschanges(struct agh_state *mstate, struct uci_option *opt, MMModem *modem) {
	if (opt->type != UCI_TYPE_STRING)
		return;

	if (!g_strcmp0(opt->v.string, "1")) {
		g_signal_handlers_disconnect_by_func(modem, agh_mm_sm_properties_changed, mstate);
		g_signal_connect(modem, "g-properties-changed", G_CALLBACK(agh_mm_sm_properties_changed), mstate);
		g_print("%s: OK\n",__FUNCTION__);
	}

	return;
}

static void agh_mm_sm_properties_changed(MMModem *modem, GVariant *changed_props, GStrv inv_props, gpointer user_data) {
	struct agh_state *mstate = user_data;
	GString *content;
	struct command *event;
	gchar *modem_idx;

	modem_idx = agh_mm_modem_to_index(mm_modem_get_path(modem));
	content = g_string_new("\n");
	g_string_append_printf(content, "modem=%s\n",modem_idx);
	g_free(modem_idx);
	modem_idx = NULL;

	content = g_variant_print_string(changed_props, content, TRUE);
	event = cmd_event_prepare();
	cmd_answer_set_status(event, CMD_ANSWER_STATUS_OK);
	cmd_answer_addtext(event, "\""AGH_MM_SM_MODEM_PROPSCHANGES_EVENT_NAME"\"");
	cmd_answer_set_data(event, TRUE);
	cmd_answer_peektext(event, g_string_free(content, FALSE));
	cmd_emit_event(mstate->comm, event);
	content = NULL;

	return;
}

static void agh_mm_sm_call_outside_helper(struct agh_state *mstate, MMBearer *b) {
	gchar *ubus_call_bearers_info_message;
	gchar *ubus_message;
	gint status;
	gchar *callee_output;

	ubus_call_bearers_info_message = NULL;
	status = 0;
	ubus_message = NULL;
	callee_output = NULL;

	if (!mstate->uctx) {
		agh_mm_sm_report_error(mstate, "agh_mm_sm_call_outside_helper_no_ubus_context");
		return;
	}

	if (agh_ubus_connection_state != AGH_UBUS_STATE_CONNECTED) {
		agh_mm_sm_report_error(mstate, "agh_mm_sm_call_outside_helper_no_ubus_connection");
		return;
	}

	ubus_call_bearers_info_message = agh_mm_sm_call_outside_build_message(mstate, b);

	ubus_message = g_strdup_printf("{\"command\":\"/opt/bearer_setup_helper.sh\",\"env\":%s}", ubus_call_bearers_info_message);
	//g_print("%s: message: \n%s\n",__FUNCTION__,ubus_message);

	status = agh_ubus_call(mstate->uctx, "file", "exec", ubus_message);
	if (status)
		agh_mm_sm_report_error(mstate, "agh_mm_sm_call_outside_helper_call_error");
	else {
		callee_output = agh_ubus_get_call_result();

		if (callee_output) {
			agh_mm_sm_report(mstate, CMD_ANSWER_STATUS_OK, "agh_mm_sm_call_outside_helper", "agh_mm_sm_call_outside_helper_callee_output", "system", callee_output, TRUE);
			g_free(callee_output);
			callee_output = NULL;
		}
	}

	g_free(ubus_message);
	ubus_message = NULL;
	g_free(ubus_call_bearers_info_message);
	ubus_call_bearers_info_message = NULL;
	return;
}

static struct agh_family_table {
	char *name;
	int type;
} agh_family_table[] = {
	{"none", MM_BEARER_IP_FAMILY_NONE},
	{"IPV4", MM_BEARER_IP_FAMILY_IPV4},
	{"IPV6", MM_BEARER_IP_FAMILY_IPV6},
	{"IPV4V6", MM_BEARER_IP_FAMILY_IPV4V6},
	{"any", MM_BEARER_IP_FAMILY_ANY},
	{NULL,}
};

static gchar *agh_mm_sm_call_outside_build_message(struct agh_state *mstate, MMBearer *b) {
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
	struct agh_family_table *family;
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

	if (mm_bearer_get_connected(b))
		g_print("Yes, the bearer IS connected.\n");

	/* bearer path */
	str_tmp = agh_mm_sm_call_outside_build_message_add_element("BEARER_PATH", mm_bearer_get_path(b), FALSE);
	g_string_append(s, str_tmp);
	g_free(str_tmp);

	/* interface */
	str_tmp = agh_mm_sm_call_outside_build_message_add_element("BEARER_INTERFACE", mm_bearer_get_interface(b), FALSE);
	g_string_append(s, str_tmp);
	g_free(str_tmp);

	numeric_quantity_string_tmp = g_strdup_printf("%" G_GUINT16_FORMAT"",mm_bearer_get_ip_timeout(b));
	str_tmp = agh_mm_sm_call_outside_build_message_add_element("BEARER_IP_TIMEOUT", numeric_quantity_string_tmp, FALSE);
	g_string_append(s, str_tmp);
	g_free(numeric_quantity_string_tmp);
	g_free(str_tmp);

	props = mm_bearer_get_properties(b);
	type = mm_bearer_properties_get_ip_type(props);
	for (family = agh_family_table; family->name; family++)
		if (family->type == type)
			break;
	str_tmp = agh_mm_sm_call_outside_build_message_add_element("BEARER_IP_FAMILY", family->name, FALSE);
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
				str_tmp = agh_mm_sm_call_outside_build_message_add_element("BEARER_IP_METHOD", "unknown", FALSE);
				g_string_append(s, str_tmp);
				g_free(str_tmp);
				break;
			case MM_BEARER_IP_METHOD_PPP:
				str_tmp = agh_mm_sm_call_outside_build_message_add_element("BEARER_IP_METHOD", "ppp", FALSE);
				g_string_append(s, str_tmp);
				g_free(str_tmp);
				break;
			case MM_BEARER_IP_METHOD_STATIC:
				str_tmp = agh_mm_sm_call_outside_build_message_add_element("BEARER_IP_METHOD", "static", FALSE);
				g_string_append(s, str_tmp);
				g_free(str_tmp);
				break;
			case MM_BEARER_IP_METHOD_DHCP:
				str_tmp = agh_mm_sm_call_outside_build_message_add_element("BEARER_IP_METHOD", "DHCP", FALSE);
				g_string_append(s, str_tmp);
				g_free(str_tmp);
				break;
		}

		/* IP address */
		str_tmp = agh_mm_sm_call_outside_build_message_add_element("BEARER_IP_ADDRESS", mm_bearer_ip_config_get_address(ipv4_config), FALSE);
		g_string_append(s, str_tmp);
		g_free(str_tmp);

		/* prefix */
		numeric_quantity_string_tmp = g_strdup_printf("%" G_GUINT16_FORMAT"",mm_bearer_ip_config_get_prefix(ipv4_config));
		str_tmp = agh_mm_sm_call_outside_build_message_add_element("BEARER_IP_PREFIX", numeric_quantity_string_tmp, FALSE);
		g_string_append(s, str_tmp);
		g_free(numeric_quantity_string_tmp);
		g_free(str_tmp);

		/* playground */
		dns_tmp = mm_bearer_ip_config_get_dns(ipv4_config);
		if (dns_tmp) {
			while (*dns_tmp) {
				dns_str_tmp = g_strdup_printf("BEARER_IP_DNS_%" G_GUINT16_FORMAT"",dns_counter);
				str_tmp = agh_mm_sm_call_outside_build_message_add_element(dns_str_tmp, *dns_tmp, FALSE);
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

		str_tmp = agh_mm_sm_call_outside_build_message_add_element("BEARER_IP_GATEWAY", mm_bearer_ip_config_get_gateway(ipv4_config), FALSE);
		g_string_append(s, str_tmp);
		g_free(str_tmp);

		numeric_quantity_string_tmp = g_strdup_printf("%" G_GUINT16_FORMAT"",mm_bearer_ip_config_get_mtu(ipv4_config));
		str_tmp = agh_mm_sm_call_outside_build_message_add_element("BEARER_IP_MTU", numeric_quantity_string_tmp, FALSE);
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
				str_tmp = agh_mm_sm_call_outside_build_message_add_element("BEARER_IPV6_METHOD", "unknown", FALSE);
				g_string_append(s, str_tmp);
				g_free(str_tmp);
				break;
			case MM_BEARER_IP_METHOD_PPP:
				str_tmp = agh_mm_sm_call_outside_build_message_add_element("BEARER_IPV6_METHOD", "ppp", FALSE);
				g_string_append(s, str_tmp);
				g_free(str_tmp);
				break;
			case MM_BEARER_IP_METHOD_STATIC:
				str_tmp = agh_mm_sm_call_outside_build_message_add_element("BEARER_IPV6_METHOD", "static", FALSE);
				g_string_append(s, str_tmp);
				g_free(str_tmp);
				break;
			case MM_BEARER_IP_METHOD_DHCP:
				str_tmp = agh_mm_sm_call_outside_build_message_add_element("BEARER_IPV6_METHOD", "DHCP", FALSE);
				g_string_append(s, str_tmp);
				g_free(str_tmp);
				break;
		}

		/* IP address */
		str_tmp = agh_mm_sm_call_outside_build_message_add_element("BEARER_IPV6_ADDRESS", mm_bearer_ip_config_get_address(ipv6_config), FALSE);
		g_string_append(s, str_tmp);
		g_free(str_tmp);

		/* prefix */
		numeric_quantity_string_tmp = g_strdup_printf("%" G_GUINT16_FORMAT"",mm_bearer_ip_config_get_prefix(ipv6_config));
		str_tmp = agh_mm_sm_call_outside_build_message_add_element("BEARER_IPV6_PREFIX", numeric_quantity_string_tmp, FALSE);
		g_string_append(s, str_tmp);
		g_free(numeric_quantity_string_tmp);
		g_free(str_tmp);

		/* playground */
		dns_tmp = mm_bearer_ip_config_get_dns(ipv6_config);
		if (dns_tmp) {
			while (*dns_tmp) {
				dns_str_tmp = g_strdup_printf("BEARER_IP_DNSV6_%" G_GUINT16_FORMAT"",dns_counter);
				str_tmp = agh_mm_sm_call_outside_build_message_add_element(dns_str_tmp, *dns_tmp, FALSE);
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

		str_tmp = agh_mm_sm_call_outside_build_message_add_element("BEARER_IPV6_GATEWAY", mm_bearer_ip_config_get_gateway(ipv6_config), FALSE);
		g_string_append(s, str_tmp);
		g_free(str_tmp);

		numeric_quantity_string_tmp = g_strdup_printf("%" G_GUINT16_FORMAT"",mm_bearer_ip_config_get_mtu(ipv6_config));
		str_tmp = agh_mm_sm_call_outside_build_message_add_element("BEARER_IPV6_MTU", numeric_quantity_string_tmp, FALSE);
		g_string_append(s, str_tmp);
		g_free(numeric_quantity_string_tmp);
		g_free(str_tmp);

		g_object_unref(ipv6_config);
		ipv6_config = NULL;
	}

	g_string_append(s, "}");

	return g_string_free(s, FALSE);
}

static gchar *agh_mm_sm_call_outside_build_message_add_element(const gchar *name, const gchar *value, gboolean last) {
	gchar *res;

	res = NULL;

	if (!name)
		return res;

	if (!strlen(name))
		return res;

	if (!value || (!strlen(value)))
		value = "unknown";

	if (!last)
		res = g_strdup_printf("\"%s\":\"%s\",",name,value);
	else
		res = g_strdup_printf("\"%s\":\"%s\"",name,value);

	return res;
}

struct uci_section *agh_mm_sm_select_generic(struct agh_state *mstate, MMModem *modem, MMSim *sim) {
	struct uci_section *section;
	const gchar *operator_id;
	struct uci_element *e;
	struct uci_section *current_section;
	struct uci_option *opt;

	section = NULL;
	operator_id = NULL;
	e = NULL;
	current_section = NULL;
	opt = NULL;

	if (!sim)
		return section;

	operator_id = mm_sim_get_operator_identifier(sim);
	if (!operator_id) {
		g_print("%s: can not get operator id\n",__FUNCTION__);
		agh_mm_sm_report_failure_modem(mstate, modem, AGH_MM_SM_MODEM_INIITSTATE_FAILURE_NO_OPERATOR_ID);
		return section;
	}

	uci_foreach_element(&mstate->mmstate->package->sections, e) {
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
