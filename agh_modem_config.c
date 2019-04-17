#include <glib.h>
#include "agh.h"
#include "agh_modem.h"
#include "agh_modem_config.h"
#include "agh_logging.h"

/* Log messages from AGH_LOG_DOMAIN_MODEM_CONFIG domain. */
#define AGH_LOG_DOMAIN_MODEM_CONFIG "MM_CONFIG"

/* Logging macros. */
#define agh_log_mm_config_dbg(message, ...) agh_log_dbg(AGH_LOG_DOMAIN_MODEM_CONFIG, message, ##__VA_ARGS__)
#define agh_log_mm_config_crit(message, ...) agh_log_crit(AGH_LOG_DOMAIN_MODEM_CONFIG, message, ##__VA_ARGS__)

static gint agh_modem_validate_config_modem_section(struct uci_section *s, GQueue **referenced_sims, GQueue **referenced_modem_bearers, GError **error) {
	gboolean equipment_id_found;
	struct uci_element *e;
	struct uci_option *opt;
	gint error_code;
	GQueue *bearers_names;
	struct uci_element *f;
	guint bcounter;
	guint scounter;

	g_assert(error == NULL || *error == NULL);

	equipment_id_found = FALSE;
	error_code = 0;
	bearers_names = NULL;

	/* Maybe we can replace this with another g_assert? */
	if ((!referenced_sims || *referenced_sims) || (!referenced_modem_bearers || *referenced_modem_bearers)) {
		error_code = AGH_MODEM_VALIDATE_CONFIG_PROGRAMMING_ERROR;
		agh_log_mm_config_crit("invalid referenced_sims or referenced_modem_bearers");
		goto out;
	}

	/* configuration scanning block */
	uci_foreach_element(&s->options, e) {
		opt = uci_to_option(e);

		/* if we face a AGH_MM_SECTION_MODEM_OPTION_EQUIPMENT_ID setting of type string, maybe we found an Equipment Identifier. */
		if ((!g_strcmp0(e->name, AGH_MM_SECTION_MODEM_OPTION_EQUIPMENT_ID)) && (opt->type == UCI_TYPE_STRING))
			equipment_id_found = TRUE;

		/* If modem bearers definitions are found, then save them in a GQueue, until we reach allowed maximum. */
		if ((!g_strcmp0(e->name, AGH_MM_SECTION_MODEM_OPTION_BEARERSLIST)) && (opt->type == UCI_TYPE_LIST)) {
			bearers_names = g_queue_new();
			bcounter = 0;

			/* list "scanning" block */
			uci_foreach_element(&opt->v.list, f) {
				if (bcounter > AGH_MM_SECTION_MODEM_MAXBEARERS)
					break;

				g_queue_push_tail(bearers_names, g_strdup(f->name));
				bcounter++;
			}

		}

		/* do the same for SIM cards that are allowed to be handled with this modem */
		if ((!g_strcmp0(e->name, AGH_MM_SECTION_MODEM_OPTION_SIMLIST_NAME)) && (opt->type == UCI_TYPE_LIST)) {
			*referenced_sims = g_queue_new();
			scounter = 0;

			uci_foreach_element(&opt->v.list, f) {
				if (scounter > AGH_MM_SECTION_MODEM_MAXSIMS)
					break;

				g_queue_push_tail(*referenced_sims, g_strdup(f->name));
				scounter++;
			}
		}

	}

	if (!equipment_id_found) {
		error_code = AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_EQUIPMENT_ID_NOT_SPECIFIED;
		agh_log_mm_config_crit("mandatory Equipment Identifier not found");
		goto out;
	}

	if (bearers_names) {
		if (!bcounter) {
			error_code = AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_MODEM_BEARERS_GQUEUE_PRESENT_BUT_EMPTY;
			agh_log_mm_config_crit("empty modem bearers list");
			goto out;
		}

		if (bcounter > AGH_MM_SECTION_MODEM_MAXBEARERS) {
			error_code = AGH_MODEM_VALIDATE_CONFIG_MAXBEARERS_EXCEEDED;
			agh_log_mm_config_crit("maximum allowed number of modem bearers exceeded");
			goto out;
		}

	}

	/* repeat all togeter now */
	if (*referenced_sims) {
		if (!scounter) {
			error_code = AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_SIMLIST_GQUEUE_PRESENT_BUT_EMPTY;
			agh_log_mm_config_crit("SIM cards list present, but empty");
			goto out;
		}

		if (scounter > AGH_MM_SECTION_MODEM_MAXSIMS) {
			error_code = AGH_MODEM_VALIDATE_CONFIG_MAXSIMS_EXCEEDED;
			agh_log_mm_config_crit("maximum number of specifiable SIM card for a modem section has been exceeded");
			goto out;
		}

	}

out:
	if (error_code) {

		if (bearers_names) {
			g_queue_free_full(bearers_names, g_free);
			bearers_names = NULL;
		}

		if (*referenced_sims) {
			g_queue_free_full(*referenced_sims, g_free);
			*referenced_sims = NULL;
		}

		g_set_error(error, AGH_MM_SECTION_MODEM, error_code, "failure while validating modem section");

	}

	*referenced_modem_bearers = bearers_names;

	return error_code;
}

