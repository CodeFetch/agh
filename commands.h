#ifndef __commands_h__
#include <glib.h>
#define __commands_h__
#include <libconfig.h>
#include "messages.h"

/* How many characters are acceptable as part of an operation name.*/
#define CMD_MAX_OP_NAME_LEN 10

/* Overall command input text length limit. */
#define CMD_MAX_TEXT_LEN 120

/* IN keyword: should be used for incoming commands */
#define CMD_IN_KEYWORD "AT"

/* OUT keyword: for outgoing commands. */
#define CMD_OUT_KEYWORD "IH"

/* use this to determine an unknown status */
#define CMD_ANSWER_STATUS_UNKNOWN 380

struct command_result {
	guint status;
	GQueue *restextparts;
};

struct command {
	config_t *cmd;
	struct command_result *answer;
};

struct command *text_to_cmd(gchar *content);

/* command results */

void cmd_answer_set_status(struct command *cmd, guint status);
guint cmd_answer_get_status(struct command *cmd);
guint cmd_answer_addtext(struct command *cmd, gchar *text);
gchar *cmd_answer_to_text(struct command *cmd);
guint cmd_answer_prepare(struct command *cmd);

/* assorted management functions */
void cmd_free(struct command *cmd);
struct command *cmd_copy(struct command *cmd);
config_t *cmd_copy_cfg(config_t *src);
void cmd_copy_textpart_single(gpointer data, gpointer user_data);
struct agh_message *cmd_msg(struct command *cmd, GAsyncQueue *src, GAsyncQueue *dest);
struct agh_message *cmd_answer_msg(struct command *cmd, GAsyncQueue *src, GAsyncQueue *dest);

/* Some useful functions to access commands data.
 *
 * note: when one of these functions return a pointer to a string, it is of const type. This is due to the fact that libconfig
 * itself manages their storage. Don't try to modify them in any case.
*/
config_setting_t *cmd_get_in_keyword_setting(struct command *cmd);
gint cmd_get_id(struct command *cmd);
const gchar *cmd_get_operation(struct command *cmd);
config_setting_t *cmd_get_arg(struct command *cmd, guint arg_index, gint config_type);
void print_config_type(gint type);

#endif
