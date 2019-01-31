#ifndef __agh_commands_h__
#include <glib.h>
#define __agh_commands_h__
#include <libconfig.h>
#include "agh_messages.h"

/* Status codes. */
#define CMD_ANSWER_STATUS_UNKNOWN 380
#define CMD_ANSWER_STATUS_OK 200
#define CMD_ANSWER_STATUS_FAIL 400
/* End of status codes. */

/* Unset event IDs. */
#define CMD_EVENT_UNKNOWN_ID CMD_ANSWER_STATUS_UNKNOWN
#define CMD_EVENT_MAX_ID CMD_EVENT_UNKNOWN_ID
/* End of unset event IDs. */

/* Unset answer text or event name. */
#define BUG_EMPTY_ANSWER_TEXT "BUG_EMPTY_ANSWER_TEXT"
#define BUG_EMPTY_EVENT_NAME "BUG_EMPTY_EVENT_NAME"

struct command_result {
	gboolean is_data;
	guint status;
	GQueue *restextparts;
};

struct command {
	config_t *cmd;
	struct command_result *answer;
	gchar *cmd_source_id;
};

struct command *text_to_cmd(gchar *from, gchar *content);

/* command results */

void cmd_answer_set_status(struct command *cmd, guint status);
void cmd_answer_set_data(struct command *cmd, gboolean is_data);
void cmd_answer_if_empty(struct command *cmd, guint status, gchar *text, gboolean set_is_data);
guint cmd_answer_get_status(struct command *cmd);
guint cmd_answer_addtext(struct command *cmd, const gchar *text);
guint cmd_answer_peektext(struct command *cmd, gchar *text);
guint cmd_answer_prepare(struct command *cmd);

/* assorted management functions */
gint agh_cmd_free(struct command *cmd);
struct command *cmd_copy(struct command *cmd);
struct agh_message *cmd_answer_msg(struct command *cmd, struct agh_comm *src_comm, struct agh_comm *dest_comm);

/* Some useful functions to access commands data.
 *
 * note: when one of these functions return a pointer to a string, it is of const type. This is due to the fact that libconfig
 * itself manages their storage. Don't try to modify them in any case.
*/
const gchar *cmd_get_operation(struct command *cmd);
config_setting_t *cmd_get_arg(struct command *cmd, guint arg_index, gint config_type);

/* events */
struct command *cmd_event_prepare(void);
gchar *cmd_event_to_text(struct command *cmd, gint event_id);
void cmd_emit_event(struct agh_comm *agh_core_comm, struct command *cmd);

#endif
