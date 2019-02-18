#include <string.h>
#include <libconfig.h>
#include "agh_commands.h"
#include "agh_messages.h"
#include "agh_logging.h"

/* How many characters are acceptable as part of an operation name? */
#define AGH_CMD_MAX_OP_NAME_LEN 10

/* Overall command input text length limit. */
#define AGH_CMD_MAX_TEXT_LEN 400

/* command source identifier max length */
#define AGH_CMD_MAX_FROM_LEN 70

/* IN keyword: should be used for incoming commands */
#define AGH_CMD_IN_KEYWORD "AT"

/* OUT keyword: for outgoing commands. */
#define AGH_CMD_OUT_KEYWORD "IH"

/* EVENT keyword, for events */
#define AGH_CMD_EVENT_KEYWORD AGH_CMD_OUT_KEYWORD"!"

/* Log messages from AGH_LOG_DOMAIN_COMMANDS domain. */
#define AGH_LOG_DOMAIN_COMMAND	"COMMAND"

/* Logging macros. */
#define agh_log_cmd_dbg(message, ...) agh_log_dbg(AGH_LOG_DOMAIN_COMMAND, message, ##__VA_ARGS__)
#define agh_log_cmd_crit(message, ...) agh_log_crit(AGH_LOG_DOMAIN_COMMAND, message, ##__VA_ARGS__)

/* Unset answer text or event name. */
#define AGH_CMD_NO_DATA_MSG "NO_DATA"
#define AGH_CMD_BUG_EMPTY_EVENT_NAME "BUG_EMPTY_EVENT_NAME"

/* Data structure for AGH command's "answers", or "results". */
struct agh_cmd_res {
	gboolean is_data;
	guint status;
	GQueue *restextparts;
};

/*
 * Sets the status of an agh_cmd command's answer (agh_cmd_res structure).
 *
 * Returns 1 if:
 *  - a NULL agh_cmd structure was passed
 *  - an agh_cmd structure with a NULL agh_cmd_res member was given
 *  - status value was set to 0 or AGH_CMD_ANSWER_STATUS_UNKNOWN (not legal).
*/
gint agh_cmd_answer_set_status(struct agh_cmd *cmd, guint status) {
	gint retval;

	if ((!cmd) || (!cmd->answer) || !status || (status == AGH_CMD_ANSWER_STATUS_UNKNOWN)) {
		agh_log_cmd_crit("attempted to set AGH answer status to an invalid agh_cmd structure");
		retval = 1;
	}
	else {
		cmd->answer->status = status;
		retval = 0;
	}

	return retval;
}

/*
 * Returns the current status value for a command. A return value of 0 indicates a NULL agh_cmd struct was passed.
*/
guint agh_cmd_answer_get_status(struct agh_cmd *cmd) {
	guint retval;

	retval = 0;

	if (cmd && cmd->answer)
		retval = cmd->answer->status;
	else
		agh_log_cmd_crit("attempted to get the AGH answer status for an invalid agh_cmd structure");

	return retval;
}

/*
 * Adds a text argument to an agh_cmd's agh_cmd_res structure, by appending a pointer to the restextparts GQueue.
 * If dup is TRUE, the pointer will point to a copy of the passed text allocated via g_strdup. An allocation failure in this context will lead to an unclean program termination.
 * Otherwise, the passed pointer will become part of the GQueue.
 *
 * Returns 1 when:
 *  - a NULL agh_cmd structure was passed
 *  - the passed agh_cmd struct had a NULL agh_cmd_res pointer structure
 *  - passed text pointer was NULL
*/
guint agh_cmd_answer_addtext(struct agh_cmd *cmd, const gchar *text, gboolean dup) {
	guint retval;

	retval = 0;

	if (!cmd || !cmd->answer || !text) {
		retval = 1;
		agh_log_cmd_crit("attempted to push NULL text to an AGH answer, or to use an invalid agh_cmd structure");
	}
	else {

		if (dup)
			g_queue_push_tail(cmd->answer->restextparts, g_strdup(text));
		else
			g_queue_push_tail(cmd->answer->restextparts, (gchar *)text);

	}

	return retval;
}

