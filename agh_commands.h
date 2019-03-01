#ifndef __agh_commands_h__
#include <glib.h>
#define __agh_commands_h__
#include <libconfig.h>
#include "agh_messages.h"

/* Most of the things you'll find there are used by the core. Things like AGH_CMD_ANSWER_STATUS_{OK,FAIL} are used all around in the code base. */

/* Status codes. */
#define AGH_CMD_ANSWER_STATUS_UNKNOWN 380
#define AGH_CMD_ANSWER_STATUS_OK 200
#define AGH_CMD_ANSWER_STATUS_FAIL 400
/* End of status codes. */

/* Unset event IDs. */
#define AGH_CMD_EVENT_UNKNOWN_ID AGH_CMD_ANSWER_STATUS_UNKNOWN

/* Max event ID. */
#define AGH_CMD_EVENT_MAX_ID AGH_CMD_EVENT_UNKNOWN_ID-1

/* IN keyword: should be used for incoming commands */
#define AGH_CMD_IN_KEYWORD "AT"

/* OUT keyword: for outgoing commands. */
#define AGH_CMD_OUT_KEYWORD "IH"

/* EVENT keyword, for events */
#define AGH_CMD_EVENT_KEYWORD AGH_CMD_OUT_KEYWORD"!"

struct agh_cmd {
	config_t *cmd;
	struct agh_cmd_res *answer;
	gchar *cmd_source_id;
};

/* An operation entry on the table used for matching, checking and executing operations. */
struct agh_cmd_operation {
	const gchar *op_name;
	guint min_args;
	guint max_args;
	gint (*cmd_cb)(struct agh_state *mstate, struct agh_cmd *cmd);
};

struct agh_cmd *agh_text_to_cmd(gchar *from, gchar *content);

/* AGH commands results */
gint agh_cmd_answer_set_status(struct agh_cmd *cmd, guint status);
gint agh_cmd_answer_set_data(struct agh_cmd *cmd, gboolean is_data);
gint agh_cmd_answer_if_empty(struct agh_cmd *cmd, guint status, gchar *text, gboolean is_data);
guint agh_cmd_answer_get_status(struct agh_cmd *cmd);
gint agh_cmd_answer_addtext(struct agh_cmd *cmd, const gchar *text, gboolean dup);
gint agh_cmd_answer_alloc(struct agh_cmd *cmd);
gint agh_cmd_op_answer_error(struct agh_cmd *cmd, guint status, gchar *text, gboolean dup);

/* assorted management functions */
gint agh_cmd_free(struct agh_cmd *cmd);
struct agh_cmd *agh_cmd_copy(struct agh_cmd *cmd);
struct agh_message *agh_cmd_answer_msg(struct agh_cmd *cmd, struct agh_comm *src_comm, struct agh_comm *dest_comm);

/* Some useful functions to access commands data.
 *
 * Note: when one of these functions return a pointer to a string, it is of const type. This is due to the fact that libconfig
 * itself manages their storage.
*/
const gchar *agh_cmd_get_operation(struct agh_cmd *cmd);
config_setting_t *agh_cmd_get_arg(struct agh_cmd *cmd, guint arg_index, gint config_type);
gint agh_cmd_get_id(struct agh_cmd *cmd);

/* events */
struct agh_cmd *agh_cmd_event_alloc(gint *error_value);
gchar *agh_cmd_answer_to_text(struct agh_cmd *cmd, const gchar *keyword, gint event_id);
gint agh_cmd_emit_event(struct agh_comm *agh_core_comm, struct agh_cmd *cmd);
const gchar *agh_cmd_event_arg(struct agh_cmd *cmd, guint arg_index);
const gchar *agh_cmd_event_name(struct agh_cmd *cmd);

/* Operations related functions. */
gint agh_cmd_op_match(struct agh_state *mstate, const struct agh_cmd_operation *ops, struct agh_cmd *cmd, guint index);

#endif
