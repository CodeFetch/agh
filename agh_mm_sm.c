#include "agh_mm_sm.h"
#include "agh_commands.h"
#include "agh_mm_handlers.h"

/* Function prototypes. */

static void agh_mm_sm_stateaction(struct agh_state *mstate, MMModem *modem, MMModemState oldstate, MMModemState newstate, MMModemStateChangeReason reason);

static void agh_mm_sm_unlock(struct agh_state *mstate, MMModem *modem);

static void agh_mm_sm_enable(struct agh_state *mstate, MMModem *modem);
static void agh_mm_sm_enable_finish(MMModem *modem, GAsyncResult *res, struct agh_state *mstate);
static void agh_mm_sm_bearers_init(struct agh_state *mstate, MMModem *modem);
static void agh_mm_sm_bearers_init_get_list(MMModem *modem, GAsyncResult *res, struct agh_state *mstate);
static void agh_mm_sm_bearers_delete(MMModem *modem, GAsyncResult *res, struct agh_state *mstate);
static void agh_mm_sm_bearers_delete_next(MMModem *modem, GAsyncResult *res, struct agh_mm_asyncstate *a);
static void agh_mm_sm_handle_cfg_bearers_list(struct agh_state *mstate, MMModem *modem, GList *blist);

static void agh_mm_sm_stateaction(struct agh_state *mstate, MMModem *modem, MMModemState oldstate, MMModemState newstate, MMModemStateChangeReason reason) {

	switch(newstate) {
		case MM_MODEM_STATE_FAILED:
			agh_mm_report_failed_reason(mstate, modem);
			break;
		case MM_MODEM_STATE_UNKNOWN:
		case MM_MODEM_STATE_INITIALIZING:
			break;
		case MM_MODEM_STATE_LOCKED:
			agh_mm_report_locked_reason(mstate, modem);
			agh_mm_sm_unlock(mstate, modem);
			break;
		case MM_MODEM_STATE_DISABLED:
			if (!mstate->exiting)
				agh_mm_sm_enable(mstate, modem);
			break;
		case MM_MODEM_STATE_DISABLING:
		case MM_MODEM_STATE_ENABLING:
		case MM_MODEM_STATE_ENABLED:
			break;
		case MM_MODEM_STATE_REGISTERED:
			agh_mm_sm_bearers_init(mstate, modem);
			agh_mm_sm_general_init(mstate, modem);
			break;
		case MM_MODEM_STATE_SEARCHING:
		case MM_MODEM_STATE_DISCONNECTING:
		case MM_MODEM_STATE_CONNECTING:
		case MM_MODEM_STATE_CONNECTED:
			break;
	}

	return;
}

static void agh_mm_sm_statechange(MMModem *modem, MMModemState oldstate, MMModemState newstate, MMModemStateChangeReason reason, gpointer user_data) {
	struct agh_state *mstate = user_data;
	struct agh_cmd *event;
	gchar *modem_idx;

	event = NULL;
	modem_idx = agh_mm_modem_to_index(mm_modem_get_path(modem));

	event = cmd_event_prepare();
	agh_cmd_answer_set_status(event, AGH_CMD_ANSWER_STATUS_OK);
	agh_cmd_answer_addtext(event, AGH_MM_MODEM_EVENT_NAME);
	agh_cmd_answer_addtext(event, modem_idx);
	agh_cmd_answer_addtext(event, AGH_MM_VALIDATE_UNKNOWN(mm_modem_state_get_string(oldstate)));
	agh_cmd_answer_addtext(event, AGH_MM_VALIDATE_UNKNOWN(mm_modem_state_get_string(newstate)));
	agh_cmd_answer_addtext(event, AGH_MM_VALIDATE_UNKNOWN(agh_mm_get_statechange_reason_string(reason)));
	agh_cmd_answer_addtext(event, AGH_MM_VALIDATE_UNKNOWN(mm_modem_state_failed_reason_get_string(mm_modem_get_state_failed_reason(modem))));
	cmd_emit_event(mstate->comm, event);
	g_free(modem_idx);
	modem_idx = NULL;

	agh_mm_sm_stateaction(mstate, modem, oldstate, newstate, reason);

	return;
}

