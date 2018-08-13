#include <glib.h>
#include <glib-unix.h>
#include "agh_commands.h"
#include "agh_handlers.h"
#include "agh_xmpp.h"
#include "agh_modem.h"
#include "agh_ubus.h"
#include "agh_ubus_handler.h"

gint main(void) {

	struct agh_state *mstate;

	mstate = agh_state_setup();

	mstate->agh_handlers = handlers_setup();
	mstate->comm = agh_comm_setup(mstate->agh_handlers, mstate->ctx, "AGH");

	/* This will need to be done in a better way; handlers are registered from within the function called here. */
	agh_core_handlers_setup_ext(mstate);

	/*
	 * We init XMPP here to give a chance to XMPP code to set up its handlers.
	 * But we may directly invoke xmpp_set_handlers_ext here, especially if at some point XMPP init needs other parts of AGH to be active.
	*/
	agh_xmpp_init(mstate);
	//agh_modem_init(mstate);

	agh_sources_setup(mstate);

	agh_threads_setup(mstate);

	/* Here you can set up threads. */

	/* ubus connection */
	mstate->uctx = agh_ubus_setup(mstate->comm);

	agh_threads_prepare(mstate);

	agh_threads_start(mstate);

	//g_usleep(40*G_USEC_PER_SEC);

	g_print("%s: entering main loop\n",__FUNCTION__);

	g_main_loop_run(mstate->agh_mainloop);

	g_print("%s: main loop exited\n",__FUNCTION__);

	agh_exit(mstate);

	process_signals(mstate);

	agh_threads_stop(mstate);
	agh_threads_deinit(mstate);
	agh_threads_teardown(mstate);

	if (mstate->uctx) {
		agh_ubus_teardown(mstate->uctx);
	}

	agh_xmpp_deinit(mstate);
	//agh_mm_deinit(mstate);

	agh_sources_teardown(mstate);
	handlers_finalize(mstate->agh_handlers);
	handlers_teardown(mstate->agh_handlers);
	agh_comm_teardown(mstate->comm);
	agh_state_teardown(mstate);
	return 0;
}

struct agh_state * agh_state_setup(void) {
	struct agh_state *mstate;

	/* Note: GLib will terminate the application should this memory allocation fail. */
	mstate = g_malloc0(sizeof *mstate);

	/* Set up the main event loop */
	mstate->ctx = g_main_context_default();
	mstate->agh_mainloop = g_main_loop_new(mstate->ctx, FALSE);

	return mstate;
}

void agh_sources_setup(struct agh_state *mstate) {
	/* Intercepts SIGINT UNIX signal. This is useful at least to exit the main loop gracefully (on ctrl+c press) */
	mstate->agh_main_unix_signals = g_unix_signal_source_new(SIGINT);
	g_source_set_callback(mstate->agh_main_unix_signals, agh_unix_signals_cb, mstate, NULL);
	mstate->agh_main_unix_signals_tag = g_source_attach(mstate->agh_main_unix_signals, mstate->ctx);
	g_source_unref(mstate->agh_main_unix_signals);

	handlers_init(mstate->agh_handlers, mstate);

	return;
}

void agh_sources_teardown(struct agh_state *mstate) {

	/* UNIX SIGINT signal source */
	if (!mstate->sigint_received)
		g_source_destroy(mstate->agh_main_unix_signals);

	mstate->agh_main_unix_signals_tag = 0;
	mstate->agh_main_unix_signals = NULL;

	return;
}

void agh_state_teardown(struct agh_state *mstate) {
	/* XXX is this the proper order? */
	g_main_loop_unref(mstate->agh_mainloop);
	g_main_context_unref(mstate->ctx);
	mstate->agh_mainloop = NULL;
	mstate->ctx = NULL;
	mstate->comm = NULL;
	g_free(mstate);
	mstate = NULL;
	return;
}

void process_signals(struct agh_state *mstate) {

	if (mstate->sigint_received)
		g_print("%s: exiting because of SIGINT!\n",__FUNCTION__);

	return;
}

