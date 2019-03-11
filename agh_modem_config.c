#include <glib.h>
#include "agh.h"
#include "agh_modem.h"
#include "agh_modem_config.h"

/* config section types */
#define AGH_MM_SECTION_UNKNOWN 0
#define AGH_MM_SECTION_MODEM_NAME "modem"
#define AGH_MM_SECTION_MODEM 1
#define AGH_MM_SECTION_SIMCARD_NAME "simcard"
#define AGH_MM_SECTION_SIMCARD 2
#define AGH_MM_SECTION_BEARER_NAME "bearer"
#define AGH_MM_SECTION_BEARER 3
/* end of config section types */

/* Maximum number of bearers accepted in a AGH_MM_SECTION_SIMCARD section */
#define AGH_MM_SECTION_SIMCARD_MAXBEARERS 2

/* or in a modem section */
#define AGH_MM_SECTION_MODEM_MAXBEARERS 2

/* config options for AGH_MM_SECTION_MODEM. */
#define AGH_MM_SECTION_MODEM_OPTION_EQUIPMENT_ID "Equipment_Identifier"
#define AGH_MM_SECTION_MODEM_OPTION_ENABLE "enable"
#define AGH_MM_SECTION_MODEM_OPTION_SIMLIST_NAME "expected_simcards"
#define AGH_MM_SECTION_MODEM_OPTION_BEARERSLIST "modem_bearers"
#define AGH_MM_SECTION_MODEM_OPTION_REPORT_PROPSCHANGES "report_changes"
/* End of config options for AGH_MM_SECTION_MODEM. */

/* Config options for AGH_MM_SECTION_SIMCARD. */
#define AGH_MM_SECTION_SIMCARD_OPTION_SIM_ID "id"
#define AGH_MM_SECTION_SIMCARD_OPTION_BEARERSLIST "sim_bearers"
#define AGH_MM_SECTION_SIMCARD_OPTION_PIN_CODE "PIN_code"
/* End of config options for AGH_MM_SECTION_SIMCARD. */

/* Config options for AGH_MM_SECTION_BEARER. */
#define AGH_MM_SECTION_BEARER_OPTION_USERNAME "user"
#define AGH_MM_SECTION_BEARER_OPTION_PASSWORD "password"
#define AGH_MM_SECTION_BEARER_OPTION_APN "apn"
#define AGH_MM_SECTION_BEARER_OPTION_IP_TYPE "ip_type"
#define AGH_MM_SECTION_BEARER_OPTION_AUTH_METHOD "allowed_auth"
#define AGH_MM_SECTION_BEARER_OPTION_NUMBER "number"
#define AGH_MM_SECTION_BEARER_OPTION_ALLOW_ROAMING "allow_roaming"
#define AGH_MM_SECTION_BEARER_OPTION_RM_PROTOCOL "rm_protocol"
#define AGH_MM_SECTION_BEARER_OPTION_OPERATOR_ID "operator_id"
/* End of config option for AGH_MM_SECTION_BEARER. */

/* Config validation errors. */

/* General failures / issues */
#define AGH_MODEM_VALIDATE_CONFIG_ERROR_OOM 1
#define AGH_MODEM_VALIDATE_CONFIG_ERROR_OOM_DESC "Out of memory while allocating UCI context"
#define AGH_MODEM_VALIDATE_CONFIG_ERROR_NOPKG 2
#define AGH_MODEM_VALIDATE_CONFIG_ERROR_NOPKG_DESC "Can not find config package"
#define AGH_MODEM_VALIDATE_CONFIG_ERROR_INVALLIDSECTION 3
#define AGH_MODEM_VALIDATE_CONFIG_ERROR_INVALLIDSECTION_DESC "Invalid section type"
#define AGH_MODEM_VALIDATE_CONFIG_PROGRAMMING_ERROR 23
#define AGH_MODEM_VALIDATE_CONFIG_PROGRAMMING_ERROR_DESC "Programming error"
#define AGH_MODEM_VALIDATE_CONFIG_ERROR_REFERENCED_SECTION_NOT_FOUND 24
#define AGH_MODEM_VALIDATE_CONFIG_ERROR_REFERENCED_SECTION_NOT_FOUND_DESC "Referenced element was not found in config"
#define AGH_MODEM_VALIDATE_CONFIG_UNIQUENESS_VIOLATION 25
#define AGH_MODEM_VALIDATE_CONFIG_UNIQUENESS_VIOLATION_DESC "This element has been referenced more than once"
#define AGH_MODEM_VALIDATE_CONFIG_MAXBEARERS_EXCEEDED 26
#define AGH_MODEM_VALIDATE_CONFIG_MAXBEARERS_EXCEEDED_DESC "Maximum number of bearers exceeded"

