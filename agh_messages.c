// SPDX-License-Identifier: GPL-2.0-or-later

#include <string.h>
#include <glib.h>
#include "agh_messages.h"
#include "agh.h"
#include "agh_commands.h"
#include "agh_xmpp.h"
#include "agh_logging.h"

/* Log messages from comm domain. */
#define AGH_LOG_DOMAIN_COMM	"COMM"
#define agh_log_comm_dbg(message, ...) agh_log_dbg(AGH_LOG_DOMAIN_COMM, message, ##__VA_ARGS__)
#define agh_log_comm_crit(message, ...) agh_log_crit(AGH_LOG_DOMAIN_COMM, message, ##__VA_ARGS__)

/* Function prototypes. */
static gboolean agh_handle_message_inside_dest_thread(gpointer data);

/* Convenience function to allocate an AGH message. Simply calls g_try_malloc0.
 *
 * Returns: NULL is returned in case of allocation failure.
*/
struct agh_message *agh_msg_alloc(void) {
	struct agh_message *m;

	m = g_try_malloc0(sizeof(struct agh_message));

	if (!m)
		agh_log_comm_crit("AGH message allocation failure");

	return m;
}

/*
 * Deallocates an AGH message, and it's CSP, when of a known type.
 *
 * Returns: 0 on success (even when message's CSP was NULL).
 * -1 if the message was NULL
 * -2 if message's message type was set to MSG_INVALID (0)
 * -3 when the message had an unknown type.
 * Bigger values (in absolute sense, like -10), are returned from agh_cmd_free.
 *
 * When -2 or -3 are returned, a memory leak may have happened.
*/
gint agh_msg_dealloc(struct agh_message *m) {
	struct agh_text_payload *csptext;
	struct agh_cmd *cmd;
	struct xmpp_csp *xmppdata;
	gint retval;

	retval = 0;

	if (!m) {
		agh_log_comm_dbg("not deallocating a NULL AGH message");
		retval--;
		return retval;
	}

	if (m->csp) {
		switch(m->msg_type) {
		case MSG_INVALID:
			agh_log_comm_crit("type %" G_GUINT16_FORMAT" (invalid!) message", m->msg_type);
			retval = -2;
			break;
		case MSG_EXIT:
			break;
		case MSG_RECVTEXT:
		case MSG_SENDTEXT:
			csptext = m->csp;

			if (!csptext->text)
				agh_log_comm_crit("received a MSG_{SENDTEXT,RECVTEXT} message with NULL text");

			g_free(csptext->text);
			g_free(csptext->source_id);
			g_free(csptext);
			break;
		case MSG_SENDCMD:
		case MSG_EVENT:
			cmd = m->csp;
			retval = agh_cmd_free(cmd);
			break;
		case MSG_XMPPTEXT:
			xmppdata = m->csp;
			agh_xmpp_free_csp(xmppdata);
			break;
		default:
			agh_log_comm_crit("message with unknown CSP type (%" G_GUINT16_FORMAT")", m->msg_type);
			retval = -3;
			break;
		}
	}

	g_free(m);
	return retval;
}

/*
 * Sends a message to this or another thread, so it can be processed by currently installed handlers.
 * If dest_comm is NULL, src_comm will be used as destination as well.
 *
 * Returns: 0 on success, or
 *  - 1 when a NULL message or source COMM is passed in
 *  - 2 when both source and destination COMMs where NULL
 *  - 3 when a teardown is in progress (e.g.: AGH is terminating).
 *
 * Negative integer values are directly returned fro agh_msg_dealloc, which in turn may return errors from agh_cmd_free.
 * When teardown is in progress, the message is deallocated.
*/
gint agh_msg_send(struct agh_message *m, struct agh_comm *src_comm, struct agh_comm *dest_comm) {
	gint retval;

	retval = 0;

	if (!m || !src_comm) {
		agh_log_comm_crit("can not send a NULL message or use a NULL COMM");
		retval++;
		return retval;
	}

	if ((!dest_comm) && (!src_comm)) {
		agh_log_comm_crit("sender and recipient COMMs where NULL");
		retval = 2;
		return retval;
	}

	if (!dest_comm)
		dest_comm = src_comm;

	if (dest_comm->teardown_in_progress) {
		agh_log_comm_dbg("not sending message to %s due to teardown being in progress",dest_comm->name);
		retval = agh_msg_dealloc(m);

		if (!retval)
			retval = 3;

		return retval;
	}

	m->src = src_comm;
	m->dest = dest_comm;

	g_main_context_invoke(m->dest->ctx, agh_handle_message_inside_dest_thread, m);

	return retval;
}

