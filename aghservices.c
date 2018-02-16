/* Some of these functions are called from within treads, some aren't. */
#include "agh.h"
#include "aghservices.h"
#include "messages.h"
#include "callbacks.h"

void aghservices_messaging_setup(struct agh_thread *ct) {

	if (!ct->handlers) {
		g_print("AGH CORE: a thread (%s) called us with a NULL handlers queue pointer. Messaging setup not happening for this thread to prevent an immediate segfault, but things will probably not work correctly.\n\t(maybe you forgot to call handlers_setup ? )\n",ct->thread_name);
		return;
	}

	/* Sets up a new Main Loop Context, and related Main Loop of course, for the new thread. */
	ct->evl_ctx = g_main_context_new();
	ct->evl = g_main_loop_new(ct->evl_ctx, FALSE);

	aghservices_common_messaging_setup(ct->comm_timeout, aghservices_receive_messages, ct, &ct->comm_timeout_tag, ct->evl_ctx);

	return;
}

void aghservices_core_messaging_setup(struct agh_state *mstate) {

	if (!mstate->agh_handlers) {
		g_print("AGH CORE: handlers queue not allocated. Something should be going horribly wrong. CORE will not receive messages.\n");
		return;
	}

	aghservices_common_messaging_setup(mstate->comm_timeout, aghservices_core_receive_messages, mstate, &mstate->comm_timeout_tag, mstate->ctx);

	return;
}

void aghservices_common_messaging_setup(GSource *evsrc, GSourceFunc evsrc_callback, gpointer data, guint *tag, GMainContext *ctx) {

	/* New timeout source to check for messages periodically. */
	evsrc = g_timeout_source_new_seconds(2); // ????????
	g_source_set_callback(evsrc, evsrc_callback, data, NULL);
	*tag = g_source_attach(evsrc, ctx);

	return;
}

gboolean aghservices_receive_messages(gpointer data) {
	struct agh_thread *ct = data;
	GQueue *handlers_answers;
	guint num_answers;
	guint i;
	struct agh_message *m;

	handlers_answers = g_async_queue_try_pop(ct->comm);

	if (handlers_answers) {
		num_answers = g_queue_get_length(handlers_answers);
		for (i=num_answers;i>0;i--) {
			m = g_queue_pop_head(handlers_answers);
			aghservices_handle_message(ct->handlers, m, ct->comm);
			g_print("Deallocating message...\n");
			msg_dealloc(m);
		}

		if (g_queue_get_length(handlers_answers)) {
			g_print("AGH messages: not all messages where processed. This is going to be a problem. We are leaking memory now.\n");
		}

		g_print("Deallocating queue.\n");
		g_queue_free(handlers_answers);

	}

	return TRUE;
}

void aghservices_handle_message(GQueue *handlers, struct agh_message *m, GAsyncQueue *src_comm) {
	guint num_handlers;
	GQueue *answers;
	struct handler *h;
	struct agh_message *answer;
	guint i;

	num_handlers = 0;
	if (handlers)
		num_handlers = g_queue_get_length(handlers);
	else {
		g_print("handlers: WARNING - a message has been received, but no handlers are present. This may be a problem.\n");
	}

	if (num_handlers) {
		answers = g_queue_new();
		for (i=0;i<num_handlers;i++) {
			h = g_queue_peek_nth(handlers, i);
			answer = h->handle(h, m);
			if (answer) {
				msg_prepare(answer, src_comm, m->src_comm);
				g_queue_push_tail(answers, answer);
			}
		}

		/* queue back messages to sender */
		if (!g_queue_get_length(answers)) {
			g_print("No answers from handlers, not responding.\n");
		}
		else {
			g_async_queue_push(m->src_comm, answers);
		}
	}

	return;
}

gboolean aghservices_core_receive_messages(gpointer data) {
	struct agh_state *mstate = data;
	GQueue *handlers_answers;
	guint num_answers;
	guint i;
	struct agh_message *m;

	handlers_answers = g_async_queue_try_pop(mstate->agh_comm);

	if (handlers_answers) {
		num_answers = g_queue_get_length(handlers_answers);
		for (i=num_answers;i>0;i--) {
			m = g_queue_pop_head(handlers_answers);
			aghservices_handle_message(mstate->agh_handlers, m, mstate->agh_comm);
			g_print("Deallocating message...\n");
			msg_dealloc(m);
		}

		if (g_queue_get_length(handlers_answers)) {
			g_print("AGH messages: the core did not process all messages. This is going to be a problem. We are leaking memory now.\n");
		}

		g_print("Deallocating queue.\n");
		g_queue_free(handlers_answers);

	}

	return TRUE;
}
