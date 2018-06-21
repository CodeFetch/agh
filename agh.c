#include <glib.h>
#include "commands.h"
#include <glib-unix.h>
#include "handlers.h"
#include "agh.h"
#include "xmpp.h"
#include "modem.h"
#include "aghservices.h"

gint main(void) {

	struct agh_state *mstate;

	mstate = agh_state_setup();

	mstate->agh_handlers = handlers_setup();

	/* handlers are registered from within the function called here. */
	agh_core_handlers_setup_ext(mstate);

	agh_sources_setup(mstate);

	agh_threads_setup(mstate);

	/* This will need to be done in a better way. Threads are registered within the function called here. */
	agh_thread_setup_ext(mstate);

	agh_threads_prepare(mstate);

	agh_threads_start(mstate);

	g_print("AGH CORE: Entering main loop...\n");

	g_main_loop_run(mstate->agh_mainloop);

	g_print("AGH CORE: Main loop exiting.\n");

	process_signals(mstate);

	agh_threads_stop(mstate);
	agh_threads_deinit(mstate);
	agh_threads_teardown(mstate);

	agh_sources_teardown(mstate);
	handlers_finalize(mstate->agh_handlers);
	handlers_teardown(mstate->agh_handlers);
	agh_state_teardown(mstate);
	return 0;
}

struct agh_state * agh_state_setup(void) {
	struct agh_state *mstate;

	/* Note: GLib will terminate the application should this memory allocation fail. */
	mstate = g_malloc0(sizeof *mstate);

	/* Set up the main event loop */
	mstate->ctx = g_main_context_new();
	mstate->agh_mainloop = g_main_loop_new(mstate->ctx, FALSE);

	return mstate;
}

void agh_sources_setup(struct agh_state *mstate) {
	/* Intercepts SIGINT UNIX signal. This is useful at least to exit the main loop gracefully (on ctrl+c press) */
	mstate->agh_main_unix_signals = g_unix_signal_source_new(SIGINT);
	g_source_set_callback(mstate->agh_main_unix_signals, agh_unix_signals_cb_dispatch, mstate, NULL);
	mstate->agh_main_unix_signals_tag = g_source_attach(mstate->agh_main_unix_signals, mstate->ctx);

	/* Communications with other threads */
	aghservices_core_messaging_setup(mstate);

	handlers_init(mstate->agh_handlers, mstate);

	return;
}

void agh_sources_teardown(struct agh_state *mstate) {
	/* UNIX SIGINT signal source */
	g_source_destroy(mstate->agh_main_unix_signals);
	mstate->agh_main_unix_signals_tag = 0;
	mstate->agh_main_unix_signals = NULL;
	g_print("AGH CORE: SIGINT will not be handled from now on.\n");
	aghservices_core_messaging_teardown(mstate);
	return;
}

void agh_state_teardown(struct agh_state *mstate) {
	// XXX is this the proper order?
	g_main_loop_unref(mstate->agh_mainloop);
	g_main_context_unref(mstate->ctx);
	mstate->agh_mainloop = NULL;
	mstate->ctx = NULL;
	g_free(mstate);
	mstate = NULL;
	return;
}

void process_signals(struct agh_state *mstate) {
	if (mstate->sigint_received)
		g_print("\nSIGINT!\n");
}

void agh_threads_start(struct agh_state *mstate) {
	g_print("AGH CORE: starting threads: \n");
	g_queue_foreach(mstate->agh_threads, agh_threads_start_single, mstate);
	g_print("done\n");
}

void agh_thread_register(struct agh_state *mstate, struct agh_thread *ct) {
	if (!ct) {
		g_print("AGH CORE: ** INVALID THREAD REGISTRATION CALL: NULL VALUE NOT ACCEPTABLE AS A THREAD.\n");
		return;
	}

	if (!mstate->agh_threads) {
		g_print("AGH CORE: ** INVALID THREAD REGISTRATION CALL: THREADS QUEUE NOT INITIALIZED.\n");
		return;
	}
	
	g_print("AGH CORE: registering %s thread: ",ct->thread_name);
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
	g_print("AGH CORE: deallocating threads queue.\n");
	g_queue_free(mstate->agh_threads);
	mstate->agh_threads = NULL;
	return;
}

void agh_threads_prepare(struct agh_state *mstate) {
	g_print("AGH CORE: preparing threads \n");

	g_queue_foreach(mstate->agh_threads, agh_threads_prepare_single, mstate);
	g_print(" done\n");
	return;
}