void agh_threads_start(struct agh_state *mstate) {
	g_print("%s: starting threads: ",__FUNCTION__);
	g_queue_foreach(mstate->agh_threads, agh_threads_start_single, mstate);
	g_print("done\n");
}

void agh_thread_register(struct agh_state *mstate, struct agh_thread *ct) {
	if (!ct) {
		g_print("%s: ** INVALID THREAD REGISTRATION CALL: NULL VALUE NOT ACCEPTABLE AS A THREAD\n",__FUNCTION__);
		return;
	}

	if (!mstate->agh_threads) {
		g_print("%s: ** INVALID THREAD REGISTRATION CALL: THREADS QUEUE NOT INITIALIZED\n",__FUNCTION__);
		return;
	}
	
	g_print("%s: registering %s thread: ",__FUNCTION__,ct->thread_name);
	g_queue_push_tail(mstate->agh_threads, ct);
	g_print(" done\n");
	return;
}

void agh_threads_stop(struct agh_state *mstate) {
	g_print("Stopping threads: ");

	g_queue_foreach(mstate->agh_threads, agh_threads_stop_single, mstate);
	g_print("done\n");
}

void agh_threads_setup(struct agh_state *mstate) {
	mstate->agh_threads = g_queue_new();
	return;
}

void agh_threads_teardown(struct agh_state *mstate) {
	g_print("%s: deallocating threads queue\n",__FUNCTION__);
	g_queue_free(mstate->agh_threads);
	mstate->agh_threads = NULL;
	return;
}

void agh_threads_prepare(struct agh_state *mstate) {
	g_print("%s: preparing threads \n",__FUNCTION__);

	g_queue_foreach(mstate->agh_threads, agh_threads_prepare_single, mstate);
	g_print(" done\n");
	return;
}

void agh_threads_deinit(struct agh_state *mstate) {
	g_print("%s: invoking threads deinit functions ",__FUNCTION__);
	g_queue_foreach(mstate->agh_threads, agh_threads_deinit_single, mstate);
	g_print("done\n");
	return;
}

void agh_threads_prepare_single(gpointer data, gpointer user_data) {
	struct agh_thread *ct = data;
	struct agh_state *mstate = user_data;

	g_print("%s\n",ct->thread_name);

	ct->agh_maincontext = mstate->ctx;
	ct->agh_mainloop = mstate->agh_mainloop;
	ct->agh_comm = mstate->comm;
	ct->handlers = NULL;

	/* We would like to be informed about this situation: and if someone finds this useful we are going to remove this message
	* and think about what's going on and what's needed. I don't think there's a valid reason for doing this, but who knows.
	*/
	if (ct->agh_thread_init)
		ct->agh_thread_init(ct);
	else
		g_print("%s: %s has no init function\n",__FUNCTION__,ct->thread_name);

	return;
}

void agh_threads_deinit_single(gpointer data, gpointer user_data) {
	struct agh_thread *ct = data;
	struct agh_state *mstate = user_data;

	g_print("%s\n",ct->thread_name);
	if (ct->agh_thread_deinit)
		ct->agh_thread_deinit(ct);

	if (ct->handlers)
		g_print("%s: WARNING: %s thread may not be deinitializing its handlers\n",__FUNCTION__, ct->thread_name);

	ct->agh_maincontext = NULL;
	ct->agh_mainloop = NULL;
	ct->comm = NULL;
	ct->agh_comm = NULL;

	if (ct->evl) {
		g_main_loop_unref(ct->evl);

		if (ct->evl_ctx)
			g_main_context_unref(ct->evl_ctx);

	}

	g_free(ct->thread_name);

	/* XXX: a better way to do this? */
	g_queue_remove(mstate->agh_threads, ct);
	g_free(ct);
	return;
}

