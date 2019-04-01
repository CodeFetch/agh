#include "agh_handlers.h"
#include "agh_messages.h"
#include "agh_commands.h"
#include "agh_modem.h"
#include "agh_mm_helpers.h"
#include "agh_mm_handlers.h"

struct agh_message *agh_mm_cmd_handle(struct agh_handler *h, struct agh_message *m) {
	struct agh_state *mstate = h->handler_data;
	struct agh_message *answer;
	struct agh_mm_state *mmstate;

	answer = NULL;

	if (!mstate || !mstate->mmstate || !mstate->mmstate->manager)
		return answer;

	mmstate = mstate->mmstate;

	return answer;
}