void agh_threads_deinit(struct agh_state *mstate) {
	g_print("AGH CORE: invoking threads deinit functions ");
	g_queue_foreach(mstate->agh_threads, agh_threads_deinit_single, mstate);
	g_print("done\n");
	return;
}

void agh_threads_prepare_single(gpointer data, gpointer user_data) {
	struct agh_thread *ct = data;
	struct agh_state *mstate = user_data;

	g_print("%s ",ct->thread_name);

	/* Before starting our thread, provide basic facilities: the communication asynchronous queue and the main thread's GMainContext. */
	ct->agh_maincontext = mstate->ctx;
	ct->agh_mainloop = mstate->agh_mainloop;
	ct->agh_comm = mstate->agh_comm;

	ct->comm = g_async_queue_new();
	ct->handlers = NULL;

	/* We would like to be informed about this situation: and if someone finds this useful we are going to remove this message
	* and think about what's going on and what's needed. I don't think there's a valid reason for doing this, but who knows.
	*/
	if (ct->agh_thread_init)
		ct->agh_thread_init(ct);

	return;
}

void agh_threads_deinit_single(gpointer data, gpointer user_data) {
	struct agh_thread *ct = data;
	struct agh_state *mstate = user_data;

	g_print(ct->thread_name);
	if (ct->agh_thread_deinit)
		ct->agh_thread_deinit(ct);

	if (ct->handlers) {
		g_print("WARNING: %s thread may not be deinitializing its handlers correctly. This needs to be investigated.\n", ct->thread_name);
	}
	g_async_queue_unref(ct->comm);
	ct->agh_maincontext = NULL;
	ct->agh_mainloop = NULL;
	ct->comm = NULL;

	/* Due to the usage of on-stack structures, if the current thread is not using GLib main loop, I guess we're going to access uninitialized memory. This should be fixed. For now, just let the code remind us something is not right. */
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

	g_print(ct->thread_name);

	if (ct->agh_thread_main)
		ct->current_thread = g_thread_new(ct->thread_name, ct->agh_thread_main, ct);
	else
		g_print("AGH CORE: so you found useful to create a thread that actually can't start (NULL main function). Contact us to discuss this, if you intend to leave your code this way, since we will need to do some changes for this to work as you expect, I guess.\n");

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
gboolean agh_unix_signals_cb_dispatch(gpointer data) {
	struct agh_state *mstate = data;
	mstate->sigint_received = TRUE;
	g_main_loop_quit(mstate->agh_mainloop);
	return FALSE;
}

/*
 * This handler is meant to:
 * - receive text messages from components willing to send them
 * - build a command using the appropriate functions in commands.c
 * - if building a command succeeds, then build and send a message containing it
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
			/* Send this message around to all registered components. */
			g_print("AGH: processing CMD.\n");

			num_threads = g_queue_get_length(mstate->agh_threads);
			if (num_threads) {

				for (i=0;i<num_threads;i++) {
					command_message = msg_alloc();
					command_message->msg_type = MSG_SENDCMD;
					ct = g_queue_peek_nth(mstate->agh_threads, i);
					g_print("Sending command message to %s thread.\n",ct->thread_name);
					if (msg_prepare(command_message, mstate->agh_comm, ct->comm)) {
						g_print("AGH CORE: error while preparing message for sending to %s thread.\n",ct->thread_name);
						msg_dealloc(command_message);
						continue;
					}

					command_message->csp = cmd_copy(cmd);
					msg_send(command_message);

				} /* end of for loop */

			} /* num_threads not zero */

			/*
			 * Send command messages to the core itself.
			*/
			command_message = msg_alloc();
			command_message->msg_type = MSG_SENDCMD;
			if (msg_prepare(command_message, mstate->agh_comm, mstate->agh_comm)) {
				g_print("AGH CORE: error while preparing message for sending to core.\n");
				msg_dealloc(command_message);
			}
			else {
				command_message->csp = cmd;
				msg_send(command_message);
			}

		} /* valid command was received */

	}

	return NULL;
}