/*
 * This function transforms an agh_cmd_res structure content to text. It is destructive, and infact it also deallocates the
 * passed in agh_cmd_res structure. Yeah, this is arguable design.
 *
 * Returns: a gchar pointer, pointing to the resulting text, or NULL when a NULL agh_cmd structure was passed, or one containing a NULL agh_cmd_res pointer.
 *
 * This function can terminate the program uncleanly.
*/
static gchar *agh_cmd_answer_to_text(struct agh_cmd *cmd) {
	GString *output;
	guint ntextparts;
	guint i;
	gchar *current_textpart;

	ntextparts = 0;

	if ((!cmd) || (!cmd->answer)) {
		agh_log_cmd_crit("can not convert to text a NULL agh_cmd_res structure, or passed in agh_cmd structure was NULL");
		return NULL;
	}

	/* Start with the OUT keyword */
	output = g_string_new(AGH_CMD_OUT_KEYWORD" = ( ");

	/* Appends command ID, and status code, adding a comma and a space in between to keep the structure consistent when later appending text parts. */
	g_string_append_printf(output, "%" G_GINT16_FORMAT", %" G_GUINT16_FORMAT"", config_setting_get_int(config_setting_get_elem(config_lookup(cmd->cmd, AGH_CMD_IN_KEYWORD), 0)), cmd->answer->status);

	/* We are going to process the restextparts queue now: it's guaranteed to be not NULL, but it may contain 0 items. */
	ntextparts = g_queue_get_length(cmd->answer->restextparts);

	if (!ntextparts) {
		g_queue_push_tail(cmd->answer->restextparts, g_strdup(AGH_CMD_NO_DATA_MSG));
		ntextparts++;
	}

	if (cmd->answer->is_data)
		g_string_append_printf(output, ", \"DATA\" )");

	for (i=0;i<ntextparts;i++) {
		current_textpart = g_queue_pop_head(cmd->answer->restextparts);

		if (!cmd->answer->is_data)
			g_string_append_printf(output, ", \"%s\"", current_textpart);
		else
			g_string_append_printf(output, "%s", current_textpart);

		g_free(current_textpart);
	}

	/* A space and a close round bracket are to be added to complete the answer. */
	if (!cmd->answer->is_data)
		g_string_append_printf(output, " )");

	/*
	 * I should be honest: I did not think about this when I started using g_queue_pop_head in the above loop. Still, we are
	 * modifying the data structure we are translating to text.
	 * We could have decided to do differently, but... let's deallocate the structure itself, and go on.
	*/
	g_queue_free(cmd->answer->restextparts);

	cmd->answer->status = AGH_CMD_ANSWER_STATUS_UNKNOWN;
	cmd->answer->restextparts = NULL;

	g_free(cmd->answer);
	cmd->answer = NULL;

	return g_string_free(output, FALSE);
}

/*
 * Allocates an agh_cmd_res structure, and prepares it for later use by other AGH command's related functions.
 *
 * Returns: an unsigned integer with value 1 if the passed in agh_cmd structure is NULL or holds a not NULL pointer to an agh_cmd_res one.
 * An unsigned integer of value 2 indicates a memory allocation failure.
 *
 * This function may lead to an unclean program termination.
*/
guint agh_cmd_answer_alloc(struct agh_cmd *cmd) {
	guint retval;

	retval = 0;

	if (!cmd || cmd->answer) {
		agh_log_cmd_crit("NULL agh_cmd structure or agh_cmd structure with an answer already allocated");
		retval = 1;
	}
	else {

		cmd->answer = g_try_malloc0(sizeof(*cmd->answer));
		if (!cmd->answer) {
			agh_log_cmd_crit("can not allocate memory for the answer agh_cmd_res structure");
			retval = 2;
		}
		else {
			cmd->answer->status = AGH_CMD_ANSWER_STATUS_UNKNOWN;
			cmd->answer->restextparts = g_queue_new();
		}

	}

	return retval;
}

