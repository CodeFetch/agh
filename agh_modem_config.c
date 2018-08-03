#include <glib.h>
#include <uci.h>
#include "agh_modem_config.h"

#if 0
gint main(void) {
	struct agh_modem_config_validation_error *validation_error;

	validation_error = NULL;

	agh_modem_validate_config(AGH_MODEM_UCI_CONFIG_PACKAGE, &validation_error);

	if (validation_error) {
		g_print("Failure %" G_GINT16_FORMAT" (%s): %s\n",validation_error->error_code,validation_error->element_name ? validation_error->element_name : "**",validation_error->error_desc);
		agh_modem_config_validation_error_free(validation_error);
		validation_error = NULL;
	}

	return 0;
}
#endif

void agh_modem_validate_config(gchar *package_name, struct agh_modem_config_validation_error **validation_error) {
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

	uci_foreach_element( &ptr.p->sections, e) {
		current_section = uci_to_section(e);
		section_type = AGH_MM_SECTION_UNKNOWN;

		g_print("Section named %s, of type %s\n",current_section->e.name, current_section->type);

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

	/* Check for referenced, but missing, elements in the configuration file. */
	while ((!retval) && (refstate < 3)) {

		switch (refstate) {
			case 0:
				retval = agh_modem_validate_config_check_references(uci_ctx, ptr.p, referenced_sims, &reference_error_element_name);
				break;
			case 1:
				retval = agh_modem_validate_config_check_references(uci_ctx, ptr.p, referenced_modem_bearers, &reference_error_element_name);
				break;
			case 2:
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
			error_location_str = g_strdup_printf("references: %s",reference_error_element_name);
	}

	uci_unload(uci_ctx, ptr.p);
	uci_free_context(uci_ctx);
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

gint agh_modem_validate_config_modem_section(struct uci_section *s, GQueue **referenced_sims, GQueue **referenced_modem_bearers) {
	gboolean imei_found;
	gboolean equipment_id_found;
	struct uci_element *e;
	struct uci_option *opt;
	gint retval;
	GQueue *bearers_names;
	struct uci_element *f;
	gchar *default_bearer_name;
	guint bcounter;
	guint i;
	guint n_bearers;
	gchar *tmp_bearer_name;
	gboolean default_bearer_found;

	imei_found = FALSE;
	equipment_id_found = FALSE;
	e = NULL;
	opt = NULL;
	retval = 0;
	bearers_names = NULL;
	f = NULL;
	default_bearer_name = NULL;
	bcounter = 0;
	i = 0;
	n_bearers = 0;
	default_bearer_found = FALSE;
	tmp_bearer_name = NULL;

	if ((*referenced_sims) || (*referenced_modem_bearers)) {
		retval = AGH_MODEM_VALIDATE_CONFIG_PROGRAMMING_ERROR;
		goto out;
	}

	uci_foreach_element(&s->options, e) {
		opt = uci_to_option(e);

		if ((!g_strcmp0(e->name, AGH_MM_SECTION_MODEM_OPTION_IMEI)) && (opt->type == UCI_TYPE_STRING))
			imei_found = TRUE;

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

		if ((!g_strcmp0(e->name, AGH_MM_SECTION_MODEM_OPTION_DEFAULT_BEARER)) && (opt->type == UCI_TYPE_STRING))
			default_bearer_name = opt->v.string;

		if ((!g_strcmp0(e->name, AGH_MM_SECTION_MODEM_OPTION_SIMLIST_NAME)) && (opt->type == UCI_TYPE_LIST)) {
			*referenced_sims = g_queue_new();
			uci_foreach_element(&opt->v.list, f) {
				g_queue_push_tail(*referenced_sims, g_strdup(f->name));
			}
		}

	}

	if ((!imei_found) && (!equipment_id_found)) {
		retval = AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_IMEI_OR_EQUIPMENT_ID_NOT_SPECIFIED;
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

		if ((n_bearers > 1) && (!default_bearer_name)) {
			retval = AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_DEFAULT_BEARER_MISSING;
			goto out;
		}

		if (default_bearer_name)
			for (i=0;i<n_bearers;i++) {
				tmp_bearer_name = g_queue_peek_nth(bearers_names, i);
				if (!g_strcmp0(tmp_bearer_name, default_bearer_name)) {
					default_bearer_found = TRUE;
					break;
				}
			}

		if ((default_bearer_name) && (!default_bearer_found)) {
			retval = AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_NONEXISTENT_DEFAULT_BEARER;
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

gint agh_modem_validate_config_simcard_section(struct uci_section *s, GQueue **referenced_bearers) {
	gboolean iccid_found;
	GQueue *bearers_names;
	struct uci_element *e;
	struct uci_element *f;
	struct uci_option *opt;
	gint retval;
	gchar *default_bearer_name;
	guint bcounter;
	guint i;
	guint n_bearers;
	gchar *tmp_bearer_name;
	gboolean default_bearer_found;

	retval = 0;
	iccid_found = FALSE;
	e = NULL;
	f = NULL;
	opt = NULL;
	bearers_names = NULL;
	default_bearer_name = NULL;
	bcounter = 0;
	i = 0;
	n_bearers = 0;
	tmp_bearer_name = NULL;
	default_bearer_found = FALSE;

	if (*referenced_bearers) {
		retval = AGH_MODEM_VALIDATE_CONFIG_PROGRAMMING_ERROR;
		goto out;
	}

	uci_foreach_element(&s->options, e) {
		opt = uci_to_option(e);

		if ((!g_strcmp0(e->name, AGH_MM_SECTION_SIMCARD_OPTION_ICCID)) && (opt->type == UCI_TYPE_STRING))
			iccid_found = TRUE;

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

		if ((!g_strcmp0(e->name, AGH_MM_SECTION_SIMCARD_OPTION_DEFAULT_BEARER)) && (opt->type == UCI_TYPE_STRING))
			default_bearer_name = opt->v.string;
	}

	if (!iccid_found) {
		retval = AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_ICCID_NOT_SPECIFIED;
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

		if ((n_bearers > 1) && (!default_bearer_name)) {
			retval = AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_DEFAULT_BEARER_MISSING;
			goto out;
		}

		if (default_bearer_name)
			for (i=0;i<n_bearers;i++) {
				tmp_bearer_name = g_queue_peek_nth(bearers_names, i);
				if (!g_strcmp0(tmp_bearer_name, default_bearer_name)) {
					default_bearer_found = TRUE;
					break;
				}
			}

		if ((default_bearer_name) && (!default_bearer_found)) {
			retval = AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_NONEXISTENT_DEFAULT_BEARER;;
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

gint agh_modem_validate_config_bearer_section(struct uci_section *s) {
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

gchar *agh_modem_validate_config_strerror(gint retval) {
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
		case AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_IMEI_OR_EQUIPMENT_ID_NOT_SPECIFIED:
			error_desc = g_strdup(AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_IMEI_OR_EQUIPMENT_ID_NOT_SPECIFIED_DESC);
			break;
		case AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_MODEM_BEARERS_GQUEUE_PRESENT_BUT_EMPTY:
			error_desc = g_strdup(AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_MODEM_BEARERS_GQUEUE_PRESENT_BUT_EMPTY_DESC);
			break;
		case AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_DEFAULT_BEARER_MISSING:
			error_desc = g_strdup(AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_DEFAULT_BEARER_MISSING_DESC);
			break;
		case AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_NONEXISTENT_DEFAULT_BEARER:
			error_desc = g_strdup(AGH_MODEM_VALIDATE_CONFIG_MODEM_SECTION_ERROR_NONEXISTENT_DEFAULT_BEARER_DESC);
			break;
		case AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_ICCID_NOT_SPECIFIED:
			error_desc = g_strdup(AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_ICCID_NOT_SPECIFIED_DESC);
			break;
		case AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_BEARERS_GQUEUE_DEFINED_BUT_EMPTY:
			error_desc = g_strdup(AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_BEARERS_GQUEUE_DEFINED_BUT_EMPTY_DESC);
			break;
		case AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_DEFAULT_BEARER_MISSING:
			error_desc = g_strdup(AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_DEFAULT_BEARER_MISSING_DESC);
			break;
		case AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_NONEXISTENT_DEFAULT_BEARER:
			error_desc = g_strdup(AGH_MODEM_VALIDATE_CONFIG_SIMCARD_SECTION_ERROR_NONEXISTENT_DEFAULT_BEARER_DESC);
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

struct agh_modem_config_validation_error *agh_modem_config_validation_error_alloc(void) {
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

gint agh_modem_validate_config_check_references(struct uci_context *ctx, struct uci_package *p, GQueue *names, gchar **current_section) {
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