static gint agh_modem_validate_config_simcard_section(struct uci_section *s, GQueue **referenced_bearers, GError **error) {
	gboolean sim_id_found;
	GQueue *bearers_names;
	struct uci_element *e;
	struct uci_element *f;
	struct uci_option *opt;
	gint error_code;
	guint bcounter;

	g_assert(error == NULL || *error == NULL);

	error_code = 0;
	sim_id_found = FALSE;
	bearers_names = NULL;

	if (!referenced_bearers || *referenced_bearers) {
		error_code = AGH_MODEM_VALIDATE_CONFIG_PROGRAMMING_ERROR;
		agh_log_mm_config_crit("invalid referenced_bearers");
		goto out;
	}

	uci_foreach_element(&s->options, e) {
		opt = uci_to_option(e);

		if ((!g_strcmp0(e->name, AGH_MM_SECTION_SIMCARD_OPTION_SIM_ID)) && (opt->type == UCI_TYPE_STRING))
			sim_id_found = TRUE;

		if ((!g_strcmp0(e->name, AGH_MM_SECTION_SIMCARD_OPTION_BEARERSLIST)) && (opt->type == UCI_TYPE_LIST)) {
			bearers_names = g_queue_new();
			bcounter = 0;
			uci_foreach_element(&opt->v.list, f) {
				if (bcounter > AGH_MM_SECTION_SIMCARD_MAXBEARERS)
					break;

				g_queue_push_tail(bearers_names, g_strdup(f->name));
				bcounter++;
			}

		}

	}

	if (!sim_id_found) {
		error_code = AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_SIM_ID_NOT_SPECIFIED;
		agh_log_mm_config_crit("SIM ID not found");
		goto out;
	}

	if (bearers_names) {
		if (!bcounter) {
			error_code = AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_BEARERS_GQUEUE_DEFINED_BUT_EMPTY;
			agh_log_mm_config_crit("empty SIM bearer list");
			goto out;
		}

		if (bcounter > AGH_MM_SECTION_SIMCARD_MAXBEARERS) {
			error_code = AGH_MODEM_VALIDATE_CONFIG_MAXBEARERS_EXCEEDED;
			agh_log_mm_config_crit("maximum number of bearer specifiable on a SIM section has been exceeded");
			goto out;
		}

	}

out:
	if (error_code) {
		if (bearers_names) {
			g_queue_free_full(bearers_names, g_free);
			bearers_names = NULL;
		}

		g_set_error(error, AGH_MM_SECTION_SIMCARD, error_code, "failure while validating SIM card section");

	}

	*referenced_bearers = bearers_names;

	return error_code;
}