/*
 * Frees an AGH command structure.
 *
 * Returns: an integer with value 0 on success,
 * -10 when command structure is NULL
 * -11 when a command answer structure was present, but no restextparts GQueue.
*/
gint agh_cmd_free(struct agh_cmd *cmd) {
	gint retval;

	retval = 0;

	if (!cmd) {
		agh_log_cmd_dbg("not deallocating a NULL AGH command");
		retval = -10;
		return retval;
	}

	if (cmd->cmd) {
		config_destroy(cmd->cmd);
		g_free(cmd->cmd);
	}

	/* Command answer. */
	if (cmd->answer) {
		cmd->answer->status = AGH_CMD_ANSWER_STATUS_UNKNOWN;

		if (cmd->answer->restextparts)
			g_queue_free_full(cmd->answer->restextparts, g_free);
		else {
			agh_log_cmd_dbg("command with an answer structure, but no restextparts");
			retval = -11;
		}

		g_free(cmd->answer);
	}

	g_free(cmd->cmd_source_id);

	g_free(cmd);

	return retval;
}

/*
 * Creates a new copy of a libconfig command config structure, as used in AGH.
 * This function has been written to cope with known "valid" config_t command structures, as checked in the agh_text_to_cmd
 * function. Thus, not all config_t structures will be handled correctly. Any better way to do this is apreciated.
 *
 * Returns: a new config_t structure, or NULL when:
 *  - the passed in source config_t structure was NULL
 *  - failure while allocating the new config_t structure
 *  - the error_value integer is not initialized to zero when calling us
*/
static config_t *agh_cmd_copy_cfg(config_t *src, gint *error_value) {
	config_t *dest_cfg;
	config_setting_t *dest_root_setting;
	config_setting_t *dest_list_setting;
	config_setting_t *dest_current_setting;
	gint src_current_setting_type;
	config_setting_t *src_in_keyword;

	/* Index used for scanning the source config structure in a while loop. */
	guint index;

	/* Element currently being processed. */
	config_setting_t *src_elem;

	index = 0;
	dest_cfg = NULL;

	/* This is going to happen every time this function operates on AGH events, which are basically "answers nobody asked for". */
	if (!src || *error_value) {
		agh_log_cmd_dbg("NULL config_t structure or error_value not set to zero");
		return dest_cfg;
	}

	dest_cfg = g_try_malloc0(sizeof(*dest_cfg));
	if (!dest_cfg) {
		agh_log_cmd_crit("can not allocate new config_t structure");
#define AGH_CMD_COPY_CFG_ENOMEM 1
		*error_value = AGH_CMD_COPY_CFG_ENOMEM;
		return dest_cfg;
	}

	config_init(dest_cfg);

	/* 1 - Get root setting on new config. We're sure there is one. */
	dest_root_setting = config_root_setting(dest_cfg);

	/* 2 - Add our AGH_CMD_IN_KEYWORD list setting. */
	dest_list_setting = config_setting_add(dest_root_setting, AGH_CMD_IN_KEYWORD, CONFIG_TYPE_LIST);

	if (!dest_list_setting) {
		agh_log_cmd_crit("internal error while adding the destination list config setting via config_setting_add when copying a command");
		goto wayout;
	}

	/* 3 - Copy config settings from source to destination. */
	src_in_keyword = config_lookup(src, AGH_CMD_IN_KEYWORD);

	while ( (src_elem = config_setting_get_elem(src_in_keyword, index)) ) {
		src_current_setting_type = config_setting_type(src_elem);
		dest_current_setting = config_setting_add(dest_list_setting, NULL, src_current_setting_type);

		/*
		 * Given the fact we check for settings types before writing them to new ones, we are not checking the final result(s).
		 * Infact, both config_setting_set_* and their config_setting_get_* counterparts, return useful error values.
		 * Should components receive zero values unexpectedly, checking what's going on here could be a good idea. This place may
		 * also be more subject than others to effects due to changes in libconfig.
		*/
		switch(src_current_setting_type) {
			case CONFIG_TYPE_INT:
				config_setting_set_int(dest_current_setting, config_setting_get_int(src_elem));
				break;
			case CONFIG_TYPE_INT64:
				config_setting_set_int64(dest_current_setting, config_setting_get_int64(src_elem));
				break;
			case CONFIG_TYPE_FLOAT:
				config_setting_set_float(dest_current_setting, config_setting_get_float(src_elem));
				break;
			case CONFIG_TYPE_STRING:
				config_setting_set_string(dest_current_setting, config_setting_get_string(src_elem));
				break;
			case CONFIG_TYPE_BOOL:
				config_setting_set_bool(dest_current_setting, config_setting_get_bool(src_elem));
				break;
			default:
				agh_log_cmd_crit("unsupported (or unknown) setting type while processing command config structure for copying.\n\tGrab a cup of coffee and look at this please");
				goto wayout;
		}

		index++;
	}

	return dest_cfg;

wayout:
	config_destroy(dest_cfg);
	g_free(dest_cfg);
	dest_cfg = NULL;
	return dest_cfg;
}

