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

/* Log messages from AGH_LOG_DOMAIN_COMMAND domain. */
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
gint agh_cmd_answer_addtext(struct agh_cmd *cmd, const gchar *text, gboolean dup) {
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
 * Parameters:
 *  - struct agh_cmd *cmd: command
 *  - const gchar *keyword: the keyword to be used when building answer text
 *  - gint event_id: event ID (if not 0, we are building an event, otherwise this is an answer, and ID is taken from the libconfig config_t structure);
 *
 * Returns: a gchar pointer, pointing to the resulting text, or NULL when a NULL agh_cmd structure was passed, or one containing a NULL "answer" pointer.
 *
 * This function can terminate the program uncleanly.
*/
gchar *agh_cmd_answer_to_text(struct agh_cmd *cmd, const gchar *keyword, gint event_id) {
	GString *output;
	guint ntextparts;
	guint i;
	gchar *current_textpart;

	ntextparts = 0;

	if ((!cmd) || (!cmd->answer)) {
		agh_log_cmd_crit("can not convert to text a NULL agh_cmd_res structure, or passed in agh_cmd structure was NULL");
		return NULL;
	}

	if (!keyword) {
		agh_log_cmd_crit("keyword may not be NULL");
		return NULL;
	}

	/* Start with the keyword */
	output = g_string_new(NULL);
	g_string_append_printf(output, "%s = ( ", keyword);

	/* Appends command ID or event ID, and status code, adding a comma and a space in between to keep the structure consistent when later appending text parts. */
	if (event_id)
		g_string_append_printf(output, "%" G_GINT16_FORMAT", %" G_GUINT16_FORMAT"", event_id, cmd->answer->status);
	else
		g_string_append_printf(output, "%" G_GINT16_FORMAT", %" G_GUINT16_FORMAT"", config_setting_get_int(config_setting_get_elem(config_lookup(cmd->cmd, AGH_CMD_IN_KEYWORD), 0)), cmd->answer->status);

	/* We are going to process the restextparts queue now: it's guaranteed to be not NULL, but it may contain 0 items. */
	ntextparts = g_queue_get_length(cmd->answer->restextparts);

	if (!ntextparts) {
		g_queue_push_tail(cmd->answer->restextparts, g_strdup(AGH_CMD_NO_DATA_MSG));
		ntextparts++;
	}

	if (cmd->answer->is_data) {

		if (event_id) {
			current_textpart = g_queue_pop_head(cmd->answer->restextparts);
			g_string_append_printf(output, ", %s, \"DATA\" ) ", current_textpart);
			ntextparts--;
			g_free(current_textpart);
		}

		else
			g_string_append_printf(output, ", \"DATA\" )");
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
	 * I should be honest: I did not think about this when I started using g_queue_pop_head in the above loop. Still, we are
	 * modifying the data structure we are translating to text.
	 * We could have decided to do differently, but... let's deallocate the structure itself, and go on.
	*/
	g_queue_free(cmd->answer->restextparts);

	if (event_id)
		cmd->answer->status = AGH_CMD_EVENT_UNKNOWN_ID;
	else
		cmd->answer->status = AGH_CMD_ANSWER_STATUS_UNKNOWN;

	cmd->answer->restextparts = NULL;

	g_free(cmd->answer);
	cmd->answer = NULL;

	return g_string_free(output, FALSE);
}

/*
 * Allocates an agh_cmd_res structure, and prepares it for later use by other AGH command's related functions.
 *
 * Returns: an integer with value 1 if the passed in agh_cmd structure is NULL or holds a not NULL pointer to an agh_cmd_res one.
 * An integer of value 2 indicates a memory allocation failure.
 * Oh - and on success it should return 0.
 *
 * This function may lead to an unclean program termination.
*/
gint agh_cmd_answer_alloc(struct agh_cmd *cmd) {
	guint retval;

	retval = 0;

	if (!cmd || cmd->answer) {
		agh_log_cmd_crit("NULL agh_cmd structure or agh_cmd structure with an answer already allocated");
#define AGH_CMD_ANSWER_ALLOC_BADCMD 1
		retval = AGH_CMD_ANSWER_ALLOC_BADCMD;
	}
	else {

		cmd->answer = g_try_malloc0(sizeof(*cmd->answer));
		if (!cmd->answer) {
			agh_log_cmd_crit("can not allocate memory for the answer agh_cmd_res structure");
#define AGH_CMD_ANSWER_ALLOC_ENOMEM 2
			retval = AGH_CMD_ANSWER_ALLOC_ENOMEM;
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
 *  - the integer pointed by error_value is not initialized to 0 when calling us
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
	if (!src || !error_value || *error_value) {
		agh_log_cmd_dbg("NULL config_t structure or error_value not set to 0");
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

	text_payload->text = agh_cmd_answer_to_text(cmd, AGH_CMD_OUT_KEYWORD, 0);
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

	if (!cmd || !cmd->cmd) {
		agh_log_cmd_crit("can not lookup the "AGH_CMD_IN_KEYWORD" list on a NULL agh_cmd structure, or an event-related one");
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

	if (!cmd || !cmd->cmd) {
		agh_log_cmd_crit("problem getting the ID of a NULL agh_cmd struct, or an event-related one");
		return id;
	}

	if (! (in_keyword = agh_cmd_get_in_keyword_setting(cmd)) ) {
		agh_log_cmd_crit("no "AGH_CMD_IN_KEYWORD" found!");
		return id;
	}

	id = config_setting_get_int(config_setting_get_elem(in_keyword, 0));
	return id;
}

/*
 * Obtain the "operation" string of the passed in agh_cmd structure.
 *
 * Returns: a pointer to the string representing the operation related to the passed agh_cmd structure on success, NULL when:
 *  - the passed in agh_cmd structure is NULL
 *  - the AGH_CMD_IN_KEYWORD list could not be found.
 *
 * Note: the agh_cmd structure is supposed to be valid.
*/
const gchar *agh_cmd_get_operation(struct agh_cmd *cmd) {
	config_setting_t *in_keyword;
	const gchar *operation;

	operation = NULL;

	if (!cmd || !cmd->cmd) {
		agh_log_cmd_crit("NULL or event-related agh_cmd structure");
		return operation;
	}

	if (! (in_keyword = agh_cmd_get_in_keyword_setting(cmd)) ) {
		agh_log_cmd_crit("no "AGH_CMD_IN_KEYWORD" list found");
		return operation;
	}

	operation = config_setting_get_string(config_setting_get_elem(in_keyword, 1));

	/* This maybe NULL! */
	return operation;
}

/*
 * Get a specified argument and check it's of the specified type.
 * If config_type is set to CONFIG_TYPE_NONE, no type checking is performed.
 *
 * Returns: the argument's libconfig setting on success, NULL otherwise
 * (e.g.: specified argument does not exist or is not of the specified type, NULL or event-related agh_cmd struct was passed in, attention keyword list not found).
 *
 * Note: arg_index == 0 is not considered legal.
*/
config_setting_t *agh_cmd_get_arg(struct agh_cmd *cmd, guint arg_index, gint config_type) {
	config_setting_t *outset;
	config_setting_t *in_keyword;

	outset = NULL;

	if (!cmd || !cmd->cmd) {
		agh_log_cmd_crit("NULL or event-related agh_cmd structure while checking for args");
		goto wayout;
	}

	/*
	 * An arg_index == 0 should give the operation (1+0), and in general it should not be a problem. Still, we may decide or need
	 * to "reserve" this value.
	*/
	if (!arg_index) {
		agh_log_cmd_crit("arg_index == 0 not allowed");
		goto wayout;
	}

	if (! (in_keyword = agh_cmd_get_in_keyword_setting(cmd)) ) {
		agh_log_cmd_dbg(AGH_CMD_IN_KEYWORD" attention keyword not found while getting args");
		return outset;
	}

	outset = config_setting_get_elem(in_keyword, 1+arg_index);

	if (!outset)
		goto wayout;

	if (config_type == CONFIG_TYPE_NONE)
		goto wayout;

	/* This should not leak memory, because we're managing memory pertaining to the command config structure. */
	if (config_setting_type(outset) != config_type)
		outset = NULL;

wayout:
	return outset;
}

/*
 * Allocates a new AGH event, an agh_cmd struct with an agh_cmd_res struct member "only".
 *
 * Returns: on success, a new agh_cmd structure is returned.
 * A NULL pointer may be returned upon memory allocation failure, or a failure while in agh_cmd_answer_alloc.
 * Infact, this function may lead to an unclean program termination.
 *
 * If a not NULL pointer is passed as error_value, and the pointer integer has 0 value, this function returns error indications there.
 * In particular, *error_value will be equal to 10 on memory allocation failure while allocating a new agh_cmd struct. All other values are directly from agh_cmd_answer_alloc.
*/
struct agh_cmd *agh_cmd_event_alloc(gint *error_value) {
	struct agh_cmd *cmd;
	gint answer_alloc_error;

	cmd = g_try_malloc0(sizeof(*cmd));
	if (!cmd) {
		agh_log_cmd_crit("unable to allocate an agh_cmd struct for new event");

		if (error_value && !*error_value) {
#define AGH_CMD_EVENT_ALLOC_ENOMEM 10
			*error_value = AGH_CMD_EVENT_ALLOC_ENOMEM;
		}

		return cmd;
	}

	answer_alloc_error = agh_cmd_answer_alloc(cmd);

	if (answer_alloc_error) {
		agh_cmd_free(cmd);
		cmd = NULL;

		if (error_value && !*error_value)
			*error_value = answer_alloc_error;

	}
	else
		cmd->answer->status = AGH_CMD_EVENT_UNKNOWN_ID;

	return cmd;
}

/*
 * Emits an event, preparing a message on which the passed in agh_cmd structure is "linked" as CSP.
 *
 * Returns: an integer with value 0 on success, or
 *  - -20 if the passed agh_cmd structure is NULL or invalid
 *  - -21 when specified COMM is NULL
 *  - -22 when message allocation fails
 * Any other value comes directly from agh_msg_send.
*/
gint agh_cmd_emit_event(struct agh_comm *agh_core_comm, struct agh_cmd *cmd) {
	struct agh_message *m;
	gint retval;

	retval = 0;

	if (!cmd || !cmd->answer) {
		agh_log_cmd_crit("invalid or NULL agh_cmd structure");
		retval = -20;
		goto wayout;
	}

	if (!agh_core_comm) {
		agh_log_cmd_crit("NULL COMM specified");
		retval = -21;
		goto wayout;
	}

	m = agh_msg_alloc();
	if (!m) {
		retval = -22;
		goto wayout;
	}

	m->msg_type = MSG_EVENT;
	m->csp = cmd;
	retval = agh_msg_send(m, agh_core_comm, NULL);

wayout:
	return retval;
}

/*
 * Gets event name. It can return NULL.
 * Note that the pointer returned here is "inside" the structure itself, and so it's const.
*/
const gchar *agh_cmd_event_name(struct agh_cmd *cmd) {
	const gchar *textop;

	if (!cmd || !cmd->answer) {
		agh_log_cmd_crit("NULL agh_cmd struct, or no agh_cmd_res one");
		return NULL;
	}

	textop = g_queue_peek_nth(cmd->answer->restextparts, 0);

	return textop;
}

/*
 * Gets an event argument. It returns NULL if specified argument is not present. Infact, g_queue_peek_nth may return NULL if you try to access a position off the end of queue.
*/
const gchar *agh_cmd_event_arg(struct agh_cmd *cmd, guint arg_index) {
	const gchar *arg;

	arg = NULL;

	if (!arg_index) {
		agh_log_cmd_crit("arg_index not legal");
		return arg;
	}

	arg = g_queue_peek_nth(cmd->answer->restextparts, arg_index);

	return arg;
}

/*
 * Sets data flag for an agh_cmd_res structure.
 *
 * Returns: 0 on success, -1 when the passed in agh_cmd struct is NULL, or has a NULL agh_cmd_res pointer member.
*/
gint agh_cmd_answer_set_data(struct agh_cmd *cmd, gboolean is_data) {
	gint retval;

	retval = 0;

	if (!cmd || !cmd->answer) {
		agh_log_cmd_crit("can not set data flag on NULL agh_cmd or agh_cmd_res structure");
		retval = -1;
		goto wayout;
	}

	cmd->answer->is_data = is_data;

wayout:
	return retval;
}

/*
 * Act on an agh_cmd_res structure when restextparts is empty.
 *
 * What this function does is actually allow the caller to add some text, set the answer status and data flag in one call.
 *
 * Returns: 0 on success, -1 when one of these conditions are met:
 *  - status is 0 or AGH_CMD_ANSWER_STATUS_UNKNOWN (not legal)
 *  - text is NULL
 *  - agh_cmd struct is NULL or contains a NULL agh_cmd_res pointer
 *  - restextparts queue is NULL (but we won't be running to check for this in case of a queue-related memory allocation failure)
*/
gint agh_cmd_answer_if_empty(struct agh_cmd *cmd, guint status, gchar *text, gboolean is_data) {
	gint retval;

	retval = 0;

	if (!cmd || !cmd->answer || !cmd->answer->restextparts || !text || !status || (status == AGH_CMD_ANSWER_STATUS_UNKNOWN)) {
		agh_log_cmd_crit("NULL agh_cmd struct or agh_cmd_res struct pointer inside agh_cmd struct, or missing restextparts? Crazy...");
		retval = -1;
		goto wayout;
	}

	if (!g_queue_get_length(cmd->answer->restextparts)) {
		/* Not checking - we check for all of these conditions in here */
		agh_cmd_answer_addtext(cmd, text, TRUE);

		/* true in this case, too */
		agh_cmd_answer_set_data(cmd, is_data);

		/* and this also */
		agh_cmd_answer_set_status(cmd, status);
	}

wayout:
	return retval;
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

/*
 * Utility function to report errors while processing a received command.
 *
 * Returns: an integer with value 0 on success, or
 *  - -40 when a NULL agh_cmd struct is passed, when the "answer" member of the struct is NULL, when the given status value is illegal (0 or AGH_CMD_ANSWER_STATUS_UNKNOWN) or the given text pointer is NULL.
*/
gint agh_cmd_op_answer_error(struct agh_cmd *cmd, guint status, gchar *text, gboolean dup) {
	gint retval;

	retval = 0;

	if (!cmd || !cmd->answer || !status || !text || (status == AGH_CMD_ANSWER_STATUS_UNKNOWN)) {
		agh_log_cmd_crit("NULL agh_cmd or agh_cmd_res struct NULL, illegal status value or NULL text given");
		retval = -40;
		goto wayout;
	}

	agh_cmd_answer_set_status(cmd, status);
	agh_cmd_answer_addtext(cmd, text, dup);

wayout:
	if (retval && !dup)
		g_free(text);

	return retval;
}

/*
 * Checks if the required number of arguments is present. Also fails on excess of arguments.
 * If the (*args_offset) is not NULL, we'll store here a number indicating how many arguments you miss, or how many excess arguments you have.
 * A negative number indicates missing arguments, a positive one indicates excess arguments.
 * The 0 value is stored here when an "acceptable" number of arguments is present.
 *
 * Returns: an integer of value 0 on success, or
 *  - -10 when an insufficient number of arguments was present,
 *  - -11 when too many arguments where given.
 *
 * This function makes use of g_assert.
*/
static gint agh_cmd_op_check(const struct agh_cmd_operation *op, struct agh_cmd *cmd, guint index, gint *args_offset) {
	gint retval;
	guint i;

	g_assert(cmd->cmd && op && args_offset && !*args_offset);

	i = index+1;
	retval = 0;

	while(agh_cmd_get_arg(cmd, i, CONFIG_TYPE_NONE))
		i++;

	i = i-index-1;

	if (i < op->min_args) {
		agh_log_cmd_dbg("got %" G_GUINT16_FORMAT" args but %" G_GUINT16_FORMAT" where needed",i,op->min_args);

		*args_offset = op->min_args-i;

		retval = -10;
		goto wayout;
	}

	if (i > op->max_args) {
		agh_log_cmd_dbg("got %" G_GUINT16_FORMAT" args (%" G_GUINT16_FORMAT" more than expected)",i,(i-op->max_args));

		*args_offset = op->max_args-i;

		retval = -11;
		goto wayout;
	}

wayout:
	return retval;
}

/*
 * Given an operation table, an agh_cmd struct and an index, this function will search for the requested operation.
 * When no errors are detected in the function, the agh_cmd_op_check one is called.
 * This function won't complain if no cmd_cb callback is present on a given operation entry.
 * In that case, the command is "answered" infact, reporting the issue.
 *
 * Returns: an integer value with value 0 on success;
 *  - -1: NULL operations vector or the passed agh_cmd struct is NULL, misses the required config_t pointer or has a not-NULL answer member; a NULL AGH state struct may cause this value to be returned as well, but it should not be possible to reach here
 *  - -2: unable to obtain operation argument, maybe specified index is out of range?
 *  - -3: unable to obtain operation text (at index 0)
 *  - -4: mo match
 *
 * Other (negative) error codes are directly from agh_cmd_op_check and functions it may invoke. Positive error codes with values less than 100 are from agh_cmd_answer_alloc.
 * Return codes greather than 100 are from callbacks.
 *
 * Note: this function may be executed recursively!
*/
gint agh_cmd_op_match(struct agh_state *mstate, const struct agh_cmd_operation *ops, struct agh_cmd *cmd, guint index) {
	gint retval;
	const struct agh_cmd_operation **current_op;
	const gchar *requested_op_text;
	config_setting_t *arg;
	gint args_needed;

	retval = 0;
	args_needed = 0;

	if (!ops || !cmd || !cmd->cmd || !mstate) {
		agh_log_cmd_crit("NULL operations vector or the passed agh_cmd struct is NULL (missing required config_t pointer). Or maybe we have a NULL AGH state?");
		retval = -1;
		goto wayout;
	}

	current_op = &ops;

	if (!index)
		requested_op_text = agh_cmd_get_operation(cmd);
	else {
		arg = agh_cmd_get_arg(cmd, index, CONFIG_TYPE_STRING);

		if (!arg) {
			agh_log_cmd_crit("unable to obtain argument at index=%" G_GUINT16_FORMAT" (index out of range?)",index);
			retval = -2;
			goto wayout;
		}

		requested_op_text = config_setting_get_string(arg);
	}

	if (!requested_op_text) {
		agh_log_cmd_crit("can not obtain operation text");
		retval = -3;
		goto wayout;
	}

	while ((*current_op)->op_name) {

		if (!g_strcmp0((*current_op)->op_name, requested_op_text))
			break;

		(*current_op)++;
	}

	if (!(*current_op)->op_name) {
		agh_log_cmd_dbg("no match while scanning for operation=%s (index=%" G_GUINT16_FORMAT")",requested_op_text,index);

		if (index)
			agh_cmd_op_answer_error(cmd, AGH_CMD_ANSWER_STATUS_FAIL, "INVALID_SUBCOMMAND", TRUE);

		retval = -4;
		goto wayout;
	}

	if (!cmd->answer) {
		retval = agh_cmd_answer_alloc(cmd);
		if (retval) {
			agh_log_cmd_crit("agh_cmd_answer_alloc failure (code=%" G_GINT16_FORMAT")",retval);
			goto wayout;
		}

		agh_cmd_answer_set_status(cmd, AGH_CMD_ANSWER_STATUS_FAIL);

	}

	/* check */
	if (agh_cmd_op_check(*current_op, cmd, index, &args_needed)) {
		agh_cmd_op_answer_error(cmd, AGH_CMD_ANSWER_STATUS_FAIL, g_strdup_printf("args=%" G_GINT16_FORMAT"", args_needed), FALSE);
		goto wayout;
	}

	if (!(*current_op)->cmd_cb) {
		agh_cmd_op_answer_error(cmd, AGH_CMD_ANSWER_STATUS_FAIL, "NO_CB", TRUE);
		goto wayout;
	}

	retval = (*current_op)->cmd_cb(mstate, cmd);
	if (retval && retval < 100)
		agh_log_cmd_dbg("a (*current_op)->cmd_cb function invaded our return values space; this may complicate troubleshooting");

wayout:
	return retval;
}