static gint agh_modem_validate_config_bearer_section(struct uci_section *s, GError **error) {
	gint error_code;
	struct uci_element *e;
	gboolean auth_method_found;
	gboolean APN_found;
	struct uci_option *opt;

	g_assert(error == NULL || *error == NULL);

	error_code = 0;
	auth_method_found = FALSE;
	APN_found = FALSE;

	uci_foreach_element(&s->options, e) {
		opt = uci_to_option(e);

		if ((!g_strcmp0(e->name, AGH_MM_SECTION_BEARER_OPTION_APN)) && (opt->type == UCI_TYPE_STRING))
			APN_found = TRUE;

		if ((!g_strcmp0(e->name, AGH_MM_SECTION_BEARER_OPTION_AUTH_METHOD)) && (opt->type == UCI_TYPE_STRING))
			auth_method_found = TRUE;

	}

	if (!APN_found || !auth_method_found) {
		error_code = AGH_MODEM_VALIDATE_CONFIG_BEARER_SECTION_ERROR_APN_OR_AUTH_METHOD_NOT_FOUND;
		agh_log_mm_config_crit("seems an APN or an authentication method have not been specified");
		g_set_error(error, AGH_MM_SECTION_BEARER, error_code, "failure while validating bearer section");
	}

	return error_code;
}

static gint agh_modem_validate_config_check_references(struct uci_context *ctx, struct uci_package *p, GQueue *names, gchar **current_section) {
	GQueue *visited_sections;
	guint i;
	guint num_names;
	gchar *current_name;
	struct uci_section *cs;
	gint retval;
	GList *hits;

	retval = 0;
	visited_sections = NULL;

	if (!p || !ctx || !current_section || !names) {
		agh_log_mm_config_crit("!p || !ctx || !current_section || !names");
		retval = AGH_MODEM_VALIDATE_CONFIG_PROGRAMMING_ERROR;
		goto out;
	}

	num_names = g_queue_get_length(names);

	if (!num_names) {
		agh_log_mm_config_crit("passed names GQueue is empty");
		retval = AGH_MODEM_VALIDATE_CONFIG_PROGRAMMING_ERROR;
		goto out;
	}

	visited_sections = g_queue_new();

	for (i=0;i<num_names;i++) {
		current_name = g_queue_peek_nth(names, i);

		if (!current_name) {
			agh_log_mm_config_crit("current_name was NULL!");
			break;
		}

		agh_log_mm_config_dbg("searching for %s",current_name);
		cs = uci_lookup_section(ctx, p, current_name);
		if (!cs) {
			retval = AGH_MODEM_VALIDATE_CONFIG_ERROR_REFERENCED_SECTION_NOT_FOUND;
			*current_section = current_name;
			agh_log_mm_config_crit("referenced section %s was not found",current_name);
			goto out;
		}

		hits = g_queue_find(visited_sections, cs);
		if (hits) {
			/* Do not free this list with g_list_free. */
			retval = AGH_MODEM_VALIDATE_CONFIG_UNIQUENESS_VIOLATION;
			*current_section = current_name;
			goto out;
		}

		g_queue_push_tail(visited_sections, cs);

	}

out:
	if (visited_sections)
		g_queue_free(visited_sections);

	return retval;
}

