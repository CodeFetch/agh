#include <glib.h>
#include <glib-unix.h>
#include "agh_commands.h"
#include "agh_logging.h"
#include "agh_xmpp.h"
#include "agh_modem.h"
#include "agh_ubus.h"
#include "agh_ubus_handler.h"

/* Log messages from core domain. */
#define AGH_LOG_DOMAIN_CORE	"CORE"
#define agh_log_core_dbg(message, ...) agh_log_dbg(AGH_LOG_DOMAIN_CORE, message, ##__VA_ARGS__)
#define agh_log_core_info(message, ...) agh_log_info(AGH_LOG_DOMAIN_CORE, message, ##__VA_ARGS__)
#define agh_log_core_crit(message, ...) agh_log_crit(AGH_LOG_DOMAIN_CORE, message, ##__VA_ARGS__)

/*
 * This function continues running until mstate->mainloop_needed is different from 0.
 * The purpose of this function is to allow all of the parts of AGH needing the mainloop to be running to deinitialize correctly.
 * This function respects GLib semantics for GSources.
*/
static gboolean exitsrc_idle_cb(gpointer data) {
	struct agh_state *mstate = data;

	if (!mstate) {
		agh_log_core_crit("no AGH state while in %s GSource",__FUNCTION__);
		return FALSE;
	}

	if (mstate->mainloop_needed)
		return TRUE;
	else {
		mstate->exitsrc = NULL;
		g_main_loop_quit(mstate->agh_mainloop);
	}

	return FALSE;
}

/*
 * Starts AGH exit process, which ideally should lead to program termination. :)
 *
 * Returns: an integer with value 0 on success, or
 *  - 1: passed AGH state was NULL
 *  - 2: GSource already present
 *  - 3: GSource attach failed
*/
static gint agh_start_exit(struct agh_state *mstate) {
	gint retval;

	retval = 0;

	if (!mstate) {
		agh_log_core_crit("no AGH state while starting AGH exit process");
		retval = 1;
		goto out;
	}

	if (mstate->exitsrc) {
		agh_log_core_crit("seems our GSource is already present");
		retval = 2;
		goto out;
	}

	mstate->exitsrc = g_idle_source_new();
	g_source_set_callback(mstate->exitsrc, exitsrc_idle_cb, mstate, NULL);
	mstate->exitsrc_tag = g_source_attach(mstate->exitsrc, mstate->ctx);
	g_source_unref(mstate->exitsrc);
	if (!mstate->exitsrc_tag) {
		agh_log_core_crit("unable to attach mstate->exitsrc GSource to GMainContext");
		mstate->exitsrc = NULL;
		retval = 3;
		goto out;
	}

	mstate->exiting = 1;

	retval = agh_mm_deinit(mstate);
	if (retval)
		agh_log_core_crit("failure when deinitializing MM interaction code (code=%" G_GINT16_FORMAT")", retval);

out:
	return retval;
}

/*
 * This function is invoked when the "quit" operation is detected.
 *
 * Returns: an integer with value 100 on success, 100+<error from agh_start_exit> on failure.
*/
gint agh_core_cmd_cb_quit(struct agh_state *mstate, struct agh_cmd *cmd) {
	gint retval;

	retval = 0;

	retval = agh_start_exit(mstate);
	if (retval) {
		agh_log_core_crit("agh_start_exit failure; I may need some help (code=%" G_GINT16_FORMAT")", retval);
	}

	return 100+retval;
}

/* playground: needs to be removed when no more needed */
/* Core operations. */
static const struct agh_cmd_operation core_ops[] = {
	{
		.op_name = AGH_CMD_QUIT,
		.min_args = 0,
		.max_args = 0,
		.cmd_cb = agh_core_cmd_cb_quit
	},

	{ }
};

/* Print some messages at startup. */
static void agh_hello(void) {
	agh_log_core_info("\n\n******************** Hello! This is AGH %s (%s), compiled %s (%s). ********************",AGH_VERSION,AGH_RELEASE_NAME,__DATE__,__TIME__);
	return;
}

/*
 * This is a GLib GSource, invoked upon UNIX SIGINT signal reception, in this case.
 * This function should respect GLib GSources semantics.
*/
static gboolean agh_unix_signals_cb(gpointer data) {
	struct agh_state *mstate = data;
	gint retval;

	if (!mstate) {
		agh_log_core_crit("we can not be called with a NULL AGH state");
		return FALSE;
	}

	mstate->sigint_received = TRUE;

	retval = agh_start_exit(mstate);
	if (retval) {
		agh_log_core_crit("agh_start_exit failure; enjoy your permanence in AGH, but this needs to be fixed (code=%" G_GINT16_FORMAT")", retval);
	}

	return FALSE;
}