/* AGH_MM_SECTION_MODEM section related issues */
#define AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_EQUIPMENT_ID_NOT_SPECIFIED 10
#define AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_EQUIPMENT_ID_NOT_SPECIFIED_DESC "Equipment ID not specified, modem can not be identified"
#define AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_MODEM_BEARERS_GQUEUE_PRESENT_BUT_EMPTY 20
#define AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_MODEM_BEARERS_GQUEUE_PRESENT_BUT_EMPTY_DESC "modem bearers GQueue was created, but is empty; this should not happen, but it did"

/* AGH_MM_SECTION_SIMCARD issues */
#define AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_SIM_ID_NOT_SPECIFIED 11
#define AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_SIM_ID_NOT_SPECIFIED_DESC "SIM ID not found"
#define AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_BEARERS_GQUEUE_DEFINED_BUT_EMPTY 12
#define AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_BEARERS_GQUEUE_DEFINED_BUT_EMPTY_DESC "SIM bearers GQueue was created, but is empty; this should not happen, but it did"

/* AGH_MM_SECTION_BEARER issues */
#define AGH_MODEM_VALIDATE_CONFIG_BEARER_SECTION_ERROR_APN_OR_AUTHMETHOD_NOT_FOUND 15
#define AGH_MODEM_VALIDATE_CONFIG_BEARER_SECTION_ERROR_APN_OR_AUTHMETHOD_NOT_FOUND_DESC "APN or authentication method not specified for this bearer"
/* End of config validation errors. */

static gint agh_modem_validate_config_modem_section(struct uci_section *s, GQueue **referenced_sims, GQueue **referenced_modem_bearers) {
	gboolean equipment_id_found;
	struct uci_element *e;
	struct uci_option *opt;
	gint retval;
	GQueue *bearers_names;
	struct uci_element *f;
	guint bcounter;
	guint n_bearers;

	equipment_id_found = FALSE;
	e = NULL;
	opt = NULL;
	retval = 0;
	bearers_names = NULL;
	f = NULL;
	bcounter = 0;
	n_bearers = 0;

	if ((*referenced_sims) || (*referenced_modem_bearers)) {
		retval = AGH_MODEM_VALIDATE_CONFIG_PROGRAMMING_ERROR;
		goto out;
	}

	uci_foreach_element(&s->options, e) {
		opt = uci_to_option(e);

		if ((!g_strcmp0(e->name, AGH_MM_SECTION_MODEM_OPTION_EQUIPMENT_ID)) && (opt->type == UCI_TYPE_STRING))
			equipment_id_found = TRUE;

		if ((!g_strcmp0(e->name, AGH_MM_SECTION_MODEM_OPTION_BEARERSLIST)) && (opt->type == UCI_TYPE_LIST)) {
			bearers_names = g_queue_new();
			bcounter = 0;
			uci_foreach_element(&opt->v.list, f) {
				if (bcounter > AGH_MM_SECTION_MODEM_MAXBEARERS)
					break;

				g_queue_push_tail(bearers_names, g_strdup(f->name));
				bcounter++;
			}

		}

		if ((!g_strcmp0(e->name, AGH_MM_SECTION_MODEM_OPTION_SIMLIST_NAME)) && (opt->type == UCI_TYPE_LIST)) {
			*referenced_sims = g_queue_new();
			uci_foreach_element(&opt->v.list, f) {
				g_queue_push_tail(*referenced_sims, g_strdup(f->name));
			}
		}

	}

	if (!equipment_id_found) {
		retval = AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_EQUIPMENT_ID_NOT_SPECIFIED;
		goto out;
	}

	if (bearers_names) {
		/* This should not happen, even here. */
		n_bearers = g_queue_get_length(bearers_names);
		if (!n_bearers) {
			retval = AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_MODEM_BEARERS_GQUEUE_PRESENT_BUT_EMPTY;
			goto out;
		}

		if (bcounter > AGH_MM_SECTION_MODEM_MAXBEARERS) {
			retval = AGH_MODEM_VALIDATE_CONFIG_MAXBEARERS_EXCEEDED;
			goto out;
		}

	}

out:
	if (retval) {

		if (bearers_names) {
			g_queue_free_full(bearers_names, g_free);
			bearers_names = NULL;
		}

		if (*referenced_sims) {
			g_queue_free_full(*referenced_sims, g_free);
			*referenced_sims = NULL;
		}

	}

	*referenced_modem_bearers = bearers_names;

	return retval;
}