/*
 * Copies an agh_cmd structure, and involved members structures.
 *
 * Returns: an agh_cmd struct on success, NULL when a memory allocation failure occurs, an error is encountered in agh_cmd_copy_cfg,
 * or a NULL agh_cmd structure is passed as parameter.
 *
 * Note that, especially when processing agh_cmd_res structures, failures while allocating memory will result in the program terminating uncleanly.
*/
struct agh_cmd *agh_cmd_copy(struct agh_cmd *cmd) {
	struct agh_cmd *new_cmd;
	config_t *new_cfg;
	struct agh_cmd_res *new_cmd_answer;
	gint agh_cmd_copy_cfg_error_value;

	/* required by agh_cmd_copy_cfg */
	agh_cmd_copy_cfg_error_value = 0;

	if (!cmd) {
		agh_log_cmd_dbg("NULL agh_cmd structure");
		return cmd;
	}

	new_cmd = g_try_malloc0(sizeof(*new_cmd));
	if (!new_cmd) {
		agh_log_cmd_crit("unable to allocate a new agh_cmd structure for copying");
		return new_cmd;
	}

	new_cfg = agh_cmd_copy_cfg(cmd->cmd, &agh_cmd_copy_cfg_error_value);
	if (new_cfg)
		new_cmd->cmd = new_cfg;
	else
		switch(agh_cmd_copy_cfg_error_value) {
			case 0:
				/* no config was present on the original structure */
				break;
			case AGH_CMD_COPY_CFG_ENOMEM:
				goto wayout;
			default:
				agh_log_cmd_crit("unhandled error while copying command");
				goto wayout;
		}

	if (cmd->answer) {
		new_cmd_answer = g_try_malloc0(sizeof(*new_cmd_answer));
		if (!new_cmd_answer) {
			agh_log_cmd_crit("failure while allocating new agh_cmd_res structure for copying");
			goto wayout;
		}

		new_cmd_answer->status = cmd->answer->status;
		new_cmd_answer->is_data = cmd->answer->is_data;

		new_cmd_answer->restextparts = g_queue_new();

		g_queue_foreach(cmd->answer->restextparts, agh_copy_textparts, new_cmd_answer->restextparts);

		new_cmd->answer = new_cmd_answer;
	}

	if (cmd->cmd_source_id)
		new_cmd->cmd_source_id = g_strdup(cmd->cmd_source_id);

	if ((!new_cmd->cmd) && (!new_cmd->answer)) {
		agh_log_cmd_dbg("no agh_cmd nor agh_cmd_res structures where successfully copied");
		goto wayout;
	}

	return new_cmd;

wayout:
	agh_cmd_free(new_cmd);
	new_cmd = NULL;
	return new_cmd;
}

/*
 * Builds a new agh_message structure, holding the text representation of the answer to the agh_cmd structure passed in input.
 * If dest_comm is NULL, src_comm will be used as destination as well.
 *
 * Returns: on success, an agh_message containing the agh_cmd_res text representation, or NULL when:
 *  - a NULL agh_cmd structure is passed in, or one with a NULl agh_cmd_res pointer
 *  - both src_comm and dest_comm are NULL
 *
 * Note: this function performs the same checks we can find in agh_cmd_answer_to_text. No other failures are "possible" inside
 * agh_cmd_answer_to_text itself at the moment (e.g.: memory allocations failures will cause the program to terminate uncleanly).
 * Still, we keep these checks to try to catch changes in that function, possibly generating new failure conditions.
 * Furthermore, this function may terminate the program uncleanly, too.
*/
struct agh_message *agh_cmd_answer_msg(struct agh_cmd *cmd, struct agh_comm *src_comm, struct agh_comm *dest_comm) {
	struct agh_message *m;
	struct agh_text_payload *text_payload;