/*
 * This function allocates AGH state in memory via g_try_malloc0, and performs some more setup:
 *  - event loop: we grab the default context, and ask for a new GMainLoop,
 *  - we increment event_id, so it starts from 1. :)
 *
 * Return: a new AGH state pointer on success, NULL on failure.
 *
 * Note: this function assumes logging is already initialised.
*/
static struct agh_state *agh_state_setup(void) {
	struct agh_state *mstate;

	mstate = g_try_malloc0(sizeof *mstate);
	if (!mstate) {
		agh_log_core_crit("state allocation failure");
		return mstate;
	}

	/* Set up the main event loop */
	mstate->ctx = g_main_context_default();
	mstate->agh_mainloop = g_main_loop_new(mstate->ctx, FALSE);
	mstate->event_id++;

	return mstate;
}

/*
 * Sets up AGH signal interception source.
 *
 * Returns: an integer with value  on success, 1 when a NULL AGH state is supplied, 2 when attaching the GSource to GMainContext fails.
*/
static gint agh_sources_setup(struct agh_state *mstate) {
	gint retval;

	retval = 0;

	if (!mstate) {
		agh_log_core_crit("can not install mstate->agh_main_unix_signals on a NULL mstate");
		retval = 1;
		goto out;
	}

	mstate->agh_main_unix_signals = g_unix_signal_source_new(SIGINT);
	g_source_set_callback(mstate->agh_main_unix_signals, agh_unix_signals_cb, mstate, NULL);
	mstate->agh_main_unix_signals_tag = g_source_attach(mstate->agh_main_unix_signals, mstate->ctx);
	g_source_unref(mstate->agh_main_unix_signals);
	if (!mstate->agh_main_unix_signals_tag) {
		agh_log_core_crit("failure while attaching mstate->agh_main_unix_signals to GMainContext");
		mstate->agh_main_unix_signals = NULL;
		retval = 2;
		goto out;
	}

out:
	return retval;
}

/*
 * Deallocates core GSOurces, at the moment UNIX signals interception and the timeout GSource used by the MM interaction code to wait for ubus.
 *
 * Returns: an integer with value 0 on success, or
 *  - 10: when no AGH state was present
*/
static gint agh_sources_teardown(struct agh_state *mstate) {
	gint retval;

	retval = 0;

	if (!mstate) {
		agh_log_core_crit("no AGH state during GSources teardown");
		retval = 10;
		goto out;
	}

	if (mstate->agh_main_unix_signals) {
		agh_log_core_dbg("UNIX signals interception GSource was present");

		if (!mstate->sigint_received)
			g_source_destroy(mstate->agh_main_unix_signals);

		mstate->agh_main_unix_signals_tag = 0;
		mstate->agh_main_unix_signals = NULL;
	}

	if (mstate->ubus_wait_src) {
		agh_log_core_dbg("ubus wait src for MM interaction code was present");
		g_source_destroy(mstate->ubus_wait_src);
		mstate->ubus_wait_src = NULL;
		mstate->ubus_wait_src_tag = 0;
	}

out:
	return retval;
}

/*
 * Deallocates AGH state, unreferencing associated GLib objects (hopefully not forgetting any of them).
 *
 * Returns: an integer with value 0 on failure, or
 *  - 5 when the passed AGH state was NULL
 *  - 1 when there was no mainloop
 *  - 2 when there was no GMainContext.
 *
 * When values 1 or 2 are returned, mstate is still freed.
*/
static gint agh_state_teardown(struct agh_state *mstate) {
	gint retval;

	retval = 0;

	if (!mstate) {
		agh_log_core_crit("no AGH state to deallocate");
		retval = 5;
		goto out;
	}

	if (mstate->agh_mainloop) {
		g_main_loop_unref(mstate->agh_mainloop);
		mstate->agh_mainloop = NULL;
	}
	else {
		agh_log_core_crit("mstate->agh_mainloop was not present, so not unreferencing it");
		retval++;
	}

	if (mstate->ctx) {
		g_main_context_unref(mstate->ctx);
		mstate->ctx = NULL;
	}
	else {
		agh_log_core_crit("no mstate->ctx to unreference");
		retval++;
	}

	g_free(mstate);
out:
	return retval;
}