gint agh_modem_validate_config(struct agh_mm_state *mmstate, const gchar *path, gchar *package_name) {
	gint retval;
	struct uci_context *uci_ctx;
	struct uci_ptr ptr;
	gint section_type;
	struct uci_element *e;
	struct uci_section *current_section;
	struct uci_option *current_option;
	GQueue *referenced_sims;
	GQueue *referenced_sim_bearers;
	GQueue *referenced_modem_bearers;
	gchar *error_location_str;
	gchar *reference_error_element_name;
	gint refstate;

	retval = 0;
	referenced_sims = NULL;
	referenced_sim_bearers = NULL;
	referenced_modem_bearers = NULL;
	error_location_str = NULL;
	reference_error_element_name = NULL;
	refstate = 0;
	current_section = NULL;

	if (!mmstate || !package_name) {
		agh_log_mm_config_crit("passed AGH MM state was NULL, or NULL package name specified");
		retval = 100;
		goto out_noctx;
	}

	if (mmstate->mctx) {
		agh_log_mm_config_dbg("a config was already loaded, unloading");
		uci_unload(mmstate->mctx, mmstate->uci_package);
		uci_free_context(mmstate->mctx);
		mmstate->mctx = NULL;
		mmstate->uci_package = NULL;
	}

	uci_ctx = uci_alloc_context();
	if (!uci_ctx) {
		retval = AGH_MODEM_VALIDATE_CONFIG_ERROR_OOM;
		agh_log_mm_config_crit("UCI context allocation failure");
		goto out_noctx;
	}

	uci_ctx->flags |= UCI_FLAG_STRICT;

	if (path) {
		agh_log_mm_config_dbg("setting search path to %s",path);
		if ( (retval = uci_set_confdir(uci_ctx, path)) ) {
			agh_log_mm_config_crit("got failure from uci_set_confdir (code=%" G_GINT16_FORMAT")",retval);
			retval = AGH_MODEM_VALIDATE_CONFIG_SET_CONFDIR_FAILURE;
			goto out;
		}
	}

	/*
	 * Search for our package. We are not testing for UCI_LOOKUP_COMPLETE because no other parts are supposed to be specified in the tuple.
	*/
	if (uci_lookup_ptr(uci_ctx, &ptr, package_name, FALSE) != UCI_OK) {
		retval = AGH_MODEM_VALIDATE_CONFIG_ERROR_NOPKG;
		agh_log_mm_config_crit("no UCI package %s was found",package_name);
		goto out;
	}

	uci_foreach_element(&ptr.p->sections, e) {
		current_section = uci_to_section(e);

		if (!current_section->e.name || !strlen(current_section->e.name)) {
			agh_log_mm_config_crit("section with an empty name");
			retval = AGH_MODEM_VALIDATE_CONFIG_ERROR_INVALLIDSECTION;
			goto out;
		}
		section_type = AGH_MM_SECTION_UNKNOWN;

		if (!g_strcmp0(current_section->type, AGH_MM_SECTION_MODEM_NAME))
			section_type = AGH_MM_SECTION_MODEM;

		if (!g_strcmp0(current_section->type, AGH_MM_SECTION_SIMCARD_NAME))
			section_type = AGH_MM_SECTION_SIMCARD;

		if (!g_strcmp0(current_section->type, AGH_MM_SECTION_BEARER_NAME))
			section_type = AGH_MM_SECTION_BEARER;

		switch(section_type) {
			case AGH_MM_SECTION_MODEM:
				retval = agh_modem_validate_config_modem_section(current_section, &referenced_sims, &referenced_modem_bearers, &mmstate->current_gerror);

				break;
			case AGH_MM_SECTION_SIMCARD:
				retval = agh_modem_validate_config_simcard_section(current_section, &referenced_sim_bearers, &mmstate->current_gerror);

				break;
			case AGH_MM_SECTION_BEARER:
				retval = agh_modem_validate_config_bearer_section(current_section, &mmstate->current_gerror);

				break;
			default:
				agh_log_mm_config_crit("unknown section type found");
				retval = AGH_MODEM_VALIDATE_CONFIG_ERROR_INVALLIDSECTION;
				goto out;
		}

		if (retval)
			goto out;

	}

	/* now we check for references, so we do not work with a specific section - that's important when diagnosing errors */
	current_section = NULL;
	reference_error_element_name = NULL;

	/* Check for referenced, but missing, elements in config. */
	while ((!retval) && (refstate < 3)) {

		switch (refstate) {
			case 0:
				if (referenced_sims)
					retval = agh_modem_validate_config_check_references(uci_ctx, ptr.p, referenced_sims, &reference_error_element_name);

				break;
			case 1:
				if (referenced_modem_bearers)
					retval = agh_modem_validate_config_check_references(uci_ctx, ptr.p, referenced_modem_bearers, &reference_error_element_name);

				break;
			case 2:
				if (referenced_sim_bearers)
					retval = agh_modem_validate_config_check_references(uci_ctx, ptr.p, referenced_sim_bearers, &reference_error_element_name);

				break;
			default: {
				agh_log_mm_config_crit("unknown state while checking references");
				refstate = 4;
				break;
			}
		}

		if (!retval)
			refstate++;
	}

out:

	if (retval) {
		if (current_section)
			error_location_str = g_strdup_printf("type=%s, name=%s",current_section->type, current_section->e.name);
		else
			error_location_str = g_strdup_printf("references: %s",reference_error_element_name ? reference_error_element_name : "**");

		uci_unload(uci_ctx, ptr.p);
		uci_free_context(uci_ctx);
	}
	else {
		mmstate->mctx = uci_ctx;
		mmstate->uci_package = ptr.p;
	}

out_noctx:
	if (referenced_sims)
		g_queue_free_full(referenced_sims, g_free);

	if (referenced_modem_bearers)
		g_queue_free_full(referenced_modem_bearers, g_free);

	if (referenced_sim_bearers)
		g_queue_free_full(referenced_sim_bearers, g_free);

	/*
	 * NOTE: without { / } parentheses, GCC says it finds an else without a previous "if".
	 * This sounds to me like an alarm bell. What's going on here? Has this to do with the fact we invoke macros for logging?
	*/
	if (retval) {
		agh_log_mm_config_crit("failure %" G_GINT16_FORMAT" (%s)",retval,error_location_str ? error_location_str : "??");
	}
	else
		agh_log_mm_config_dbg("(%s) config load was successful",package_name);

	g_free(error_location_str);

	return retval;
}