void agh_threads_start_single(gpointer data, gpointer user_data) {
	struct agh_thread *ct = data;
	struct agh_state *mstate = user_data;

	g_print("%s ",ct->thread_name);

	if (ct->agh_thread_main)
		ct->current_thread = g_thread_new(ct->thread_name, ct->agh_thread_main, ct);
	else
		g_print("%s: %s has a NULL main function\n",__FUNCTION__,ct->thread_name);

	return;
}

void agh_threads_stop_single(gpointer data, gpointer user_data) {
	struct agh_thread *ct = data;
	struct agh_state *mstate = user_data;

	g_print("%s ",ct->thread_name);
	ct->current_thread = g_thread_join(ct->current_thread);
	return;
}

/* Invoked upon SIGINT reception. */
gboolean agh_unix_signals_cb(gpointer data) {
	struct agh_state *mstate = data;

	mstate->sigint_received = TRUE;
	agh_start_exit(mstate);

	return FALSE;
}

/*
 * This handler is meant to:
 * - receive text messages from components willing to send them
 * - build a command using the appropriate functions in commands.c
 * - if building a command succeeds, then build and send around a message containing it
*/
gpointer core_recvtextcommand_handle(gpointer data, gpointer hmessage) {
	struct agh_message *m = hmessage;
	struct handler *h = data;
	struct text_csp *csp = m->csp;
	struct agh_state *mstate = h->handler_data;
	struct agh_message *command_message;
	struct command *cmd;
	guint num_threads;
	guint i;
	struct agh_thread *ct;

	command_message = NULL;
	cmd = NULL;
	num_threads = 0;
	ct = NULL;
	i = 0;

	if (m->msg_type == MSG_RECVTEXT) {
		/* Parse incoming text. */
		cmd = text_to_cmd(csp->text);

		if (cmd) {
			/* Send this message around. */
			g_print("%s: processing CMD\n",__FUNCTION__);

			num_threads = g_queue_get_length(mstate->agh_threads);
			if (num_threads) {

				for (i=0;i<num_threads;i++) {
					command_message = msg_alloc();
					command_message->msg_type = MSG_SENDCMD;
					ct = g_queue_peek_nth(mstate->agh_threads, i);
					//g_print("Sending command message to %s thread\n",ct->thread_name);
					command_message->csp = cmd_copy(cmd);
					if (msg_send(command_message, mstate->comm, ct->comm)) {
						g_print("%s: error while sending message to %s thread\n",__FUNCTION__,ct->thread_name);
						continue;
					}

				} /* end of for loop */

			} /* num_threads not zero */

			/*
			 * Send command messages to the core itself.
			*/
			command_message = msg_alloc();
			command_message->msg_type = MSG_SENDCMD;
			command_message->csp = cmd;
			if (msg_send(command_message, mstate->comm, NULL))
				g_print("%s: error while sending message to self\n",__FUNCTION__);

		} /* valid command was received */

	}

	return NULL;
}

/*
 * This handler receives MSG_SENDTEXT text messages from sources willing to send them, and broadcast them around.
*/
gpointer core_sendtext_handle(gpointer data, gpointer hmessage) {
	struct agh_message *m = hmessage;
	struct handler *h = data;
	struct text_csp *csp = m->csp;
	struct agh_state *mstate = h->handler_data;
	guint num_threads;
	guint i;
	struct agh_thread *ct;
	struct agh_message *text_message;
	struct text_csp *ncsp;

	num_threads = 0;
	ct = NULL;
	i = 0;
	text_message = NULL;
	ncsp = NULL;

	if (m->msg_type == MSG_SENDTEXT) {
		/* Send this message around to all registered components. */
		g_print("%s: broadcasting text\n",__FUNCTION__);

		num_threads = g_queue_get_length(mstate->agh_threads);
		if (num_threads) {

			for (i=0;i<num_threads;i++) {
				text_message = msg_alloc();
				text_message->msg_type = MSG_SENDTEXT;
				ncsp = g_malloc0(sizeof(struct text_csp));
				ncsp->text = g_strdup(csp->text);
				text_message->csp = ncsp;
				ct = g_queue_peek_nth(mstate->agh_threads, i);
				//g_print("Sending text message to %s thread\n",ct->thread_name);
				if (msg_send(text_message, mstate->comm, ct->comm)) {
					g_print("%s: error while sending text message to %s thread (not referring to an SMS message here)\n",__FUNCTION__,ct->thread_name);
					continue;
				}

			} /* end of for loop */

		} /* num_threads not zero */

	} /* valid command was received */

	return NULL;
}