/*
 * This function does nothing particularly useful at the moment, except for printing a log message.
 *
 * Returns: an integer with value 0 when no signal was received, 1 when passed AGH state was NULL, 2 when a signal was received, hence the corresponding gboolean variable was set in mstate.
*/
static gint agh_process_signals(struct agh_state *mstate) {
	gint retval;

	retval = 0;

	if (!mstate) {
		agh_log_core_crit("no AGH state, can not check for signals");
		retval = 1;
		goto out;
	}

	if (mstate->sigint_received) {
		agh_log_core_crit("exiting because of signal");
		retval = 2;
	}

out:
	return retval;
}

/*
 * This handler is meant to:
 * - receive text messages from handlers willing to send them
 * - build a command using the appropriate functions in agh_commands.c
 * - if building a command succeeds, then build and send around a message containing it.
 *
 * This function should respect AGH handlers semantics: NULL means nothing to say / something gone wrong, a message pointer will be processed by other handlers.
*/
static struct agh_message *core_recvtextcommand_handle(struct agh_handler *h, struct agh_message *m) {
	struct agh_text_payload *csp = m->csp;

	struct agh_message *command_message;
	struct agh_cmd *cmd;

	command_message = NULL;

	if (m->msg_type == MSG_RECVTEXT) {
		/* Parse incoming text. */
		cmd = agh_text_to_cmd(csp->source_id, csp->text);

		if (cmd) {
			command_message = agh_msg_alloc();

			if (command_message) {
				command_message->msg_type = MSG_SENDCMD;
				command_message->csp = cmd;
			}

		} /* valid command was received */

	}

	return command_message;
}

/*
 * Commands for the core are handled in here, so what happens in essence is that agh_cmd_op_match is invoked.
 *
 * This function should respect AGH handlers semantics: NULL means nothing to say / something gone wrong, a message pointer will be processed by other handlers.
*/
static struct agh_message *core_cmd_handle(struct agh_handler *h, struct agh_message *m) {
	struct agh_state *mstate = h->handler_data;
	struct agh_cmd *cmd;

	cmd = m->csp;

	if (m->msg_type != MSG_SENDCMD)
		return NULL;

	agh_cmd_op_match(mstate, core_ops, cmd, 0);

	return agh_cmd_answer_msg(cmd, mstate->comm, NULL);
}

/*
 * This handler converts events to text, and then send the resulting messages to the core itself.
 * This function should respect AGH handlers semantics: NULL means nothing to say / something gone wrong, a message pointer will be processed by other handlers.
*/
static struct agh_message *core_event_to_text_handle(struct agh_handler *h, struct agh_message *m) {
	struct agh_state *mstate = h->handler_data;

	struct agh_cmd *cmd;
	gchar *evtext;
	struct agh_message *evmsg;
	struct agh_text_payload *textcsp;
	gint ret;

	evmsg = NULL;
	ret = 0;

	if (m->msg_type != MSG_EVENT)
		return evmsg;

	/* An event arrived, so we need to convert it to text, and add an event ID. We use agh_cmd_copy / agh_cmd_free, due to the fact agh_cmd_answer_to_text is destructive (cmd->answer will then be NULL). */
	cmd = agh_cmd_copy(m->csp);
	if (!cmd)
		return evmsg;

	evtext = agh_cmd_answer_to_text(cmd, AGH_CMD_EVENT_KEYWORD, mstate->event_id);
	agh_cmd_free(cmd);
	if (!evtext)
		return evmsg;

	evmsg = agh_msg_alloc();
	if (!evmsg) {
		g_free(evtext);
		return evmsg;
	}

	textcsp = g_try_malloc0(sizeof(*textcsp));
	if (!textcsp) {
		g_free(evtext);
		ret = agh_msg_dealloc(evmsg);
		if (ret)
			agh_log_core_crit("failure while deallocating message (code=%" G_GINT16_FORMAT")", ret);

		evmsg = NULL;
		return evmsg;
	}

	/* Connect the pieces we have: */

	/* 1 - the text of the text CSP is the event text we got from agh_cmd_answer_to_text. */
	textcsp->text = evtext;

	/* 2 - the CSP of the event message should be the one we prepared in step 1 */
	evmsg->csp = textcsp;

	/* 3 - (unrelated) set message type */
	evmsg->msg_type = MSG_SENDTEXT;

	mstate->event_id++;