static GList *agh_mm_config_build_simlist(struct agh_state *mstate, struct uci_section *section) {
	GList *l;
	struct uci_option *opt;
	struct uci_element *e;
	struct uci_section *sim_section;

	l = NULL;

	/* Validation guarantees that, if a SIM has been "referenced" in a modem section, then it should exist in the config. */
	if ((section) && (!g_strcmp0(section->type, AGH_MM_SECTION_MODEM_NAME))) {
		opt = uci_lookup_option(mstate->mmstate->mctx, section, AGH_MM_SECTION_MODEM_OPTION_SIMLIST_NAME);
		if ((opt) && (opt->type == UCI_TYPE_LIST)) {
			uci_foreach_element(&opt->v.list, e) {
				sim_section = uci_lookup_section(mstate->mmstate->mctx, mstate->mmstate->uci_package, e->name);

				g_assert(sim_section);
				l = g_list_append(l, sim_section);
			}
		}
		else {
			uci_foreach_element(&mstate->mmstate->uci_package->sections, e) {
				sim_section = uci_to_section(e);
				if (!g_strcmp0(sim_section->type, AGH_MM_SECTION_SIMCARD_NAME)) {
					l = g_list_append(l, sim_section);
				}
			}
		}
	}
	else {
		uci_foreach_element(&mstate->mmstate->uci_package->sections, e) {
			sim_section = uci_to_section(e);
			if (!g_strcmp0(sim_section->type, AGH_MM_SECTION_SIMCARD_NAME))
				l = g_list_append(l, sim_section);
		}
	}

	return l;
}

struct uci_section *agh_mm_config_get_sim_section(struct agh_state *mstate, MMModem *modem, MMSim *sim) {
	struct uci_section *modem_section;
	struct uci_section *sim_section;
	GList *simlist;
	GList *process_simlist;
	const gchar *sim_id;
	struct uci_option *opt;
	struct uci_section *res_section;

	res_section = NULL;

	if (!mstate || !mstate->mmstate || !mstate->mmstate->mctx || !modem || !sim) {
		agh_log_mm_config_crit("missing context");
		return res_section;
	}

	modem_section = agh_mm_config_get_modem_section(mstate, modem);
	if (!modem_section)
		agh_log_mm_config_crit("no section for this modem, or we where not able to retrieve Equipment ID");

	simlist = agh_mm_config_build_simlist(mstate, modem_section);

	if (!simlist) {
		agh_log_mm_config_crit("no SIM cards data found");
		return res_section;
	}

	sim_id = mm_sim_get_identifier(sim);
	if (!sim_id) {
		g_list_free(simlist);
		agh_log_mm_config_crit("can not get SIM ID");
		return res_section;
	}

	agh_log_mm_config_dbg("got SIM id %s",sim_id);