gpointer core_cmd_handle(gpointer data, gpointer hmessage) {
	struct handler *h = data;
	struct agh_message *m = hmessage;
	struct agh_state *mstate = h->handler_data;
	struct command *cmd;

	cmd = m->csp;

	if (m->msg_type != MSG_SENDCMD)
		return NULL;

	if (!g_strcmp0(cmd_get_operation(cmd), AGH_CMD_QUIT)) {
		agh_start_exit(mstate);
	}

	return NULL;
}

/*
 * This handler converts events to text, and then send the resulting messages to the core itself.
 * The core is expected to broadcast them, for the benefit of other threads (e.g. XMPP).
*/
gpointer core_event_to_text_handle(gpointer data, gpointer hmessage) {
	struct handler *h = data;
	struct agh_message *m = hmessage;
	struct agh_state *mstate = h->handler_data;
	struct command *cmd;
	gchar *evtext;
	struct agh_message *evmsg;
	struct text_csp *textcsp;

	cmd = NULL;
	evtext = NULL;
	evmsg = NULL;
	textcsp = NULL;

	if (m->msg_type != MSG_EVENT)
		return evmsg;

	/* An event arrived, so we need to convert it to text, and add an event ID. We use cmd_copy / cmd_free, due to the fact cmd_event_to_text is destructive (cmd->answer will be NULL). */
	cmd = cmd_copy(m->csp);
	evtext = cmd_event_to_text(cmd, mstate->event_id);

	cmd_free(cmd);

	if (!evtext)
		return evmsg;

	mstate->event_id++;
	if (mstate->event_id == CMD_EVENT_MAX_ID)
		mstate->event_id = 0;

	evmsg = msg_alloc();

	textcsp = g_malloc0(sizeof(struct text_csp));
	textcsp->text = evtext;
	evmsg->csp = textcsp;
	evmsg->msg_type = MSG_SENDTEXT;

	if (msg_send(evmsg, mstate->comm, NULL)) {
		evmsg = NULL;
	}

	return NULL;
}

gpointer core_event_broadcast_handle(gpointer data, gpointer hmessage) {
	struct handler *h = data;
	struct agh_message *m = hmessage;
	struct agh_state *mstate = h->handler_data;
	struct command *cmd;
	struct agh_message *evmsg;
	guint num_threads;
	struct command *ncmd;
	guint i;
	struct agh_thread *ct;

	cmd = m->csp;
	evmsg = NULL;
	num_threads = 0;
	ncmd = NULL;
	ct = NULL;

	if (m->msg_type != MSG_EVENT)
		return NULL;

	/* An event arrived - so we'll broadcast it. */
	g_print("%s: broadcasting event\n",__FUNCTION__);

	num_threads = g_queue_get_length(mstate->agh_threads);
	if (num_threads) {

		for (i=0;i<num_threads;i++) {
			evmsg = msg_alloc();
			evmsg->msg_type = MSG_EVENT;
			ncmd = cmd_copy(cmd);
			evmsg->csp = ncmd;
			ct = g_queue_peek_nth(mstate->agh_threads, i);
			if (msg_send(evmsg, mstate->comm, ct->comm)) {
				g_print("%s: error while sending event message to %s thread\n",__FUNCTION__,ct->thread_name);
				continue;
			}

		} /* end of for loop */

	} /* num_threads not zero */

	return NULL;
}

struct agh_thread *agh_thread_new(gchar *name) {
	struct agh_thread *ct;

	ct = NULL;

	if (!name) {
		g_print("%s: a thread may not have a NULL name\n",__FUNCTION__);
		return ct;
	}