	m = NULL;

	if (!cmd || !cmd->answer) {
		agh_log_cmd_crit("NULL agh_cmd structure, or NULL agh_cmd_res pointer");
		return m;
	}

	if ((!dest_comm) && (!src_comm)) {
		agh_log_cmd_crit("both source and dest COMMs are NULL");
		return m;
	}

	if (!dest_comm)
		dest_comm = src_comm;

	text_payload = g_try_malloc0(sizeof(*text_payload));
	if (!text_payload) {
		agh_log_cmd_crit("failure while allocating text payload when building answer message from an agh_cmd structure");
		goto wayout;
	}

	text_payload->text = agh_cmd_answer_to_text(cmd);
	if (!text_payload->text) {
		agh_log_cmd_crit("NULL answer text");
		goto wayout;
	}

	if (cmd->cmd_source_id)
		text_payload->source_id = g_strdup(cmd->cmd_source_id);

	m = agh_msg_alloc();
	if (!m)
		goto wayout;

	m->csp = text_payload;
	m->msg_type = MSG_SENDTEXT;
	m->src = src_comm;
	m->dest = dest_comm;

	return m;

wayout:
	g_free(text_payload->text);
	g_free(text_payload);
	return m;
}

/*
 * Return the AGH_CMD_IN_KEYWORD configuration setting.
 * This function has been tought to handle valid agh_cmd structures as per agh_text_to_cmd.

 * Returns: a config_setting_t pointer on success, NULL on failure (NULL agh_cmd structure passed).
 *
 * Note: may we use an assert-like function here?
*/
static config_setting_t *agh_cmd_get_in_keyword_setting(struct agh_cmd *cmd) {

	if (!cmd) {
		agh_log_cmd_crit("can not lookup the "AGH_CMD_IN_KEYWORD" list on a NULL agh_cmd structure");
		return NULL;
	}

	/* Should something go wrong, this returns NULL as well. */
	return config_lookup(cmd->cmd, AGH_CMD_IN_KEYWORD);
}

/*
 * Get the ID of the passed in agh_cmd structure.
 *
 * Returns: an integer with value set as the AGH command ID, or 0 on failure, when a NULL agh_cmd structure was passed in or the AGH_CMD_IN_KEYWORD list could not be found.
*/
gint agh_cmd_get_id(struct agh_cmd *cmd) {
	config_setting_t *in_keyword;
	gint id;

	id = 0;

	if (!cmd) {
		agh_log_cmd_crit("problem getting the ID of a NULL agh_cmd struct");
		return id;
	}

	if (! (in_keyword = agh_cmd_get_in_keyword_setting(cmd)) ) {
		agh_log_cmd_crit("no "AGH_CMD_IN_KEYWORD" found!");
		return id;
	}

	id = config_setting_get_int(config_setting_get_elem(in_keyword, 0));
	return id;
}

const gchar *cmd_get_operation(struct agh_cmd *cmd) {
	config_setting_t *in_keyword;
	const gchar *operation;

	in_keyword = NULL;
	operation = NULL;

	if (!cmd)
		return operation;

	if (! (in_keyword = agh_cmd_get_in_keyword_setting(cmd)) )
		return operation;

	operation = config_setting_get_string(config_setting_get_elem(in_keyword, 1));
	return operation;
}

config_setting_t *cmd_get_arg(struct agh_cmd *cmd, guint arg_index, gint config_type) {
	config_setting_t *outset;
	config_setting_t *in_keyword;

	outset = NULL;
	in_keyword = NULL;

	if (!cmd)
		return outset;

	/*
	 * An arg_index being zero should give the operation, and in general it should not be a problem. Still, we may decide or need
	 * to "reserve" this value.
	*/
	if (!arg_index)
		return NULL;

	if (! (in_keyword = agh_cmd_get_in_keyword_setting(cmd)) )
		return outset;

	outset = config_setting_get_elem(in_keyword, 1+arg_index);

	if (!outset)
		return outset;

	/* This should not leak memory, because we're managing memory pertaining to the command config structure. */
	if (config_setting_type(outset) != config_type) {
		outset = NULL;
	}

	return outset;
}

