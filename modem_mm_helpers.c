/*
 * This file contains some helpers functions to build strings out of various MM data structures.
 * Much of the code in this file comes from libmm-glib/mm-common-helpers.c; it may be present here in a modified form, or in a completely different one.
 * Or it may not come from that file. So any bugs in this code should be reported to me, not the original author.
 * Function names have been kept in some cases, but the "agh_" prefix was added to avoid naming collisions.
 *
 * The code in here may be subject to GPL, Version 2 or, at your option, any later version.
*/

#include <libmm-glib.h>
#include "agh.h"
#include "modem.h"
#include "modem_mm_helpers.h"

gchar *agh_mm_common_build_capabilities_string(const MMModemCapability *capabilities, guint n_capabilities) {
	GString *str;
	guint i;
	gchar *tmp;

	str = NULL;
	tmp = NULL;

	if (!capabilities || !n_capabilities)
		return g_strdup("none");

	str = g_string_new(NULL);

	for (i = 0; i < n_capabilities; i++) {
		tmp = VALIDATE_UNKNOWN(mm_modem_capability_build_string_from_mask(capabilities[i]));
		g_string_append_printf(str, "%s%s", i ? " " : "", tmp);
		g_free(tmp);
		tmp = NULL;
	}

	return g_string_free(str, FALSE);
}

gchar *agh_mm_common_build_ports_string(const MMModemPortInfo *ports, guint n_ports) {
	GString *str;
	guint i;
	const gchar *tmp;

	str = NULL;
	tmp = NULL;

	if (!ports || !n_ports)
		return g_strdup("none");

	str = g_string_new(NULL);

	for (i = 0; i < n_ports; i++) {
		tmp = VALIDATE_UNKNOWN(mm_modem_port_type_get_string(ports[i].type));
		g_string_append_printf(str, "%s%s (%s)", i ? " " : "", ports[i].name, tmp);
		tmp = NULL;
	}

	return g_string_free(str, FALSE);
}

gchar *agh_mm_unlock_retries_build_string(MMUnlockRetries *object) {
	GString *str;
	GHashTableIter iter;
	gpointer key;
	gpointer value;
	const gchar *lock_name;
	guint retries;

	str = NULL;
	key = NULL;
	value = NULL;
	lock_name = NULL;
	retries = 0;

	g_hash_table_iter_init(&iter, object->priv->ht);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		lock_name = mm_modem_lock_get_string((MMModemLock)GPOINTER_TO_UINT(key));
		retries = GPOINTER_TO_UINT(value);
		if (!str) {
			str = g_string_new(NULL);
			g_string_append_printf(str, "%s (%u)", lock_name, retries);
		} else
			g_string_append_printf(str, ", %s (%u)", lock_name, retries);
	}

	return(str ? g_string_free (str, FALSE) : NULL);
}

gchar *agh_mm_common_build_mode_combinations_string(const MMModemModeCombination *modes, guint n_modes) {
	GString *str;
	guint i;
	gchar *allowed;
	gchar *preferred;

	i = 0;
	str = NULL;
	allowed = NULL;
	preferred = NULL;

	if (!modes || !n_modes)
		return g_strdup("none");

	str = g_string_new(NULL);

	for (i = 0; i < n_modes; i++) {
		allowed = mm_modem_mode_build_string_from_mask(modes[i].allowed);
		preferred = mm_modem_mode_build_string_from_mask(modes[i].preferred);
		g_string_append_printf (str, "allowed: %s; preferred: %s",allowed, preferred);
		g_free(allowed);
		g_free(preferred);
		allowed = NULL;
		preferred = NULL;
	}

	return g_string_free(str, FALSE);
}

gchar *agh_mm_common_build_bands_string(const MMModemBand *bands, guint n_bands) {
	GString *str;
	guint i;

	if (!bands || !n_bands)
		return g_strdup("none");

	str = g_string_new(NULL);

	for (i = 0; i < n_bands; i++) {
		g_string_append_printf(str, "%s%s",i ? " " : "",mm_modem_band_get_string(bands[i]));
	}

	return g_string_free(str, FALSE);
}