/*
 * Dispatches a message inside an AGH thread, or the core itself.
 *
 * This function is called via a GLib callback, so it returns FALSE in order to be invoked only once per GMainContext iteration.
*/
static gboolean agh_handle_message_inside_dest_thread(gpointer data) {
	struct agh_message *m = data;
	guint num_handlers;
	struct agh_handler *h;
	struct agh_message *answer;
	guint i;
	GQueue *handlers;

	if (!m) {
		agh_log_comm_crit("NULL message received, no processing will take place");
		return FALSE;
	}

	handlers = m->dest->handlers;

	if (m->dest->teardown_in_progress) {
		agh_log_comm_crit("deallocating message in %s thread due to teardown being in progress",m->dest->name);

		agh_msg_dealloc(m);

		return FALSE;
	}

	if (handlers)
		num_handlers = g_queue_get_length(handlers);
	else {
		agh_log_comm_crit("a message has been received in %s, but no handlers queue is allocated",m->dest->name ? m->dest->name : "(no name)");
		agh_msg_dealloc(m);
		return FALSE;
	}

	for (i=0;i<num_handlers;i++) {
		h = g_queue_peek_nth(handlers, i);

		if (h->enabled) {
			answer = h->handle(h, m);
			if (answer) {
				answer->src = m->dest;
				answer->dest = m->src;

				if (!answer->dest->teardown_in_progress)
					agh_msg_send(answer, answer->src, answer->dest);

			}
		}
	}

	agh_msg_dealloc(m);

	return FALSE;
}

/*
 * Sets up an AGH COMM data structure, historically used to communicate with an AGH thread, now by the core to exchange messages with itself.
 *
 * Returns: NULL on failure (e.g.: memory allocation), a COMM data structure on success.
*/
struct agh_comm *agh_comm_setup(GQueue *handlers, GMainContext *ctx, gchar *name) {
	struct agh_comm *comm;

	comm = NULL;

	if (!name) {
		agh_log_comm_crit("NULL COMM name specified");
		return comm;
	}

	if (!ctx) {
		agh_log_comm_crit("NULL GMainContext");
		return comm;
	}

	comm = g_try_malloc0(sizeof(*comm));
	if (!comm) {
		agh_log_comm_crit("out of memory while allocating COMM data structure");
		return comm;
	}

	comm->name = name;
	comm->handlers = handlers;
	comm->ctx = ctx;

	return comm;
}

/*
 * As it's name suggest, this function is used when closing down a COMM.
 *
 * Returns: an integer with value -1 when a NULL COMM is passed, 0 on success.
*/
gint agh_comm_teardown(struct agh_comm *comm, gboolean do_not_iterate_gmaincontext) {
	guint i;

	i = AGH_MAX_MESSAGEWAIT_ITERATIONS;

	if (!comm) {
		agh_log_comm_crit("NULL COMM passed in for teardown");
		return -1;
	}

	comm->teardown_in_progress = TRUE;

	if (!do_not_iterate_gmaincontext)
		do {
			if (g_main_context_iteration(comm->ctx, FALSE))
				i = AGH_MAX_MESSAGEWAIT_ITERATIONS;

			i--;
		} while (i);

	comm->name = NULL;
	comm->handlers = NULL;
	comm->ctx = NULL;
	g_free(comm);

	return 0;
}

/*
 * Sets a COMM teardown state. This is meant to "prepare" a COMM for being deallocated, when AGH is terminating.
 *
 * Returns: an integer with value 0 on success, or value -1 when a NULL COMM is passed.
*/
gint agh_comm_set_teardown_state(struct agh_comm *comm, gboolean enabled) {
	gint retval;

	retval = 0;

	if (!comm) {
		agh_log_comm_crit("tried to change the teardown state of a NULL COMM");
		retval = -1;
	}
	else
		comm->teardown_in_progress = enabled;

	return retval;
}

/*
 * Parse a message source ID via strtok_r, returning to the caller two strings, holding the source name, and the "content".
 * Imagine something like "XMPP=myaccountname@myservername.com". this function should split it into:
 * - "XMPP": source name
 * - "myaccountname@myservername.com": content.
 *
 * The pointers pointed bysourcename and source_content may be NULL at function termination.
 * Strings returned by this function are expected to be freed by the caller.
 *
 * Parameters:
 * - source_id: a string holding the full source ID ("XMPP=myaccountname@myservername.com");
 * - source_name: a pointer to a pointer, where the source name will be returned;
 *   (the pointer to which source_name points is expected to be NULL)
 * - source_content: a pointer to a pointer, where the source content will be returned;
 *   (the pointer to which source_id points is expected to be NULL)
 *
 * Returns: an integer with values:
 * - 1: source_id was NULL, or the pointer pointed by source_name was not NULL, or the pointer pointed by source_content was not NULL;
*/
gint agh_message_source(gchar *source_id, gchar **source_name, gchar **source_content) {
	gchar *saveptr;
	gchar *local_source_id;
	gchar *local_source_name;
	gchar *local_source_content;

	saveptr = NULL;

	if ((!source_id) || (*source_name) || (*source_content))
		return 1;

	local_source_id = g_strdup(source_id);

	local_source_name = strtok_r(local_source_id, "=", &saveptr);

	local_source_content = strtok_r(NULL, "=", &saveptr);

	if (local_source_content && local_source_name) {
		*source_name = g_strdup(local_source_name);
		*source_content = g_strdup(local_source_content);
	}

	g_free(local_source_id);

	return 0;
}