struct agh_cmd *cmd_event_prepare(void) {
	struct agh_cmd *cmd;

	cmd = NULL;

	cmd = g_malloc0(sizeof(struct agh_cmd));

	cmd->answer = g_malloc0(sizeof(struct agh_cmd_res));

	cmd->answer->status = AGH_CMD_EVENT_UNKNOWN_ID;
	cmd->answer->restextparts = g_queue_new();

	return cmd;
}

/*
 * This function transforms an event's agh_cmd_res structure content to text. It is destructive, and infact it also
 * deallocates the agh_cmd_res structure contained in the command used as event. And again, this is arguable design. A lot of code here is in common with the agh_cmd_answer_to_text
 * function, and infact it has been copied from there. Maybe unifying those function is a good idea.
*/
gchar *cmd_event_to_text(struct agh_cmd *cmd, gint event_id) {
	GString *output;
	guint ntextparts;
	guint i;
	gchar *current_textpart;

	ntextparts = 0;
	current_textpart = NULL;

	if (!cmd)
		return NULL;

	/* Start with the EVENT keyword */
	output = g_string_new(AGH_CMD_EVENT_KEYWORD" = ( ");

	/* Appends event ID, adding at last a comma and a space to keep the structure consistent when later appending text parts. */
	g_string_append_printf(output, "%" G_GINT16_FORMAT", %" G_GUINT16_FORMAT"", event_id, cmd->answer->status);

	/* We are going to process the restextparts queue now: it's guaranteed to be not NULL, but it may contain 0 items. */
	ntextparts = g_queue_get_length(cmd->answer->restextparts);

	if (!ntextparts) {
		g_queue_push_tail(cmd->answer->restextparts, g_strdup(AGH_CMD_BUG_EMPTY_EVENT_NAME));
		ntextparts++;
	}

	if (cmd->answer->is_data) {
		current_textpart = g_queue_pop_head(cmd->answer->restextparts);
		g_string_append_printf(output, ", %s, \"DATA\" )", current_textpart);
		ntextparts--;
		g_free(current_textpart);
	}

	for (i=0;i<ntextparts;i++) {
		current_textpart = g_queue_pop_head(cmd->answer->restextparts);

		if (!cmd->answer->is_data)
			g_string_append_printf(output, ", \"%s\"", current_textpart);
		else
			g_string_append_printf(output, "%s", current_textpart);

		g_free(current_textpart);

	}

	/* A space and a close round bracket are to be added to complete the answer. */
	if (!cmd->answer->is_data)
		g_string_append_printf(output, " )");

	/*
	* See agh_cmd_answer_to_text for clarification.
	*/
	g_queue_free(cmd->answer->restextparts);

	/* Yeah, probably useless. */
	cmd->answer->status = AGH_CMD_EVENT_UNKNOWN_ID;
	cmd->answer->restextparts = NULL;

	g_free(cmd->answer);
	cmd->answer = NULL;

	return g_string_free(output, FALSE);
}

void cmd_emit_event(struct agh_comm *agh_core_comm, struct agh_cmd *cmd) {
	struct agh_message *m;

	m = NULL;

	if (!cmd)
		return;

	if (!cmd->answer) {
		g_print("%s: can not emit event with no answer (cmd->answer)\n",__FUNCTION__);
		return;
	}

	m = agh_msg_alloc();
	m->msg_type = MSG_EVENT;
	m->csp = cmd;
	if (agh_msg_send(m, agh_core_comm, NULL)) {
		g_print("%s: can not send message to core\n",__FUNCTION__);
		return;
	}

	return;
}

/*
 * Gets event name. It should not in general return NULL, but it may do so. Infact, g_queue_peek_nth may return NULL if you try to access a position off the end of queue.
 * Note that the pointer returned here is "inside" the structure itself, and that's why it's const. So don't try to modify, or free it. And clearly do not use it once you freed or sent the event.
*/
const gchar *agh_cmd_event_name(struct agh_cmd *cmd) {
	const gchar *textop;

	textop = NULL;

	if (!cmd->answer)
		return NULL;

	textop = g_queue_peek_nth(cmd->answer->restextparts, 0);

	return textop;
}