void agh_mm_sm_start(struct agh_state *mstate, MMObject *m) {
	MMModem *modem;
	MMModemState current_state;
	MMModemState fake_oldstate;
	MMModemStateChangeReason fakereason;

	modem = mm_object_get_modem(m);
	fakereason = MM_MODEM_STATE_CHANGE_REASON_UNKNOWN;
	current_state = MM_MODEM_STATE_UNKNOWN;
	fake_oldstate = MM_MODEM_STATE_UNKNOWN;

	g_signal_connect(modem, "state-changed", G_CALLBACK(agh_mm_sm_statechange), mstate);

	current_state = mm_modem_get_state(modem);
	fake_oldstate = current_state;
	agh_mm_sm_stateaction(mstate, modem, fake_oldstate, current_state, fakereason);

	g_object_unref(modem);

	return;
}

void agh_mm_sm_device_added(MMManager *manager, MMObject  *modem, gpointer user_data) {
	struct agh_state *mstate = user_data;
	struct agh_mm_state *mmstate = mstate->mmstate;

	g_print("%s: modem added\n",__FUNCTION__);

	agh_mm_sm_start(mstate, modem);

	return;
}

void agh_mm_sm_device_removed(MMManager *manager, MMObject  *modem, gpointer user_data) {
	struct agh_state *mstate = user_data;
	struct agh_mm_state *mmstate = mstate->mmstate;

	g_print("%s: modem removed\n",__FUNCTION__);

	return;
}

static void agh_mm_sm_unlock(struct agh_state *mstate, MMModem *modem) {
	MMModemLock l;

	l = mm_modem_get_unlock_required(modem);

	switch(l) {
		case MM_MODEM_LOCK_UNKNOWN:
		case MM_MODEM_LOCK_NONE:
			g_warn_if_reached();
			break;
		case MM_MODEM_LOCK_SIM_PIN:
			agh_mm_sm_sim_unlock(mstate, modem, l);
			break;
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
			break;
	}

	return;
}

static void agh_mm_sm_enable(struct agh_state *mstate, MMModem *modem) {
	struct uci_section *section;
	gboolean enable;
	struct uci_option *opt;

	enable = TRUE;
	opt = NULL;

	section = agh_mm_sm_get_modem_section(mstate, modem);
	if (section) {
		opt = uci_lookup_option(mstate->mmstate->mctx, section, AGH_MM_SECTION_MODEM_OPTION_ENABLE);
		if (opt && opt->type == UCI_TYPE_STRING) {
			if (!g_strcmp0(opt->v.string, "0")) {
				enable = FALSE;
			}
		}
	}

	if (!enable)
		return;

	mm_modem_enable(modem, NULL, (GAsyncReadyCallback)agh_mm_sm_enable_finish, mstate);

	return;
}

static void agh_mm_sm_enable_finish(MMModem *modem, GAsyncResult *res, struct agh_state *mstate) {
	gboolean sres;

	sres = FALSE;

	sres = mm_modem_enable_finish(modem, res, &mstate->mmstate->gerror);

	if (!sres)
		agh_mm_sm_report_failure_modem(mstate, modem, AGH_MM_SM_MODEM_INIITSTATE_FAILURE);

	return;
}

static void agh_mm_sm_bearers_init(struct agh_state *mstate, MMModem *modem) {
	/* And so, in a sunny day, in a function with "_init" in it's name, we start doing what? Deleting bearers. What do you think? :) */

	mm_modem_list_bearers(modem, NULL, (GAsyncReadyCallback)agh_mm_sm_bearers_delete, mstate);

	return;
}

