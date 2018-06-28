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
