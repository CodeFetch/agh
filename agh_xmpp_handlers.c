#include "agh_handlers.h"
#include "agh_xmpp.h"
#include "agh_xmpp_handlers.h"
#include "agh_messages.h"
#include "agh_commands.h"

struct agh_message *xmpp_sendmsg_handle(struct agh_handler *h, struct agh_message *m) {
	struct agh_text_payload *csp;
	struct agh_state *mstate;
	struct xmpp_state *xstate;
	struct agh_text_payload *textcopy_csp;
	struct agh_message *omsg;

	mstate = h->handler_data;
	xstate = mstate->xstate;
	textcopy_csp = NULL;
	omsg = NULL;

	csp = m->csp;

	if (!xstate->outxmpp_messages)
		return NULL;

	if (m->msg_type == MSG_SENDTEXT) {
		if (g_queue_get_length(xstate->outxmpp_messages) > AGH_XMPP_MAX_OUTGOING_QUEUED_MESSAGES) {
			g_queue_foreach(xstate->outxmpp_messages, discard_xmpp_messages, xstate);
			return NULL;
		}

		omsg = agh_msg_alloc();
		if (!omsg)
			return NULL;

		omsg->msg_type = MSG_SENDTEXT;
		textcopy_csp = g_try_malloc0(sizeof(*textcopy_csp));
		if (!textcopy_csp) {
			agh_msg_dealloc(omsg);
			return NULL;
		}

		textcopy_csp->text = g_strdup(csp->text);

		if (csp->source_id)
			textcopy_csp->source_id = g_strdup(csp->source_id);

		omsg->csp = textcopy_csp;
		g_queue_push_tail(xstate->outxmpp_messages, omsg);
	}

	return NULL;
}

struct agh_message *xmpp_cmd_handle(struct agh_handler *h, struct agh_message *m) {
	struct agh_cmd __attribute__((unused)) *cmd;
	struct agh_state *mstate;
	struct xmpp_state __attribute__((unused)) *xstate;

	mstate = h->handler_data;
	xstate = mstate->xstate;

	cmd = m->csp;

	if (m->msg_type != MSG_SENDCMD)
		return NULL;

	/* to be implemented; and remember to check if we are connected... */
	return NULL;
}