	if (mstate->event_id > AGH_CMD_EVENT_MAX_ID) {
		agh_log_core_crit("resetting mstate->event_id to initial state");
		mstate->event_id = 1;
	}

	return evmsg;
}

/*
 * Converts an XMPP message to text, potentially causing unclean program termination on memory allocation failures.
 *
 * This function should respect AGH handlers semantics: NULL means nothing to say / something gone wrong, a message pointer will be processed by other handlers.
 * Furthermore, it may lead to an unclean program termination.
*/
static struct agh_message *xmppmsg_to_text_handle(struct agh_handler *h, struct agh_message *m) {
	struct agh_text_payload *tcsp;
	struct xmpp_csp *xcsp;
	struct agh_message *tm;
	gint ret;

	tm = NULL;

	if (m->msg_type != MSG_XMPPTEXT)
		return tm;

	xcsp = m->csp;

	if (!xcsp->text)
		return tm;

	tm = agh_msg_alloc();
	if (!tm)
		return tm;

	tcsp = g_try_malloc0(sizeof(*tcsp));
	if (!tcsp) {
		ret = agh_msg_dealloc(tm);
		if (ret)
			agh_log_core_crit("failure while deallocating message (code=%" G_GINT16_FORMAT")", ret);

		tm = NULL;
		return tm;
	}

	tcsp->text = g_strdup(xcsp->text);

	if (xcsp->from)
		tcsp->source_id = g_strdup_printf("XMPP=%s",xcsp->from);

	tm->csp = tcsp;
	tm->msg_type = MSG_RECVTEXT;

	return tm;
}

static gint agh_core_handlers_setup_ext(struct agh_state *mstate) {
	/* Core handlers structs. */
	struct agh_handler *core_recvtextcommand_handler = NULL;
	struct agh_handler *core_cmd_handler = NULL;
	struct agh_handler *core_event_to_text_handler = NULL;
	struct agh_handler *core_ubus_cmd_handler = NULL;
	struct agh_handler *xmppmsg_to_text = NULL;
	gint retval;

	retval = 1;

	if ( !(core_recvtextcommand_handler = agh_new_handler("core_recvtextcommand_handler")) )
		goto out;

	agh_handler_set_handle(core_recvtextcommand_handler, core_recvtextcommand_handle);
	agh_handler_enable(core_recvtextcommand_handler, TRUE);

	if ( !(core_cmd_handler = agh_new_handler("core_cmd_handler")) )
		goto out;

	agh_handler_set_handle(core_cmd_handler, core_cmd_handle);
	agh_handler_enable(core_cmd_handler, TRUE);

	if ( !(core_event_to_text_handler = agh_new_handler("core_event_to_text_handler")) )
		goto out;

	agh_handler_set_handle(core_event_to_text_handler, core_event_to_text_handle);
	agh_handler_enable(core_event_to_text_handler, TRUE);

	if ( !(core_ubus_cmd_handler = agh_new_handler("core_ubus_cmd_handler")) )
		goto out;

	agh_handler_set_handle(core_ubus_cmd_handler, agh_core_ubus_cmd_handle);
	agh_handler_enable(core_ubus_cmd_handler, TRUE);

	if ( !(xmppmsg_to_text = agh_new_handler("xmppmsg_to_text")) )
		goto out;

	agh_handler_set_handle(xmppmsg_to_text, xmppmsg_to_text_handle);
	agh_handler_enable(xmppmsg_to_text, TRUE);

	/* register handlers */
	if (agh_handler_register(mstate->agh_handlers, core_recvtextcommand_handler))
		goto out;

	if (agh_handler_register(mstate->agh_handlers, core_cmd_handler))
		goto out;

	if (agh_handler_register(mstate->agh_handlers, core_event_to_text_handler))
		goto out;

	if (agh_handler_register(mstate->agh_handlers, core_ubus_cmd_handler))
		goto out;

	if (agh_handler_register(mstate->agh_handlers, xmppmsg_to_text))
		goto out;

	retval = 0;

out:
	if (retval) {
		g_clear_pointer(&core_recvtextcommand_handler, agh_handler_dealloc);
		g_clear_pointer(&core_cmd_handler, agh_handler_dealloc);
		g_clear_pointer(&core_event_to_text_handler, agh_handler_dealloc);
		g_clear_pointer(&core_ubus_cmd_handler, agh_handler_dealloc);
		g_clear_pointer(&xmppmsg_to_text, agh_handler_dealloc);
	}

	return retval;
}