	for (process_simlist = simlist; process_simlist; process_simlist = g_list_next(process_simlist)) {
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

struct uci_section *agh_mm_config_get_modem_section(struct agh_state *mstate, MMModem *modem) {
	struct uci_section *section;
	const gchar *equipment_id;
	struct uci_element *e;
	struct uci_option *opt;

	section = NULL;

	if (!mstate || !mstate->mmstate || !mstate->mmstate->mctx || !modem) {
		agh_log_mm_config_crit("missing context");
		return section;
	}

	equipment_id = mm_modem_get_equipment_identifier(modem);
	if (!equipment_id) {
		agh_log_mm_config_crit("unable to get Equipment ID for this modem");
		return section;
	}

	uci_foreach_element(&mstate->mmstate->uci_package->sections, e) {
		section = uci_to_section(e);

		if (!g_strcmp0(section->type, AGH_MM_SECTION_MODEM_NAME)) {
			opt = uci_lookup_option(mstate->mmstate->mctx, section, AGH_MM_SECTION_MODEM_OPTION_EQUIPMENT_ID);

			/* validation should guarantee this option is present, and is a string */
			if (!g_strcmp0(opt->v.string, equipment_id))
				return section;

		}

	}

	section = NULL;

	return section;
}

gint agh_mm_config_get_boolean(struct uci_option *o) {
	gint retval;

	retval = -1;

	if (!o || (o->type != UCI_TYPE_STRING)) {
		agh_log_mm_config_dbg("invalid UCI option passed");
		goto out;
	}

	if (!g_strcmp0(o->v.string, "1"))
		retval = 1;
	if (!g_strcmp0(o->v.string, "yes"))
		retval = 1;
	if (!g_strcmp0(o->v.string, "on"))
		retval = 1;

	if (!g_strcmp0(o->v.string, "0"))
		retval = 0;
	if (!g_strcmp0(o->v.string, "no"))
		retval = 0;
	if (!g_strcmp0(o->v.string, "off"))
		retval = 0;

out:
	return retval;
}

GList *agh_mm_config_get_referenced_sections(struct agh_state *mstate, struct uci_section *section, gchar *section_name) {
	GList *slist;
	struct uci_section *current_section;
	struct uci_option *current_list_option;
	struct uci_element *e;

	slist = NULL;

	if ((!section) || (!section_name) || !mstate || !mstate->mmstate || !mstate->mmstate->mctx) {
		agh_log_mm_config_crit("can not search on a NULL section or with a NULL section name");
		return slist;
	}

	current_list_option = uci_lookup_option(mstate->mmstate->mctx, section, section_name);
	if ((current_list_option) && (current_list_option->type == UCI_TYPE_LIST)) {
		uci_foreach_element(&current_list_option->v.list, e) {
			current_section = uci_lookup_section(mstate->mmstate->mctx, mstate->mmstate->uci_package, e->name);
			g_assert(current_section);
			slist = g_list_append(slist, current_section);
		}
	}

	return slist;
}

//-----------------------------------------------

static gint agh_mm_config_build_bearer_set_iptype(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props) {
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

static gint agh_mm_config_build_bearer_set_apn(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props) {
	mm_bearer_properties_set_apn(props, o->v.string);
	return 0;
}

static gint agh_mm_config_build_bearer_set_auth_method(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props) {
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

static gint agh_mm_config_build_bearer_set_user(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props) {
	mm_bearer_properties_set_user(props, o->v.string);
	return 0;
}

static gint agh_mm_config_build_bearer_set_pass(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props) {
	mm_bearer_properties_set_password(props, o->v.string);
	return 0;
}

static gint agh_mm_config_build_bearer_set_roaming_allowed(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props) {
	gint status;

	status = agh_mm_config_get_boolean(o);
	if (status < 0)
		return 1;

	if (status)
		mm_bearer_properties_set_allow_roaming(props, TRUE);
	else
		mm_bearer_properties_set_allow_roaming(props, FALSE);

	return 0;
}

static gint agh_mm_config_build_bearer_set_number(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props) {
	mm_bearer_properties_set_password(props, o->v.string);
	return 0;
}

static gint agh_mm_config_build_bearer_set_rm_protocol(struct agh_state *mstate, struct uci_option *o, MMBearerProperties *props) {
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

gint agh_mm_config_build_bearer(struct agh_state *mstate, MMModem *modem, struct uci_section *s, GAsyncReadyCallback cb) {
	struct agh_mm_state *mmstate;
	struct uci_option *o;
	MMBearerProperties *props;
	gint status;

	status = 0;
	props = NULL;

	if (!mstate || !mstate->mmstate || !modem || !s) {
		agh_log_mm_config_crit("missing required data");
		status = 10;
		goto out;
	}

	mmstate = mstate->mmstate;

	props = mm_bearer_properties_new();
	if (!props) {
		agh_log_mm_config_crit("unable to obtain a MMBearerProperties object");
		status = 11;
		goto out;
	}

	/* IP family */
	o = uci_lookup_option(mmstate->mctx, s, AGH_MM_SECTION_BEARER_OPTION_IP_TYPE);
	if (o && (o->type == UCI_TYPE_STRING)) {
		status = agh_mm_config_build_bearer_set_iptype(mstate, o, props);
		if (status) {
			agh_log_mm_config_crit("failure setting IP type");
			goto out;
		}

	}

	/* APN */
	o = uci_lookup_option(mmstate->mctx, s, AGH_MM_SECTION_BEARER_OPTION_APN);
	if (o && (o->type == UCI_TYPE_STRING)) {
		status = agh_mm_config_build_bearer_set_apn(mstate, o, props);
		if (status) {
			agh_log_mm_config_crit("failure setting APN");
			goto out;
		}

	}

	/* auth type */
	o = uci_lookup_option(mmstate->mctx, s, AGH_MM_SECTION_BEARER_OPTION_AUTH_METHOD);
	if (o && (o->type == UCI_TYPE_STRING)) {
		status = agh_mm_config_build_bearer_set_auth_method(mstate, o, props);
		if (status) {
			agh_log_mm_config_crit("failure setting AUTH method");
			goto out;
		}

	}

	/* user */
	o = uci_lookup_option(mmstate->mctx, s, AGH_MM_SECTION_BEARER_OPTION_USERNAME);
	if (o && (o->type == UCI_TYPE_STRING)) {
		status = agh_mm_config_build_bearer_set_user(mstate, o, props);
		if (status) {
			agh_log_mm_config_crit("failure setting username");
			goto out;
		}

	}

	/* password */
	o = uci_lookup_option(mmstate->mctx, s, AGH_MM_SECTION_BEARER_OPTION_PASSWORD);
	if (o && (o->type == UCI_TYPE_STRING)) {
		status = agh_mm_config_build_bearer_set_pass(mstate, o, props);
		if (status) {
			agh_log_mm_config_crit("failure setting password");
			goto out;
		}

	}

	/* allow roaming? */
	o = uci_lookup_option(mmstate->mctx, s, AGH_MM_SECTION_BEARER_OPTION_ALLOW_ROAMING);
	if (o && (o->type == UCI_TYPE_STRING)) {
		status = agh_mm_config_build_bearer_set_roaming_allowed(mstate, o, props);
		if (status) {
			agh_log_mm_config_crit("failure setting roaming on/off");
			goto out;
		}

	}

	/* number */
	o = uci_lookup_option(mmstate->mctx, s, AGH_MM_SECTION_BEARER_OPTION_NUMBER);
	if (o && (o->type == UCI_TYPE_STRING)) {
		status = agh_mm_config_build_bearer_set_number(mstate, o, props);
		if (status) {
			agh_log_mm_config_crit("failure setting number");
			goto out;
		}

	}

	/* rm protocol */
	o = uci_lookup_option(mmstate->mctx, s, AGH_MM_SECTION_BEARER_OPTION_RM_PROTOCOL);
	if (o && (o->type == UCI_TYPE_STRING)) {
		status = agh_mm_config_build_bearer_set_rm_protocol(mstate, o, props);
		if (status) {
			agh_log_mm_config_crit("failure setting RM protocol");
			goto out;
		}

	}

	mm_modem_create_bearer(modem, props, NULL, (GAsyncReadyCallback)cb, mstate);

out:
	if (props)
		g_object_unref(props);
	return status;
}

struct uci_section *agh_mm_get_default_bearer(struct agh_state *mstate) {
	struct uci_section *default_bearer_section;
	struct uci_element *e;
	struct uci_section *current_section;

	default_bearer_section = NULL;

	if (!mstate || !mstate->mmstate || !mstate->mmstate->mctx) {
		agh_log_mm_config_crit("missing context");
		return default_bearer_section;
	}

	uci_foreach_element(&mstate->mmstate->uci_package->sections, e) {
		current_section = uci_to_section(e);

		if (!g_strcmp0(current_section->e.name, "default"))
			if (!g_strcmp0(current_section->type, AGH_MM_SECTION_BEARER_NAME)) {
				default_bearer_section = current_section;
				break;
			}
	}

	return default_bearer_section;
}
