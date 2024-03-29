// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * This file contains some helpers function. Some of them are used to build strings out of various MM data structures.
 * Much of the code in this file comes from libmm-glib/mm-common-helpers.c; it may be present here in a modified form, or in a completely different one.
 * Or it may not come from that file. So any bugs in this code should be reported to me, not the original author, Aleksander Morgado!
 * I am very very thankful to him, as without his work AGH would not exist!
 *
 * Function names have been kept in some cases, but the "agh_" prefix was added to avoid naming collisions.
*/

#include <libmm-glib.h>
#include "agh.h"
#include "agh_modem.h"
#include "agh_mm_helpers.h"

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
		tmp = mm_modem_capability_build_string_from_mask(capabilities[i]);
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
		tmp = mm_modem_port_type_get_string(ports[i].type);
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
		g_string_append_printf (str, "(allowed: %s; preferred: %s)",allowed, preferred);
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

gchar *
agh_mm_utils_bin2hexstr (const guint8 *bin, gsize len)
{
    GString *ret;
    gsize i;

    g_return_val_if_fail (bin != NULL, NULL);

    ret = g_string_sized_new (len * 2 + 1);
    for (i = 0; i < len; i++)
        g_string_append_printf (ret, "%.2X", bin[i]);
    return g_string_free (ret, FALSE);
}

/* copied verbatim, including comments */
const gchar *
agh_mm_sms_delivery_state_get_string_extended (guint delivery_state)
{
    if (delivery_state > 0x02 && delivery_state < 0x20) {
        if (delivery_state < 0x10)
            return "completed-reason-reserved";
        else
            return "completed-sc-specific-reason";
    }

    if (delivery_state > 0x25 && delivery_state < 0x40) {
        if (delivery_state < 0x30)
            return "temporary-error-reason-reserved";
        else
            return "temporary-error-sc-specific-reason";
    }

    if (delivery_state > 0x49 && delivery_state < 0x60) {
        if (delivery_state < 0x50)
            return "error-reason-reserved";
        else
            return "error-sc-specific-reason";
    }

    if (delivery_state > 0x65 && delivery_state < 0x80) {
        if (delivery_state < 0x70)
            return "temporary-fatal-error-reason-reserved";
        else
            return "temporary-fatal-error-sc-specific-reason";
    }

    if (delivery_state >= 0x80 && delivery_state < 0x100)
        return "unknown-reason-reserved";

    if (delivery_state >= 0x100)
        return "unknown";

    /* Otherwise, use the MMSmsDeliveryState enum as we can match the known
     * value */
    return mm_sms_delivery_state_get_string ((MMSmsDeliveryState)delivery_state);
}

/* from mmcli */

const gchar *agh_mm_get_statechange_reason_string(MMModemStateChangeReason reason) {
	switch (reason) {
		case MM_MODEM_STATE_CHANGE_REASON_UNKNOWN:
			return "unknown";
		case MM_MODEM_STATE_CHANGE_REASON_USER_REQUESTED:
			return "requested";
		case MM_MODEM_STATE_CHANGE_REASON_SUSPEND:
			return "suspend";
		case MM_MODEM_STATE_CHANGE_REASON_FAILURE:
			return "failure";
		default:
			return "internal_issue";
	}

	return NULL;
}

MMModemMode
agh_mm_common_get_modes_from_string (const gchar *str, const gchar *separator,
                                 GError **error)
{
    GError *inner_error = NULL;
    MMModemMode modes;
    gchar **mode_strings;
    GFlagsClass *flags_class;

    modes = MM_MODEM_MODE_NONE;

    flags_class = G_FLAGS_CLASS (g_type_class_ref (MM_TYPE_MODEM_MODE));
    mode_strings = g_strsplit (str, separator, -1);

    if (mode_strings) {
        guint i;

        for (i = 0; mode_strings[i]; i++) {
            guint j;
            gboolean found = FALSE;

            for (j = 0; flags_class->values[j].value_nick; j++) {
                if (!g_ascii_strcasecmp (mode_strings[i], flags_class->values[j].value_nick)) {
                    modes |= flags_class->values[j].value;
                    found = TRUE;
                    break;
                }
            }

            if (!found) {
                inner_error = g_error_new (
                    MM_CORE_ERROR,
                    MM_CORE_ERROR_INVALID_ARGS,
                    "Couldn't match '%s' with a valid MMModemMode value",
                    mode_strings[i]);
                break;
            }
        }
    }

    if (inner_error) {
        g_propagate_error (error, inner_error);
        modes = MM_MODEM_MODE_NONE;
    }

    g_type_class_unref (flags_class);
    g_strfreev (mode_strings);
    return modes;
}

/* local */
gchar *agh_mm_modem_to_index(const gchar *modem_path) {
	GString *modem_index;
	gsize modem_path_size;
	gint i;

	if (!modem_path)
		return NULL;

	modem_path_size = strlen(modem_path);
	if (!modem_path_size)
		return NULL;

	modem_index = g_string_new(NULL);

	for (i=modem_path_size-1;i>=0;i--) {
		if (modem_path[i] == '/')
			break;
	}

	i++;

	while (modem_path[i]) {
		g_string_append_c(modem_index, modem_path[i]);
		i++;
	}

	return g_string_free(modem_index, FALSE);
}