/*
 * Gets an event argument. It returns NULL if specified argument is not present. Infact, g_queue_peek_nth may return NULL if you try to access a position off the end of queue.
*/
const gchar *agh_cmd_event_arg(struct agh_cmd *cmd, guint arg_index) {
	const gchar *arg;

	arg = NULL;

	if (!arg_index)
		return arg;

	arg = g_queue_peek_nth(cmd->answer->restextparts, arg_index);

	return arg;
}

void cmd_answer_set_data(struct agh_cmd *cmd, gboolean is_data) {
	cmd->answer->is_data = is_data;
	return;
}

void cmd_answer_if_empty(struct agh_cmd *cmd, guint status, gchar *text, gboolean set_is_data) {
	if (!cmd || !text)
		return;

	if (!g_queue_get_length(cmd->answer->restextparts)) {
		agh_cmd_answer_addtext(cmd, text, TRUE);
		cmd->answer->is_data = set_is_data;
		agh_cmd_answer_set_status(cmd, status);
	}

	return;
}

/*
 * This function gets a string pointer as input ( gchar * ), returning an agh_cmd structure as output.
 * If the passed in string isn't considered a valid command because of it's invalid structure, or because the resulting
 * libconfig configuration isn't the one required / expected by this program, then NULL is returned.
 * Furthermore, a memory allocation failure will result in a NULL pointer being returned.
 *
 * Returns: an agh_cmd structure holding a valid command (in terms of structure).
 * A NULL pointer is returned when:
 *   - memory allocation failure when allocating the agh_cmd or the config_t structure
 *   - a NULL pointer was returned g_str_to_ascii during ascii conversion of input
 *   - the input violated libconfig grammar rules
 *   - configuration root setting length was different from 1
 *   - AGH_CMD_IN_KEYWORD attention keyword was not found
 *   - either an ID nor an operation name are absent or invalid
 *   - a source ID was present, but it's ascii representation was not in the 1<len<AGH_CMD_MAX_FROM_LEN length range.
*/
struct agh_cmd *agh_text_to_cmd(gchar *from, gchar *content) {

	/* A new command, returned by the function in case of success. */
	struct agh_cmd *ocmd;

	/* The config structure holding the user input. May not be considered a valid command. */
	config_t *cmd_cfg;

	/* Input from user is converted to ascii text; this is a pointer to the converted text. */
	gchar *atext;

	/* libconfig setting pointer; should point to the AGH_CMD_IN_KEYWORD keyword when found */
	config_setting_t *in_keyword;

	/* command ID: libconfig setting pointer and integer ID value */
	config_setting_t *id;
	gint cmd_id;

	/* command operation: libconfig setting element and the corresponding pointer */
	config_setting_t *op;
	const gchar *cmd_operation;

	/* Used for holding lengths; command overall length, operation name length and the source ID one. */
	guint lengths;

	/* Source ID, converted to ASCII. */
	gchar *afrom;

	ocmd = NULL;
	afrom = NULL;

	if (!content) {
		agh_log_cmd_dbg("content was NULL");
		return ocmd;
	}

	lengths = strlen(content);
	if (lengths > AGH_CMD_MAX_TEXT_LEN) {
		agh_log_cmd_dbg("AGH_CMD_MAX_TEXT_LEN exceeded (%d)",AGH_CMD_MAX_TEXT_LEN);
		return ocmd;
	}

	cmd_cfg = g_try_malloc0(sizeof(config_t));
	if (!cmd_cfg) {
		agh_log_cmd_dbg("can not allocate memory for config structure");
		return ocmd;
	}

	config_init(cmd_cfg);

	/* Convert given input to ascii, just in case. */
	atext = g_str_to_ascii(content, "C");

	/* Is this useless? */
	if (!atext) {
		agh_log_cmd_crit("oh, so it is possible to send an input which results in a NULL ptr as output from ascii conversion");
		goto wayout;
	}

	if (!config_read_string(cmd_cfg, atext)) {
		/* Invalid input. */
		agh_log_cmd_dbg("invalid input: \n\t\t%s\n",atext);
		goto wayout;
	}