/*
 * This handler receives MSG_SENDTEXT text messages from sources willing to send them, and broadcast them to all components.
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
		g_print("AGH: broadcasting text.\n");

		num_threads = g_queue_get_length(mstate->agh_threads);
		if (num_threads) {

			for (i=0;i<num_threads;i++) {
				text_message = msg_alloc();
				text_message->msg_type = MSG_SENDTEXT;
				ct = g_queue_peek_nth(mstate->agh_threads, i);
				g_print("Sending text message to %s thread.\n",ct->thread_name);
				if (msg_prepare(text_message, mstate->agh_comm, ct->comm)) {
					g_print("AGH CORE: error while preparing text message for sending to %s thread.\n",ct->thread_name);
					msg_dealloc(text_message);
					continue;
				}

				ncsp = g_malloc0(sizeof(struct text_csp));
				ncsp->text = g_strdup(csp->text);
				text_message->csp = ncsp;
				msg_send(text_message);

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

	if (!g_strcmp0(cmd_get_operation(cmd), "quit")) {
		g_main_loop_quit(mstate->agh_mainloop);
	}

	return NULL;
}

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

	/* An event arrived - so we need to assign it an event ID. */
	cmd = cmd_copy(m->csp);
	evtext = cmd_event_to_text(cmd, mstate->event_id);
	mstate->event_id++;
	if (mstate->event_id == CMD_EVENT_MAX_ID)
		mstate->event_id = 0;

	if (!evtext) {
		cmd_free(cmd);
		return evmsg;
	}

	evmsg = msg_alloc();

	textcsp = g_malloc0(sizeof(struct text_csp));
	textcsp->text = evtext;
	evmsg->csp = textcsp;
	evmsg->msg_type = MSG_SENDTEXT;

	if (msg_prepare(evmsg, mstate->agh_comm, mstate->agh_comm)) {
		msg_dealloc(evmsg);
		evmsg = NULL;
	}

	cmd_free(cmd);
	return evmsg;
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
	g_print("AGH CORE: broadcasting event.\n");

	num_threads = g_queue_get_length(mstate->agh_threads);
	if (num_threads) {

		for (i=0;i<num_threads;i++) {
			evmsg = msg_alloc();
			evmsg->msg_type = MSG_EVENT;
			ct = g_queue_peek_nth(mstate->agh_threads, i);
			g_print("Sending event message to %s thread.\n",ct->thread_name);
			if (msg_prepare(evmsg, mstate->agh_comm, ct->comm)) {
				g_print("AGH CORE: error while preparing event message for sending to %s thread.\n",ct->thread_name);
				msg_dealloc(evmsg);
				continue;
			}

			ncmd = cmd_copy(cmd);

			/* ncmd may well be NULL here */
			evmsg->csp = ncmd;
			msg_send(evmsg);

		} /* end of for loop */

	} /* num_threads not zero */

	return NULL;
}

struct agh_thread *agh_thread_new(gchar *name) {
	struct agh_thread *ct;

	ct = NULL;

	if (!name) {
		g_print("AGH CORE: a thread may not have a NULL name.\n");
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

/*
 * This function exists solely because I don't know a better way to do this.
*/
void agh_thread_setup_ext(struct agh_state *mstate) {
	struct agh_thread *xmpp_thread;
	struct agh_thread *modem_thread;

	xmpp_thread = NULL;
	modem_thread = NULL;

	/* XMPP */
	xmpp_thread = agh_thread_new("XMPP");
	agh_thread_set_init(xmpp_thread, xmpp_thread_init);
	agh_thread_set_main(xmpp_thread, xmpp_thread_start);
	agh_thread_set_deinit(xmpp_thread, xmpp_thread_deinit);

	/* Modem */
	modem_thread = agh_thread_new("Modem");
	agh_thread_set_init(modem_thread, modem_thread_init);
	agh_thread_set_main(modem_thread, modem_thread_start);
	agh_thread_set_deinit(modem_thread, modem_thread_deinit);

	agh_thread_register(mstate, xmpp_thread);
	agh_thread_register(mstate, modem_thread);
	return;
}

void agh_core_handlers_setup_ext(struct agh_state *mstate) {
	/* Core handlers. */
	struct handler *core_recvtextcommand_handler;
	struct handler *core_sendtext_handler;
	struct handler *core_cmd_handler;
	struct handler *core_event_to_text_handler;
	struct handler *core_event_broadcast_handler;

	core_recvtextcommand_handler = NULL;
	core_cmd_handler = NULL;
	core_sendtext_handler = NULL;
	core_event_to_text_handler = NULL;
	core_event_broadcast_handler = NULL;

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

	handler_register(mstate->agh_handlers, core_recvtextcommand_handler);
	handler_register(mstate->agh_handlers, core_sendtext_handler);
	handler_register(mstate->agh_handlers, core_cmd_handler);
	handler_register(mstate->agh_handlers, core_event_to_text_handler);
	handler_register(mstate->agh_handlers, core_event_broadcast_handler);

	return;
}