	ct = g_malloc0(sizeof(struct agh_thread));
	ct->thread_name = g_strdup(name);
	return ct;
}

void agh_thread_set_init(struct agh_thread *ct, void (*agh_thread_init_cb)(gpointer data)) {
	ct->agh_thread_init = agh_thread_init_cb;
	return;
}

void agh_thread_set_main(struct agh_thread *ct, gpointer (*agh_thread_main_cb)(gpointer data)) {
	ct->agh_thread_main = agh_thread_main_cb;
	return;
}

void agh_thread_set_deinit(struct agh_thread *ct, void (*agh_thread_deinit_cb)(gpointer data)) {
	ct->agh_thread_deinit = agh_thread_deinit_cb;
	return;
}

void agh_core_handlers_setup_ext(struct agh_state *mstate) {
	/* Core handlers. */
	struct handler *core_recvtextcommand_handler;
	struct handler *core_sendtext_handler;
	struct handler *core_cmd_handler;
	struct handler *core_event_to_text_handler;
	struct handler *core_event_broadcast_handler;
	struct handler *core_ubus_cmd_handler;
	struct handler *xmppmsg_to_text;

	core_recvtextcommand_handler = NULL;
	core_cmd_handler = NULL;
	core_sendtext_handler = NULL;
	core_event_to_text_handler = NULL;
	core_event_broadcast_handler = NULL;
	core_ubus_cmd_handler = NULL;
	xmppmsg_to_text = NULL;

	core_recvtextcommand_handler = handler_new("core_recvtextcommand_handler");
	handler_set_handle(core_recvtextcommand_handler, core_recvtextcommand_handle);
	handler_enable(core_recvtextcommand_handler, TRUE);

	core_cmd_handler = handler_new("core_cmd_handler");
	handler_set_handle(core_cmd_handler, core_cmd_handle);
	handler_enable(core_cmd_handler, TRUE);

	core_sendtext_handler = handler_new("core_sendtext_handler");
	handler_set_handle(core_sendtext_handler, core_sendtext_handle);
	handler_enable(core_sendtext_handler, TRUE);

	core_event_to_text_handler = handler_new("core_event_to_text_handler");
	handler_set_handle(core_event_to_text_handler, core_event_to_text_handle);
	handler_enable(core_event_to_text_handler, TRUE);

	core_event_broadcast_handler = handler_new("core_event_broadcast_handler");
	handler_set_handle(core_event_broadcast_handler, core_event_broadcast_handle);
	handler_enable(core_event_broadcast_handler, TRUE);

	core_ubus_cmd_handler = handler_new("core_ubus_cmd_handler");
	handler_set_handle(core_ubus_cmd_handler, agh_core_ubus_cmd_handle);
	handler_enable(core_ubus_cmd_handler, TRUE);

	xmppmsg_to_text = handler_new("xmppmsg_to_text");
	handler_set_handle(xmppmsg_to_text, xmppmsg_to_text_handle);
	handler_enable(xmppmsg_to_text, TRUE);

	handler_register(mstate->agh_handlers, core_recvtextcommand_handler);
	handler_register(mstate->agh_handlers, core_sendtext_handler);
	handler_register(mstate->agh_handlers, core_cmd_handler);
	handler_register(mstate->agh_handlers, core_event_to_text_handler);
	handler_register(mstate->agh_handlers, core_event_broadcast_handler);
	handler_register(mstate->agh_handlers, core_ubus_cmd_handler);
	handler_register(mstate->agh_handlers, xmppmsg_to_text);

	return;
}

void agh_thread_eventloop_setup(struct agh_thread *ct, gboolean as_default_context) {

	if (!ct->handlers) {
		g_print("%s: (%s) called us with a NULL handlers queue pointer\n\t(maybe you forgot to call handlers_setup ? )\n",__FUNCTION__,ct->thread_name);
		return;
	}

	/* Sets up a new Main Loop Context (and related Main Loop of course). */
	ct->evl_ctx = g_main_context_new();

	ct->evl = g_main_loop_new(ct->evl_ctx, FALSE);

	if (as_default_context)
		g_main_context_push_thread_default(ct->evl_ctx);

	return;
}