static gint agh_modem_validate_config_simcard_section(struct uci_section *s, GQueue **referenced_bearers) {
	gboolean sim_id_found;
	GQueue *bearers_names;
	struct uci_element *e;
	struct uci_element *f;
	struct uci_option *opt;
	gint retval;
	guint bcounter;
	guint n_bearers;

	retval = 0;
	sim_id_found = FALSE;
	e = NULL;
	f = NULL;
	opt = NULL;
	bearers_names = NULL;
	bcounter = 0;
	n_bearers = 0;

	if (*referenced_bearers) {
		retval = AGH_MODEM_VALIDATE_CONFIG_PROGRAMMING_ERROR;
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
		retval = AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_SIM_ID_NOT_SPECIFIED;
		goto out;
	}

	if (bearers_names) {
		/* This should not happen. */
		n_bearers = g_queue_get_length(bearers_names);
		if (!n_bearers) {
			retval = AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_BEARERS_GQUEUE_DEFINED_BUT_EMPTY;
			goto out;
		}

		if (bcounter > AGH_MM_SECTION_SIMCARD_MAXBEARERS) {
			retval = AGH_MODEM_VALIDATE_CONFIG_MAXBEARERS_EXCEEDED;
			goto out;
		}

	}

out:
	if (retval) {
		if (bearers_names) {
			g_queue_free_full(bearers_names, g_free);
			bearers_names = NULL;
		}
	}

	*referenced_bearers = bearers_names;

	return retval;
}

static gint agh_modem_validate_config_bearer_section(struct uci_section *s) {
	gint retval;
	struct uci_element *e;
	gboolean auth_method_found;
	gboolean APN_found;
	struct uci_option *opt;

	retval = 0;
	e = NULL;
	auth_method_found = FALSE;
	APN_found = FALSE;
	opt = NULL;

	uci_foreach_element(&s->options, e) {
		opt = uci_to_option(e);

		if ((!g_strcmp0(e->name, AGH_MM_SECTION_BEARER_OPTION_APN)) && (opt->type == UCI_TYPE_STRING))
			APN_found = TRUE;

		if ((!g_strcmp0(e->name, AGH_MM_SECTION_BEARER_OPTION_AUTH_METHOD)) && (opt->type == UCI_TYPE_STRING))
			auth_method_found = TRUE;

	}

	if (!APN_found || !auth_method_found)
		retval = AGH_MODEM_VALIDATE_CONFIG_BEARER_SECTION_ERROR_APN_OR_AUTHMETHOD_NOT_FOUND;

	return retval;
}