/*
 * Really a wrapper around agh_comm_set_teardown_state.

 * Returns: an integer with value 40 when passed AGH state is NULL or there was no COMM; any other value comes directly from wrapped function.
*/
static gint agh_exit(struct agh_state *mstate) {
	gint retval;

	retval = 0;

	if (!mstate || !mstate->comm) {
		agh_log_core_crit("no AGH state while setting COMM teardown state");
		retval = 40;
		goto out;
	}

	retval = agh_comm_set_teardown_state(mstate->comm, TRUE);

out:
	return retval;
}

/*
 * Utility function, metant to copy text fragments from one GQueue to another. Complains about NULL GQueue or text; the first of these conditions is checked in GLib g_queue_push_tail as well.
 * I am pretty sure we do not need this function.
 *
 * Note: this function may lead to unclean program termination.
*/
void agh_copy_textparts(gpointer data, gpointer user_data) {
	GQueue *destqueue = user_data;
	gchar *text = data;

	if (!destqueue || !text) {
		agh_log_core_crit("mhm, agh_copy_textparts invoked on a NULL destqueue or text, and since g_queue_new can not fail, and passing a NULL pointer here is not a good idea...");
		goto out;
	}

	g_queue_push_tail(destqueue, g_strdup(data));

out:
	return;
}

gint main(void) {
	struct agh_state *mstate;
	gint retval;
	gint nonfatal_retval;

	retval = 0;
	nonfatal_retval = 0;

	/* Operations not allowed to fail. */
	agh_logging_init();
	agh_hello();

	mstate = agh_state_setup();
	if (!mstate) {
		retval = 1;
		goto out;
	}

	/* just a wrapper around g_queue_new */
	mstate->agh_handlers = agh_handlers_setup();

	mstate->comm = agh_comm_setup(mstate->agh_handlers, mstate->ctx, AGH_LOG_DOMAIN_CORE);
	if (!mstate->comm) {
		retval = 2;
		agh_log_core_crit("failure in agh_comm_setup");
		goto out;
	}

	retval = agh_core_handlers_setup_ext(mstate);
	if (retval) {
		agh_log_core_crit("unable to set up core handlers (code=%" G_GINT16_FORMAT")", retval);
		goto out;
	}

	retval = agh_sources_setup(mstate);
	if (retval) {
		agh_log_core_crit("failure while setting up core GSources");
		goto out;
	}

	mstate->uctx = agh_ubus_setup(mstate->comm, &nonfatal_retval);
	if (nonfatal_retval)
		agh_log_core_crit("ubus code init failure (code=%" G_GINT16_FORMAT")", nonfatal_retval);

	nonfatal_retval = agh_xmpp_init(mstate);
	if (nonfatal_retval)
		agh_log_core_crit("XMPP code init failure (code=%" G_GINT16_FORMAT")", nonfatal_retval);

	nonfatal_retval = agh_mm_init(mstate);
	if (nonfatal_retval)
		agh_log_core_crit("MM code init failure (code=%" G_GINT16_FORMAT")", nonfatal_retval);

	agh_handlers_init(mstate->agh_handlers, mstate);

	g_main_loop_run(mstate->agh_mainloop);

	agh_log_core_crit("seems we exited from the main loop");

out:

	retval = agh_process_signals(mstate);
	if (retval)
		agh_log_core_crit("failure from agh_process_signals (code=%" G_GINT16_FORMAT")", retval);

	if (mstate) {
		retval = agh_ubus_teardown(mstate->uctx);
		if (retval)
			agh_log_core_crit("failure when trying to deinit ubus (code=%" G_GINT16_FORMAT")", retval);
	}

	retval = agh_xmpp_deinit(mstate);
	if (retval)
		agh_log_core_crit("failure when deinitializing XMPP code (code=%" G_GINT16_FORMAT")", retval);

	retval = agh_sources_teardown(mstate);
	if (retval)
		agh_log_core_crit("failure when deinitializing core GSources (code=%" G_GINT16_FORMAT")", retval);

	agh_exit(mstate);

	if (mstate && mstate->agh_handlers) {
		agh_handlers_finalize(mstate->agh_handlers);
		agh_handlers_teardown(mstate->agh_handlers);
		mstate->agh_handlers = NULL;
	}

	if (mstate)
		agh_comm_teardown(mstate->comm, FALSE);

	agh_state_teardown(mstate);

	return retval;
}
