/* Some of these functions are called from within treads, some aren't. */
#include "agh.h"
#include "aghservices.h"
#include "messages.h"
#include "handlers.h"

void aghservices_messaging_setup(struct agh_thread *ct, gboolean no_context) {

	if (!ct->handlers) {
		g_print("AGH CORE: (%s) called us with a NULL handlers queue pointer. Messaging setup not happening for this thread to prevent an immediate segfault, but things will probably not work correctly.\n\t(maybe you forgot to call handlers_setup ? )\n",ct->thread_name);
		return;
	}

	/* Sets up a new Main Loop Context (and related Main Loop of course) for the calling thread. */
	if (!no_context)
		ct->evl_ctx = g_main_context_new();

	ct->evl = g_main_loop_new(ct->evl_ctx, FALSE);

	ct->comm_timeout = g_timeout_source_new_seconds(2);
	g_source_set_callback(ct->comm_timeout, aghservices_receive_messages, ct, NULL);
	ct->comm_timeout_tag = g_source_attach(ct->comm_timeout, ct->evl_ctx);

	return;
}

void aghservices_core_messaging_setup(struct agh_state *mstate) {

	if (!mstate->agh_handlers) {
		g_print("AGH CORE: handlers queue not allocated. Something should be going wrong. CORE will not receive messages.\n");
		return;
	}

	mstate->agh_comm = g_async_queue_new();
	mstate->comm_timeout = g_timeout_source_new_seconds(2);
	g_source_set_callback(mstate->comm_timeout, aghservices_core_receive_messages, mstate, NULL);
	mstate->comm_timeout_tag = g_source_attach(mstate->comm_timeout, mstate->ctx);

	return;
}

gboolean aghservices_receive_messages(gpointer data) {
	struct agh_thread *ct = data;

	aghservices_common_receive_messages(ct->comm, ct->handlers);

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
		g_print("handlers: WARNING - a message has been received, but no handlers queue is allocated. This may be a problem.\n");
	}

	if (num_handlers) {
		answers = g_queue_new();
		for (i=0;i<num_handlers;i++) {
			h = g_queue_peek_nth(handlers, i);
			if (h->enabled) {
				//g_print("handle(%s)\n",h->name);
				answer = h->handle(h, m);
				if (answer) {
					msg_prepare(answer, src_comm, m->src_comm);
					g_queue_push_tail(answers, answer);
				}
			}

		}

		/* queue back messages to sender */
		if (!g_queue_get_length(answers)) {
			//g_print("No answers from handlers, not responding.\n");
		}
		else {
			g_async_queue_push(m->src_comm, answers);
		}

	}

	return;
}

gboolean aghservices_core_receive_messages(gpointer data) {
	struct agh_state *mstate = data;

	aghservices_common_receive_messages(mstate->agh_comm, mstate->agh_handlers);

	return TRUE;
}

void aghservices_common_receive_messages(GAsyncQueue *comm, GQueue *handlers) {
	GQueue *handlers_answers;
	guint num_answers;
	guint i;
	struct agh_message *m;

	handlers_answers = g_async_queue_try_pop(comm);

	if (handlers_answers) {
		num_answers = g_queue_get_length(handlers_answers);
		for (i=num_answers;i>0;i--) {
			m = g_queue_pop_head(handlers_answers);
			aghservices_handle_message(handlers, m, comm);
			//g_print("Deallocating message...\n");
			msg_dealloc(m);
		}

		if (g_queue_get_length(handlers_answers)) {
			g_print("AGH messages: did not process all messages. This is going to be a problem. We are leaking memory now.\n");
		}

		//g_print("Deallocating queue.\n");
		g_queue_free(handlers_answers);

	}

	return;
}

void aghservices_messaging_teardown(struct agh_thread *ct) {
	/* XXX is this the right order? */
	g_main_loop_unref(ct->evl);
	g_main_context_unref(ct->evl_ctx);
	ct->evl = NULL;
	ct->evl_ctx = NULL;

	g_source_destroy(ct->comm_timeout);
	ct->comm_timeout_tag = 0;
	ct->comm_timeout = NULL;

	return;
}

void aghservices_core_messaging_teardown(struct agh_state *mstate) {
	g_async_queue_unref(mstate->agh_comm);
	g_source_destroy(mstate->comm_timeout);
	mstate->comm_timeout = NULL;
	mstate->comm_timeout_tag = 0;
	return;
}