static gchar *agh_modem_validate_config_strerror(gint retval) {
	gchar *error_desc;

	error_desc = NULL;

	switch(retval) {
		case AGH_MODEM_VALIDATE_CONFIG_ERROR_OOM:
			error_desc = g_strdup(AGH_MODEM_VALIDATE_CONFIG_ERROR_OOM_DESC);
			break;
		case AGH_MODEM_VALIDATE_CONFIG_ERROR_NOPKG:
			error_desc = g_strdup(AGH_MODEM_VALIDATE_CONFIG_ERROR_NOPKG_DESC);
			break;
		case AGH_MODEM_VALIDATE_CONFIG_ERROR_INVALLIDSECTION:
			error_desc = g_strdup(AGH_MODEM_VALIDATE_CONFIG_ERROR_INVALLIDSECTION_DESC);
			break;
		case AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_EQUIPMENT_ID_NOT_SPECIFIED:
			error_desc = g_strdup(AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_EQUIPMENT_ID_NOT_SPECIFIED_DESC);
			break;
		case AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_MODEM_BEARERS_GQUEUE_PRESENT_BUT_EMPTY:
			error_desc = g_strdup(AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_MODEM_BEARERS_GQUEUE_PRESENT_BUT_EMPTY_DESC);
			break;
		case AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_SIM_ID_NOT_SPECIFIED:
			error_desc = g_strdup(AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_SIM_ID_NOT_SPECIFIED_DESC);
			break;
		case AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_BEARERS_GQUEUE_DEFINED_BUT_EMPTY:
			error_desc = g_strdup(AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_BEARERS_GQUEUE_DEFINED_BUT_EMPTY_DESC);
			break;
		case AGH_MODEM_VALIDATE_CONFIG_BEARER_SECTION_ERROR_APN_OR_AUTHMETHOD_NOT_FOUND:
			error_desc = g_strdup(AGH_MODEM_VALIDATE_CONFIG_BEARER_SECTION_ERROR_APN_OR_AUTHMETHOD_NOT_FOUND_DESC);
			break;
		case AGH_MODEM_VALIDATE_CONFIG_PROGRAMMING_ERROR:
			error_desc = g_strdup(AGH_MODEM_VALIDATE_CONFIG_PROGRAMMING_ERROR_DESC);
			break;
		case AGH_MODEM_VALIDATE_CONFIG_ERROR_REFERENCED_SECTION_NOT_FOUND:
			error_desc = g_strdup(AGH_MODEM_VALIDATE_CONFIG_ERROR_REFERENCED_SECTION_NOT_FOUND_DESC);
			break;
		case AGH_MODEM_VALIDATE_CONFIG_UNIQUENESS_VIOLATION:
			error_desc = g_strdup(AGH_MODEM_VALIDATE_CONFIG_UNIQUENESS_VIOLATION_DESC);
			break;
		case AGH_MODEM_VALIDATE_CONFIG_MAXBEARERS_EXCEEDED:
			error_desc = g_strdup(AGH_MODEM_VALIDATE_CONFIG_MAXBEARERS_EXCEEDED_DESC);
			break;
		default:
			error_desc = g_strdup("Unknown error");
	}

	return error_desc;
}

static struct agh_modem_config_validation_error *agh_modem_config_validation_error_alloc(void) {
	struct agh_modem_config_validation_error *e;

	e = NULL;

	e = g_malloc0(sizeof(struct agh_modem_config_validation_error));

	return e;
}

void agh_modem_config_validation_error_free(struct agh_modem_config_validation_error *e) {

	if (!e)
		return;

	if (e->element_name) {
		g_free(e->element_name);
		e->element_name = NULL;
	}

	e->error_code = 0;

	if (e->error_desc) {
		g_free(e->error_desc);
		e->error_desc = NULL;
	}

	g_free(e);
}

static gint agh_modem_validate_config_check_references(struct uci_context *ctx, struct uci_package *p, GQueue *names, gchar **current_section) {
	GQueue *visited_sections;
	guint i;
	guint num_names;
	gchar *current_name;
	struct uci_section *cs;
	gint retval;
	GList *hits;

	visited_sections = NULL;
	i = 0;
	num_names = 0;
	current_name = NULL;
	cs = NULL;
	retval = 0;
	hits = NULL;

	if ((!p) || (!ctx))
		return AGH_MODEM_VALIDATE_CONFIG_PROGRAMMING_ERROR;

	if (!names)
		return AGH_MODEM_VALIDATE_CONFIG_PROGRAMMING_ERROR;

	num_names = g_queue_get_length(names);

	if (!num_names)
		return AGH_MODEM_VALIDATE_CONFIG_PROGRAMMING_ERROR;

	visited_sections = g_queue_new();

	for (i=0;i<num_names;i++) {
		current_name = g_queue_peek_nth(names, i);
		g_print("Searching for %s\n",current_name);
		cs = uci_lookup_section(ctx, p, current_name);
		if (!cs) {
			retval = AGH_MODEM_VALIDATE_CONFIG_ERROR_REFERENCED_SECTION_NOT_FOUND;
			*current_section = current_name;
			goto out;
		}

		hits = g_queue_find(visited_sections, cs);
		if (hits) {
			/* Do not free this list with g_list_free. */
			hits = NULL;
			retval = AGH_MODEM_VALIDATE_CONFIG_UNIQUENESS_VIOLATION;
			*current_section = current_name;
			goto out;
		}

		g_queue_push_tail(visited_sections, cs);

	}

	g_print("OK\n");

out:
	if (visited_sections) {
		g_queue_free(visited_sections);
		visited_sections = NULL;
	}
	return retval;
}

