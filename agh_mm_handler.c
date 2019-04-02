#include "agh_handlers.h"
#include "agh_messages.h"
#include "agh_commands.h"
#include "agh_modem.h"
#include "agh_mm_helpers.h"
#include "agh_mm_handler.h"

static const struct agh_cmd_operation agh_mm_handler_ops[] = {
	{
		.op_name = "modem",
		.min_args = 0,
		.max_args = 1,
		.cmd_cb = NULL
	},

	{ }
};

struct agh_message *agh_mm_cmd_handle(struct agh_handler *h, struct agh_message *m) {
	struct agh_state *mstate = h->handler_data;
	struct agh_message *answer;
	struct agh_cmd *cmd;
	config_setting_t *arg;

	answer = NULL;

	if ((!mstate || !mstate->mmstate || !mstate->mmstate->manager) || (m->msg_type != MSG_SENDCMD))
		return answer;

	cmd = m->csp;

	agh_cmd_op_match(mstate, agh_mm_handler_ops, cmd, 0);

	return answer;
}
