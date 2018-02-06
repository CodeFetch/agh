/* Some of these functions are called from within treads, some aren't. */
#include "agh.h"
#include "aghservices.h"
#include "messages.h"

void aghservices_messaging_setup(struct agh_thread *ct) {
	/* Sets up a new Main Loop Context, and Main Loop of course, for the new thread. */
	ct->evl_ctx = g_main_context_new();
	ct->evl = g_main_loop_new(ct->evl_ctx, FALSE);

	/* New timeout source to check for messages periodically. */
	ct->comm_timeout = g_timeout_source_new_seconds(2);
	g_source_set_callback(ct->comm_timeout, aghservices_get_message, ct, NULL);
	ct->comm_timeout_tag = g_source_attach(ct->comm_timeout, ct->evl_ctx);

	return;
}

gboolean aghservices_get_message(gpointer data) {
	struct agh_thread *ct = data;
	struct agh_message *m;

	m = g_async_queue_try_pop(ct->comm);

	if (!m) {
		g_print("%s: no messages received yet.\n",ct->thread_name);
	}
	else
	{
		g_print("%s: YO YO! Message received\n",ct->thread_name);
		msg_dealloc(m);
	}

	return TRUE;
}