void agh_modem_validate_config(struct agh_state *mstate, gchar *package_name, struct agh_modem_config_validation_error **validation_error) {
	struct agh_mm_state *mmstate = mstate->mmstate;
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

	uci_ctx = NULL;
	section_type = AGH_MM_SECTION_UNKNOWN;
	e = NULL;
	current_section = NULL;
	retval = 0;
	referenced_sims = NULL;
	referenced_sim_bearers = NULL;
	referenced_modem_bearers = NULL;
	error_location_str = NULL;
	reference_error_element_name = NULL;
	refstate = 0;

	if (*validation_error)
		return;

	uci_ctx = uci_alloc_context();
	if (!uci_ctx) {
		retval = AGH_MODEM_VALIDATE_CONFIG_ERROR_OOM;
		goto out_noctx;
	}
	uci_ctx->flags |= UCI_FLAG_STRICT;

	/*
	 * Search for our package. We are not testing for UCI_LOOKUP_COMPLETE because no other parts are supposed to be specified in the tuple.
	*/
	if (uci_lookup_ptr(uci_ctx, &ptr, package_name, FALSE) != UCI_OK) {
		retval = AGH_MODEM_VALIDATE_CONFIG_ERROR_NOPKG;
		goto out;
	}

	uci_foreach_element(&ptr.p->sections, e) {
		current_section = uci_to_section(e);
		section_type = AGH_MM_SECTION_UNKNOWN;

		if (!g_strcmp0(current_section->type, AGH_MM_SECTION_MODEM_NAME))
			section_type = AGH_MM_SECTION_MODEM;

		if (!g_strcmp0(current_section->type, AGH_MM_SECTION_SIMCARD_NAME))
			section_type = AGH_MM_SECTION_SIMCARD;

		if (!g_strcmp0(current_section->type, AGH_MM_SECTION_BEARER_NAME))
			section_type = AGH_MM_SECTION_BEARER;

		switch(section_type) {
			case AGH_MM_SECTION_MODEM:
				retval = agh_modem_validate_config_modem_section(current_section, &referenced_sims, &referenced_modem_bearers);

				break;
			case AGH_MM_SECTION_SIMCARD:
				retval = agh_modem_validate_config_simcard_section(current_section, &referenced_sim_bearers);

				break;
			case AGH_MM_SECTION_BEARER:
				retval = agh_modem_validate_config_bearer_section(current_section);

				break;
			default:
				g_print("Invalid section\n");
				retval = AGH_MODEM_VALIDATE_CONFIG_ERROR_INVALLIDSECTION;
				goto out;
		}

		if (retval)
			goto out;

	}

	current_section = NULL;

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
			default:
				g_print("%s: unknown state while checking references\n",__FUNCTION__);
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
	if (referenced_sims) {
		g_queue_free_full(referenced_sims, g_free);
		referenced_sims = NULL;
	}

	if (referenced_modem_bearers) {
		g_queue_free_full(referenced_modem_bearers, g_free);
		referenced_modem_bearers = NULL;
	}

	if (referenced_sim_bearers) {
		g_queue_free_full(referenced_sim_bearers, g_free);
		referenced_sim_bearers = NULL;
	}

	uci_ctx = NULL;

	if (retval) {
		*validation_error = agh_modem_config_validation_error_alloc();
		(*validation_error)->error_code = retval;
		(*validation_error)->error_desc = agh_modem_validate_config_strerror(retval);
		if (error_location_str)
			(*validation_error)->element_name = error_location_str;

	}

	return;
}