	/*
	 * A command should clearly respect the libconfig configuration grammar. In our context, it should be formed of the
	 * following elements:
	 *
	 * - the AGH_CMD_IN_KEYWORD keyword / setting
	 * - an equal sign
	 * - a list, which should contain an operation ID (long unsigned int), and an operation name (char *).
	 *
	 * Any further data is command-specific. A command may require zero or more arguments. Data outside the list should be
	 * rejected, or, at least, ignored during processing.
	*/

	/* 1 - Root setting is a group, and it should contain only one element.
	 * This should be considered a sanity check, and may be removed in future. Infact, if I understood this correctly, libconfig
	 * always builds a group root setting, which contains whatever gets processed.
	*/
	if (config_setting_length(config_root_setting(cmd_cfg)) != 1) {
		agh_log_cmd_dbg("excess data while processing command config structure");
		goto wayout;
	}

	/* 2 - AGH_CMD_IN_KEYWORD keyword */
	in_keyword = config_lookup(cmd_cfg, AGH_CMD_IN_KEYWORD);

	if (!in_keyword) {
		agh_log_cmd_dbg(AGH_CMD_IN_KEYWORD" attention keyword not found");
		goto wayout;
	}

	/* 3 - The AGH_CMD_IN_KEYWORD setting should be a list, and contain a minimum of 2 keywords. */
	if (!config_setting_is_list(in_keyword)) {
		agh_log_cmd_dbg("unexpected command config structure (not a config list)");
		goto wayout;
	}

	if (config_setting_length(in_keyword) < 2) {
		agh_log_cmd_dbg("at least an operation and a command ID are required");
		goto wayout;
	}

	/* 4 - Command ID, should be gint and != 0. */
	id = config_setting_get_elem(in_keyword, 0);
	cmd_id = config_setting_get_int(id);
	if (cmd_id < 1) {
		agh_log_cmd_dbg("invalid command ID");
		goto wayout;
	}

	/* 5 - Operation should not be an empty string. */
	op = config_setting_get_elem(in_keyword, 1);
	cmd_operation = config_setting_get_string(op);
	if (!cmd_operation) {
		agh_log_cmd_crit("NULL operation name is not considered legal");
		goto wayout;
	}

	/* 6 - Operation name should consist at least of a single character. */
	lengths = strlen(cmd_operation);
	if (!lengths) {
		agh_log_cmd_dbg("an operation name should consist of at least one character");
		goto wayout;
	}

	/* 7 - Operation name may consist of AGH_CMD_MAX_OP_NAME_LEN characters at most. */
	if (lengths > AGH_CMD_MAX_OP_NAME_LEN) {
		agh_log_cmd_dbg("AGH_CMD_MAX_OP_NAME_LEN exceeded (%d)", AGH_CMD_MAX_OP_NAME_LEN);
		goto wayout;
	}

	/* 8 - If present, the ascii representation of the source ID should be of 1<len<AGH_CMD_MAX_FROM_LEN length. */
	if (from) {
		lengths = strlen(from);

		if (lengths < 1 || lengths > AGH_CMD_MAX_FROM_LEN) {
			agh_log_cmd_crit("source identifier exceeds AGH_CMD_MAX_FROM_LEN (%d)",AGH_CMD_MAX_FROM_LEN);
			goto wayout;
		}

		afrom = g_str_to_ascii(from, "C");

		if (!afrom) {
			agh_log_cmd_crit("source ID is NULL after ascii conversion");
			goto wayout;
		}

	}

	ocmd = g_try_malloc0(sizeof(*ocmd));

	if (!ocmd) {
		agh_log_cmd_crit("can not allocate memory for agh_cmd structure, ID=%" G_GINT16_FORMAT"",cmd_id);
		goto wayout;
	}

	ocmd->cmd = cmd_cfg;

	if (afrom)
		ocmd->cmd_source_id = afrom;

	g_free(atext);

	agh_log_cmd_dbg("cmd OK, ID=%" G_GINT16_FORMAT"",cmd_id);
	return ocmd;

wayout:
	g_free(atext);
	g_free(afrom);
	config_destroy(cmd_cfg);
	g_free(cmd_cfg);
	return ocmd;
}