void agh_thread_eventloop_teardown(struct agh_thread *ct) {
	/* XXX is this the right order? */
	g_main_loop_unref(ct->evl);
	if (ct->evl_ctx)
		g_main_context_unref(ct->evl_ctx);
	else
		g_print("%s had a NULL ct->evl_ctx\n",ct->thread_name);
	ct->evl = NULL;
	ct->evl_ctx = NULL;

	return;
}

gpointer agh_thread_default_exit_handle(gpointer data, gpointer hmessage) {
	struct agh_message *m = hmessage;
	struct handler *h = data;
	struct agh_thread *ct = h->handler_data;

	if (m->msg_type != MSG_EXIT)
		return NULL;

	agh_comm_disable(ct->comm, TRUE);
	g_main_loop_quit(ct->evl);

	return NULL;
}

void agh_broadcast_exit(struct agh_state *mstate) {
	guint num_threads;
	struct agh_message *m;
	struct agh_thread *ct;
	guint i;

	num_threads = 0;
	m = NULL;
	ct = NULL;

	if (!mstate->agh_threads)
		return;

	num_threads = g_queue_get_length(mstate->agh_threads);

	for (i=0;i<num_threads;i++) {
		m = msg_alloc();
		m->msg_type = MSG_EXIT;
		ct = g_queue_peek_nth(mstate->agh_threads, i);

		if (msg_send(m, mstate->comm, ct->comm))
			g_print("%s: error while sending exit message to %s\n",__FUNCTION__,ct->thread_name);

	}

	return;
}

void agh_exit(struct agh_state *mstate) {
	agh_comm_disable(mstate->comm, TRUE);
	agh_broadcast_exit(mstate);
	return;
}

void agh_start_exit(struct agh_state *mstate) {
	mstate->exiting = 1;

	/* Install exit process idle source. */
	if (mstate->exitsrc)
		return;

	mstate->exitsrc = g_idle_source_new();
	g_source_set_callback(mstate->exitsrc, exitsrc_idle_cb, mstate, NULL);
	mstate->exitsrc_tag = g_source_attach(mstate->exitsrc, mstate->ctx);
	g_source_unref(mstate->exitsrc);

	return;
}

gboolean exitsrc_idle_cb(gpointer data) {
	struct agh_state *mstate = data;

	if (mstate->mainloop_needed)
		return TRUE;
	else {
		mstate->exitsrc = NULL;
		g_main_loop_quit(mstate->agh_mainloop);
	}

	return FALSE;
}

void agh_copy_textparts(gpointer data, gpointer user_data) {
	GQueue *destqueue = user_data;

	/*
	 * The function calling us should check if the destination queue actually has been allocated. Anyway, it seems g_queue_new
	 * can not fail.
	*/
	g_queue_push_tail(destqueue, g_strdup(data));
	return;
}

gpointer xmppmsg_to_text_handle(gpointer data, gpointer hmessage) {
	struct agh_message *m = hmessage;
	struct handler *h = data;
	struct agh_state *mstate = h->handler_data;

	struct text_csp *tcsp;
	struct xmpp_csp *xcsp;
	struct agh_message *tm;

	tcsp = NULL;
	xcsp = NULL;
	tm = NULL;

	if (m->msg_type != MSG_XMPPTEXT)
		return NULL;

	xcsp = m->csp;

	if (!xcsp->text)
		return NULL;

	tm = msg_alloc();
	tcsp = g_malloc0(sizeof(struct text_csp));
	tcsp->text = g_strdup(xcsp->text);
	tm->csp = tcsp;
	tm->msg_type = MSG_RECVTEXT;

	if (msg_send(tm, mstate->comm, NULL)) {
		g_print("%s: unable to send message\n",__FUNCTION__);
	}

	return NULL;
}
