/*
 * So here we are, for the second time: after the HDD breakage.
*/

#include <glib.h>
#include <libmm-glib.h>
#include "agh_modem.h"
#include "agh.h"
#include "agh_logging.h"
#include "agh_handlers.h"
#include "agh_mm_handlers.h"
#include "agh_modem_config.h"

/* Log messages from AGH_LOG_DOMAIN_MODEM domain. */
#define AGH_LOG_DOMAIN_MODEM "MM"

/* Logging macros. */
#define agh_log_mm_dbg(message, ...) agh_log_dbg(AGH_LOG_DOMAIN_MODEM, message, ##__VA_ARGS__)
#define agh_log_mm_crit(message, ...) agh_log_crit(AGH_LOG_DOMAIN_MODEM, message, ##__VA_ARGS__)

void agh_mm_init(struct agh_state *mstate) {
	struct agh_mm_state *mmstate;
	struct agh_modem_config_validation_error *validation_error;

	validation_error = NULL;

	mmstate = g_try_malloc0(sizeof(*mmstate));
	if (!mmstate) {
		agh_log_mm_crit("can not allocate state structure");
		return;
	}

	mstate->mmstate = mmstate;

	agh_modem_validate_config(mstate, "agh_modem", &validation_error);

	if (validation_error) {
		agh_log_mm_crit("failure %" G_GINT16_FORMAT" (%s): %s",validation_error->error_code,validation_error->element_name ? validation_error->element_name : "**",validation_error->error_desc);
		agh_modem_config_validation_error_free(validation_error);
		mmstate->mctx = NULL;
		mmstate->uci_package = NULL;
		g_free(mmstate);
		mstate->mmstate = NULL;
		return;
	}

	return;
}

void agh_mm_deinit(struct agh_state *mstate) {
	struct agh_mm_state *mmstate;

	mmstate = NULL;

	return;
}
