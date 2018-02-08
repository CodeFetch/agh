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
	ct->comm_timeout = g_timeout_source_new_seconds(2);
	g_source_set_callback(ct->comm_timeout, aghservices_receive_messages, ct, NULL);
	ct->comm_timeout_tag = g_source_attach(ct->comm_timeout, ct->evl_ctx);

	handlers_init(ct->handlers);

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
			aghservices_handle_message(ct->handlers, m);
			msg_dealloc(m);
		}

		g_queue_free(handlers_answers);

		if (!g_queue_get_length(handlers_answers)) {
			g_print("OK, remove this.\n");
		}
	}

	return TRUE;
}

void aghservices_handle_message(GQueue *handlers, struct agh_message *m) {
	guint num_handlers;
	GQueue *answers;
	struct handler *h;
	struct agh_message *answer;
	guint i;

	num_handlers = 0;
	if (handlers)
		num_handlers = g_queue_get_length(handlers);

	if (num_handlers) {
		answers = g_queue_new();
		for (i=0;i<num_handlers;i++) {
			h = g_queue_peek_nth(handlers, i);
			answer = h->handle(h, m);
			g_queue_push_tail(answers, answer);
	// and then?
		}
		g_queue_free(answers);
	}

	return;
}
