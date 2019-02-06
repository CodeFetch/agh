#ifndef __agh_commands_h__
#include <glib.h>
#define __agh_commands_h__
#include <libconfig.h>
#include "agh_messages.h"

/* Status codes. */
#define AGH_CMD_ANSWER_STATUS_UNKNOWN 380
#define AGH_CMD_ANSWER_STATUS_OK 200
#define AGH_CMD_ANSWER_STATUS_FAIL 400
/* End of status codes. */

/* Unset event IDs. */
#define AGH_CMD_EVENT_UNKNOWN_ID AGH_CMD_ANSWER_STATUS_UNKNOWN
#define AGH_CMD_EVENT_MAX_ID AGH_CMD_EVENT_UNKNOWN_ID-1
/* End of unset event IDs. */

struct agh_cmd {
	config_t *cmd;
	struct agh_cmd_res *answer;
	gchar *cmd_source_id;
};

struct agh_cmd *agh_text_to_cmd(gchar *from, gchar *content);

/* AGH commands results */
gint agh_cmd_answer_set_status(struct agh_cmd *cmd, guint status);
void cmd_answer_set_data(struct agh_cmd *cmd, gboolean is_data);
void cmd_answer_if_empty(struct agh_cmd *cmd, guint status, gchar *text, gboolean set_is_data);
guint agh_cmd_answer_get_status(struct agh_cmd *cmd);
guint agh_cmd_answer_addtext(struct agh_cmd *cmd, const gchar *text, gboolean dup);
guint cmd_answer_prepare(struct agh_cmd *cmd);

/* assorted management functions */
gint agh_cmd_free(struct agh_cmd *cmd);
struct agh_cmd *agh_cmd_copy(struct agh_cmd *cmd);
struct agh_message *cmd_answer_msg(struct agh_cmd *cmd, struct agh_comm *src_comm, struct agh_comm *dest_comm);

/* Some useful functions to access commands data.
 *
 * Note: when one of these functions return a pointer to a string, it is of const type. This is due to the fact that libconfig
 * itself manages their storage.
*/
const gchar *cmd_get_operation(struct agh_cmd *cmd);
config_setting_t *cmd_get_arg(struct agh_cmd *cmd, guint arg_index, gint config_type);
gint agh_cmd_get_id(struct agh_cmd *cmd);

/* events */
struct agh_cmd *cmd_event_prepare(void);
gchar *cmd_event_to_text(struct agh_cmd *cmd, gint event_id);
void cmd_emit_event(struct agh_comm *agh_core_comm, struct agh_cmd *cmd);
const gchar *agh_cmd_event_arg(struct agh_cmd *cmd, guint arg_index);
const gchar *agh_cmd_event_name(struct agh_cmd *cmd);

#endif