static void agh_mm_sm_bearers_init_get_list(MMModem *modem, GAsyncResult *res, struct agh_state *mstate) {
	struct uci_section *modem_section;
	struct uci_section *sim_section;
	MMSim *sim;
	GList *blist;
	struct uci_section *default_bearer;
	struct uci_section *autoselected_bearer;

	modem_section = NULL;
	sim_section = NULL;
	sim = NULL;
	blist = NULL;
	default_bearer = NULL;
	autoselected_bearer = NULL;

	sim = mm_modem_get_sim_finish(modem, res, &mstate->mmstate->gerror);
	if (!sim) {
		agh_mm_sm_report_failure_modem(mstate, modem, AGH_MM_SM_MODEM_INIITSTATE_FAILURE);
		return;
	}

	sim_section = agh_mm_sm_get_sim_section(mstate, modem, sim);

	modem_section = agh_mm_sm_get_modem_section(mstate, modem);

	blist = agh_mm_sm_get_referenced_sections(mstate, sim_section, AGH_MM_SECTION_SIMCARD_OPTION_BEARERSLIST);
	if (!blist)
		blist = agh_mm_sm_get_referenced_sections(mstate, modem_section, AGH_MM_SECTION_MODEM_OPTION_BEARERSLIST);

	if (blist) {
		agh_mm_sm_handle_cfg_bearers_list(mstate, modem, blist);
		g_object_unref(sim);
		return;
	}
	else {
		default_bearer = agh_mm_sm_get_default_bearer(mstate);
		if (default_bearer) {
			agh_mm_sm_build_bearer(mstate, modem, default_bearer);
			g_object_unref(sim);
			return;
		}
		else {
			g_print("%s: no connection settings found, trying generic profiles\n",__FUNCTION__);
			autoselected_bearer = agh_mm_sm_select_generic(mstate, modem, sim);
			if (autoselected_bearer) {
				agh_mm_sm_build_bearer(mstate, modem, autoselected_bearer);
				g_object_unref(sim);
				return;
			}
			else
				agh_mm_sm_report_failure_modem(mstate, modem, AGH_MM_SM_BEARER_CONNECT_FAILURE_NO_SETTINGS);
		}
	}

	g_object_unref(sim);
	return;
}

static void agh_mm_sm_bearers_delete(MMModem *modem, GAsyncResult *res, struct agh_state *mstate) {
	GList *blist;
	struct agh_mm_asyncstate *a;
	MMBearer *b;

	blist = NULL;
	a = NULL;
	b = NULL;

	blist = mm_modem_list_bearers_finish(modem, res, &mstate->mmstate->gerror);
	if (!blist) {
		agh_mm_sm_report_failure_modem(mstate, modem, "agh_mm_sm_bearers_delete");

		mm_modem_get_sim(modem, NULL, (GAsyncReadyCallback)agh_mm_sm_bearers_init_get_list, mstate);
		return;
	}

	a = g_malloc0(sizeof(struct agh_mm_asyncstate));

	a->blist = blist;
	a->mstate = mstate;

	a->blist = g_list_first(a->blist);

	b = MM_BEARER(a->blist->data);
	a->blist = g_list_remove(a->blist, a->blist->data);

	mm_modem_delete_bearer(modem, mm_bearer_get_path(b), NULL, (GAsyncReadyCallback)agh_mm_sm_bearers_delete_next, a);
	g_object_unref(b);

	return;
}

static void agh_mm_sm_bearers_delete_next(MMModem *modem, GAsyncResult *res, struct agh_mm_asyncstate *a) {
	gboolean success;
	MMBearer *b;

	success = FALSE;
	b = NULL;

	success = mm_modem_delete_bearer_finish(modem, res, &a->mstate->mmstate->gerror);
	if (!success) {
		agh_mm_sm_report_failure_modem(a->mstate, modem, "agh_mm_sm_bearers_delete_next");
		/* Try to go on even in the face of errors here. We'll see how this works. */
	}

	if (!a->blist) {
		mm_modem_get_sim(modem, NULL, (GAsyncReadyCallback)agh_mm_sm_bearers_init_get_list, a->mstate);
		g_free(a);
		return;
	}
	else {
		a->blist = g_list_first(a->blist);
		b = MM_BEARER(a->blist->data);
		a->blist = g_list_remove(a->blist, a->blist->data);

		mm_modem_delete_bearer(modem, mm_bearer_get_path(b), NULL, (GAsyncReadyCallback)agh_mm_sm_bearers_delete_next, a);
		g_object_unref(b);
	}

	return;
}

static void agh_mm_sm_handle_cfg_bearers_list(struct agh_state *mstate, MMModem *modem, GList *blist) {
	GList *l;
	struct uci_section *sec;

	l = blist;
	sec = NULL;

	for (l = blist; l; l = g_list_next(l)) {
		sec = l->data;
		agh_mm_sm_build_bearer(mstate, modem, sec);
	}

	g_list_free(blist);
	blist = NULL;

	return;
}
