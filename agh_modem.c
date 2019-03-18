/*
 * So here we are, for the second time: after the HDD breakage.
 * Subsequently, it got rewritten: the previous one wasn't so great.
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

gint agh_mm_init(struct agh_state *mstate) {
	struct agh_mm_state *mmstate;
	gint ret;

	ret = 0;

	mmstate = g_try_malloc0(sizeof(*mmstate));
	if (!mmstate) {
		agh_log_mm_crit("can not allocate AGH MM state structure");
		ret = -10;
		goto out;
	}

	ret = agh_modem_validate_config(mmstate, "agh_modem");
	if (ret) {
		agh_modem_report_gerror_message(&mmstate->current_gerror);
	}

out:
	if (ret)
		g_free(mmstate);
	else
		mstate->mmstate = mmstate;

	return ret;
}

gint agh_mm_deinit(struct agh_state *mstate) {
	struct agh_mm_state *mmstate;
	gint ret;

	ret = 0;

	if (!mstate || !mstate->mmstate) {
		agh_log_mm_crit("no AGH mm context found");
		ret = 1;
		goto out;
	}

	mmstate = mstate->mmstate;

	if (mmstate->mctx) {
		uci_unload(mmstate->mctx, mmstate->uci_package);
		uci_free_context(mmstate->mctx);
	}

out:
	return ret;
}


gint agh_modem_report_gerror_message(GError **error) {
	gint retval;

	retval = 0;

	if (!error || !*error) {
		agh_log_mm_crit("asked to report a NULL GError, or one pointing to a NULL pointer");
		retval = 1;
		goto out;
	}

	agh_log_mm_crit("(GError report) - %s",(*error)->message ? (*error)->message : "(no error message)");
	g_error_free(*error);
	error = NULL;

out:
	return retval;
}
