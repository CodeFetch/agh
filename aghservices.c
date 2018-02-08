/* Some of these functions are called from within treads, some aren't. */
#include "agh.h"
#include "aghservices.h"
#include "messages.h"
#include "callbacks.h"

void aghservices_messaging_setup(struct agh_thread *ct, GQueue *handlers) {

	if (!handlers) {
		g_print("AGH CORE: a thread (%s) passed us an NULL handlers queue pointer. Messaging disabled for that thread to prevent an immediate segfault, but things will probably not work correctly.\n",ct->thread_name);
		return;
	}

	/* Sets up a new Main Loop Context, and related Main Loop of course, for the new thread. */
	ct->evl_ctx = g_main_context_new();
	ct->evl = g_main_loop_new(ct->evl_ctx, FALSE);

	/* New timeout source to check for messages periodically. */
	ct->comm_timeout = g_timeout_source_new_seconds(2); // ????????
	g_source_set_callback(ct->comm_timeout, aghservices_receive_messages, ct, NULL);
	ct->comm_timeout_tag = g_source_attach(ct->comm_timeout, ct->evl_ctx);

	handlers_init(ct->handlers);

	return;
}

void aghservices_core_messaging_setup(struct agh_state *mstate) {

	if (!mstate->agh_handlers) {
		g_print("AGH CORE: the core seems to have no handlers. Something should be going horribly wrong.\n");
		return;
	}

	/* New timeout source to check for messages periodically. */
	mstate->comm_timeout = g_timeout_source_new_seconds(2); // ????????
	g_source_set_callback(mstate->comm_timeout, aghservices_core_receive_messages, mstate, NULL);
	mstate->comm_timeout_tag = g_source_attach(mstate->comm_timeout, mstate->ctx);

	handlers_init(mstate->agh_handlers);

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