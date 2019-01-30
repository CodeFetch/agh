#include <string.h>
#include <libconfig.h>
#include "agh_commands.h"
#include "agh_messages.h"
#include "agh_logging.h"

/* Log messages from AGH_LOG_DOMAIN_COMMANDS domain. */
#define AGH_LOG_DOMAIN_COMMAND	"COMMAND"
#define agh_log_cmd_dbg(message, ...) agh_log_dbg(AGH_LOG_DOMAIN_COMMAND, message, ##__VA_ARGS__)
#define agh_log_comm_crit(message, ...) agh_log_crit(AGH_LOG_DOMAIN_COMMAND, message, ##__VA_ARGS__)

/* Function prototypes. */
static gchar *cmd_answer_to_text(struct command *cmd);
static config_t *cmd_copy_cfg(config_t *src);
static config_setting_t *cmd_get_in_keyword_setting(struct command *cmd);
static gint cmd_get_id(struct command *cmd) __attribute__((unused));
static void print_config_type(gint type) __attribute__((unused));

/* And some useful functions to access events */
static const gchar *event_name(struct command *cmd) __attribute__((unused));
static const gchar *event_arg(struct command *cmd, guint arg_index) __attribute__((unused));

void cmd_answer_set_status(struct command *cmd, guint status) {
	cmd->answer->status = status;
	return;
}

guint cmd_answer_get_status(struct command *cmd) {
	return cmd->answer->status;
}

guint cmd_answer_addtext(struct command *cmd, const gchar *text) {
	guint retval;

	retval = 0;

	if (text)
		g_queue_push_tail(cmd->answer->restextparts, g_strdup(text));
	else
		retval = 1;

	return retval;
}

guint cmd_answer_peektext(struct command *cmd, gchar *text) {
	guint retval;

	retval = 0;

	if (text)
		g_queue_push_tail(cmd->answer->restextparts, text);
	else
		retval = 1;

	return retval;
}

/*
 * This function transforms a command_result structure content to text. It is destructive, and infact it also deallocates the
 * structure. Yeah, this is arguable design.
*/
static gchar *cmd_answer_to_text(struct command *cmd) {
	GString *output;
	guint ntextparts;
	guint i;
	gchar *current_textpart;

	ntextparts = 0;

	if (!cmd)
		return NULL;

	/* Start with the OUT keyword */
	output = g_string_new(CMD_OUT_KEYWORD" = ( ");

	/* Appends command ID,and status code, adding at last a comma and a space to keep the structure consistent when later appending text parts. */
	g_string_append_printf(output, "%" G_GINT16_FORMAT", %" G_GUINT16_FORMAT"", config_setting_get_int(config_setting_get_elem(config_lookup(cmd->cmd, CMD_IN_KEYWORD), 0)), cmd->answer->status);

	/* We are going to process the restextparts queue now: it's guaranteed to be not NULL, but it may contain 0 items. */
	ntextparts = g_queue_get_length(cmd->answer->restextparts);

	if (!ntextparts) {
		g_queue_push_tail(cmd->answer->restextparts, g_strdup(BUG_EMPTY_ANSWER_TEXT));
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

	/* Yeah, probably useless. */
	cmd->answer->status = CMD_ANSWER_STATUS_UNKNOWN;
	cmd->answer->restextparts = NULL;

	g_free(cmd->answer);
	cmd->answer = NULL;

	return g_string_free(output, FALSE);
}

guint cmd_answer_prepare(struct command *cmd) {
	guint retval;

	retval = 0;

	if (!cmd) {
		g_print("%s: can not prepare an answer to a NULL command\n",__FUNCTION__);
		retval = 1;
	}
	else
	{
		cmd->answer = g_malloc0(sizeof(struct command_result));

		cmd->answer->status = CMD_ANSWER_STATUS_UNKNOWN;
		cmd->answer->restextparts = g_queue_new();
	}

	return retval;
}

/*
 * Frees an AGH command structure.
 *
 * Returns: an integer with value 0 on success
 * -10 when command structure is NULL
 * -11 when a command answer structure was present, but no restextparts queue
*/
gint agh_cmd_free(struct command *cmd) {
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
		cmd->answer->status = CMD_ANSWER_STATUS_UNKNOWN;

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

struct command *cmd_copy(struct command *cmd) {
	struct command *ocmd;
	config_t *cfg;
	struct command_result *cmd_answer;

	ocmd = g_malloc0(sizeof(struct command));
	cfg = NULL;
	cmd_answer = NULL;

	cfg = cmd_copy_cfg(cmd->cmd);
	if (cfg) {
		ocmd->cmd = cfg;
	}

	if (cmd->answer) {
		/* Anser processing. */
		cmd_answer = g_malloc0(sizeof(struct command_result));
		cmd_answer->status = cmd->answer->status;

		cmd_answer->restextparts = g_queue_new();

		g_queue_foreach(cmd->answer->restextparts, agh_copy_textparts, cmd_answer->restextparts);

		cmd_answer->is_data = cmd->answer->is_data;
		ocmd->answer = cmd_answer;
	}

	if ((!ocmd->cmd) && (!ocmd->answer)) {
		agh_cmd_free(ocmd);
		g_print("\ncmd_copy: no cmd_cfg or answer, discarding structure.\n");
		ocmd = NULL;
	}
	else {
		if (cmd->cmd_source_id)
			ocmd->cmd_source_id = g_strdup(cmd->cmd_source_id);
	}

	return ocmd;
}

/*
 * This function has been written to cope with known "valid" config_t command structures, as checked in the text_to_cmd()
 * function. Other config_t structures will not be handled correctly. Any better way to do this is apreciated.
*/
static config_t *cmd_copy_cfg(config_t *src) {
	config_t *ncfg;
	config_setting_t *root_setting;
	config_setting_t *list_setting;
	guint index;
	config_setting_t *elem;
	config_setting_t *src_in_keyword;
	config_setting_t *current_setting;
	gint current_setting_type;

	ncfg = NULL;
	root_setting = NULL;
	list_setting = NULL;
	index = 0;
	elem = NULL;
	src_in_keyword = NULL;
	current_setting = NULL;
	current_setting_type = 0;

	if (!src)
		return NULL;

	ncfg = g_malloc0(sizeof(config_t));

	config_init(ncfg);

	/* 1 - Get root setting. We're guaranteed there is one. */
	root_setting = config_root_setting(ncfg);

	/* 2 - Add our CMD_IN_KEYWORD list setting. */
	list_setting = config_setting_add(root_setting, CMD_IN_KEYWORD, CONFIG_TYPE_LIST);

	if (!list_setting) {
		goto wayout;
	}

	/* 3 - Add things */
	src_in_keyword = config_lookup(src, CMD_IN_KEYWORD);

	while ( (elem = config_setting_get_elem(src_in_keyword, index)) ) {
		current_setting_type = config_setting_type(elem);
		current_setting = config_setting_add(list_setting, NULL, current_setting_type);

		/*
		 * Given the fact we check for settings types before writing them to new ones, we are not checking the final result(s).
		 * Infact, both config_setting_set_* and their config_setting_get_* counterparts, return useful error values.
		 * Should components receive zero values unexpectedly, checking what's going on here could be a good idea. This place may
		 * also be more subject than others to effects due to changes in libconfig.
		*/
		switch(current_setting_type) {
			case CONFIG_TYPE_INT:
				config_setting_set_int(current_setting, config_setting_get_int(elem));
				break;
			case CONFIG_TYPE_INT64:
				config_setting_set_int64(current_setting, config_setting_get_int64(elem));
				break;
			case CONFIG_TYPE_FLOAT:
				config_setting_set_float(current_setting, config_setting_get_float(elem));
				break;
			case CONFIG_TYPE_STRING:
				config_setting_set_string(current_setting, config_setting_get_string(elem));
				break;
			case CONFIG_TYPE_BOOL:
				config_setting_set_bool(current_setting, config_setting_get_bool(elem));
				break;
			default:
				g_print("Unsupported (or unknown) setting type while processing command config structure.\n");
				goto wayout;
		}

		index++;
	}

	return ncfg;

wayout:
	config_destroy(ncfg);
	g_free(ncfg);
	ncfg = NULL;
	return ncfg;
}

struct agh_message *cmd_answer_msg(struct command *cmd, struct agh_comm *src_comm, struct agh_comm *dest_comm) {
	struct agh_message *m;
	struct text_csp *textcsp;

	m = NULL;
	textcsp = g_malloc0(sizeof(struct text_csp));

	textcsp->text = cmd_answer_to_text(cmd);

	if (!textcsp->text) {
		g_free(textcsp);
		return m;
	}

	if (cmd->cmd_source_id)
		textcsp->source_id = g_strdup(cmd->cmd_source_id);

	m = agh_msg_alloc();
	m->csp = textcsp;
	m->msg_type = MSG_SENDTEXT;
	m->src = src_comm;
	m->dest = dest_comm;

	return m;
}

static config_setting_t *cmd_get_in_keyword_setting(struct command *cmd) {

	if (!cmd)
		return NULL;

	/* Should something go wrong, this returns NULL as well. */
	return config_lookup(cmd->cmd, CMD_IN_KEYWORD);
}

static gint cmd_get_id(struct command *cmd) {
	config_setting_t *in_keyword;
	gint id;

	in_keyword = NULL;
	id = 0;

	if (!cmd)
		return id;

	if (! (in_keyword = cmd_get_in_keyword_setting(cmd)) )
		return id;

	id = config_setting_get_int(config_setting_get_elem(in_keyword, 0));
	return id;
}

const gchar *cmd_get_operation(struct command *cmd) {
	config_setting_t *in_keyword;
	const gchar *operation;

	in_keyword = NULL;
	operation = NULL;

	if (!cmd)
		return operation;

	if (! (in_keyword = cmd_get_in_keyword_setting(cmd)) )
		return operation;

	operation = config_setting_get_string(config_setting_get_elem(in_keyword, 1));
	return operation;
}

config_setting_t *cmd_get_arg(struct command *cmd, guint arg_index, gint config_type) {
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

	if (! (in_keyword = cmd_get_in_keyword_setting(cmd)) )
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

static void print_config_type(gint type) {
	switch(type) {
		case CONFIG_TYPE_INT:
			g_print("CONFIG_TYPE_INT");
			break;
		case CONFIG_TYPE_INT64:
			g_print("CONFIG_TYPE_INT64");
			break;
		case CONFIG_TYPE_FLOAT:
			g_print("CONFIG_TYPE_FLOAT");
			break;
		case CONFIG_TYPE_STRING:
			g_print("CONFIG_TYPE_STRING");
			break;
		case CONFIG_TYPE_BOOL:
			g_print("CONFIG_TYPE_BOOL");
			break;
		default:
			g_print("Unsupported (or unknown) setting type.\n");
			break;
	}
	return;
}

struct command *cmd_event_prepare(void) {
	struct command *cmd;

	cmd = NULL;

	cmd = g_malloc0(sizeof(struct command));

	cmd->answer = g_malloc0(sizeof(struct command_result));

	cmd->answer->status = CMD_EVENT_UNKNOWN_ID;
	cmd->answer->restextparts = g_queue_new();

	return cmd;
}

/*
 * This function transforms an event's command_result structure content to text. It is destructive, and infact it also
 * deallocates the command_result structure contained in the command used as event. Yeah, this is arguable design. A lot of code here is in common with the cmd_answer_to_text
 * function, and infact it has been copied from there. Maybe unifying those function is a good idea.
*/
gchar *cmd_event_to_text(struct command *cmd, gint event_id) {
	GString *output;
	guint ntextparts;
	guint i;
	gchar *current_textpart;

	ntextparts = 0;
	current_textpart = NULL;

	if (!cmd)
		return NULL;

	/* Start with the EVENT keyword */
	output = g_string_new(CMD_EVENT_KEYWORD" = ( ");

	/* Appends event ID, adding at last a comma and a space to keep the structure consistent when later appending text parts. */
	g_string_append_printf(output, "%" G_GINT16_FORMAT", %" G_GUINT16_FORMAT"", event_id, cmd->answer->status);

	/* We are going to process the restextparts queue now: it's guaranteed to be not NULL, but it may contain 0 items. */
	ntextparts = g_queue_get_length(cmd->answer->restextparts);

	if (!ntextparts) {
		g_queue_push_tail(cmd->answer->restextparts, g_strdup(BUG_EMPTY_EVENT_NAME));
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
	* See cmd_answer_to_text for clarification.
	*/
	g_queue_free(cmd->answer->restextparts);

	/* Yeah, probably useless. */
	cmd->answer->status = CMD_EVENT_UNKNOWN_ID;
	cmd->answer->restextparts = NULL;

	g_free(cmd->answer);
	cmd->answer = NULL;

	return g_string_free(output, FALSE);
}

void cmd_emit_event(struct agh_comm *agh_core_comm, struct command *cmd) {
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
static const gchar *event_name(struct command *cmd) {
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
static const gchar *event_arg(struct command *cmd, guint arg_index) {
	const gchar *arg;

	arg = NULL;

	if (!arg_index)
		return arg;

	arg = g_queue_peek_nth(cmd->answer->restextparts, arg_index);

	return arg;
}

void cmd_answer_set_data(struct command *cmd, gboolean is_data) {
	cmd->answer->is_data = is_data;
	return;
}

void cmd_answer_if_empty(struct command *cmd, guint status, gchar *text, gboolean set_is_data) {
	if (!cmd || !text)
		return;

	if (!g_queue_get_length(cmd->answer->restextparts)) {
		cmd_answer_addtext(cmd, text);
		cmd->answer->is_data = set_is_data;
		cmd_answer_set_status(cmd, status);
	}

	return;
}

struct command *text_to_cmd(gchar *from, gchar *content) {
	struct command *ocmd;
	gchar *atext;
	config_t *cmd_cfg;
	gint cmd_id;
	config_setting_t *in_keyword;
	config_setting_t *id;
	config_setting_t *op;
	const gchar *cmd_operation;
	guint lengths;
	gchar *afrom;

	ocmd = NULL;
	atext = NULL;
	in_keyword = NULL;
	cmd_id = 0;
	id = NULL;
	op = NULL;
	cmd_operation = NULL;
	lengths = 0;
	afrom = NULL;

	lengths = strlen(content);
	if (lengths > CMD_MAX_TEXT_LEN) {
		g_print("CMD_MAX_TEXT_LEN exceeded.\n");
		return ocmd;
	}

	/* We are not checking for errors, since GLib guarantees us we'll not survive an allocation failure by default. This needs
	 * to be reviewed of course.
	*/
	cmd_cfg = g_malloc0(sizeof(config_t));

	config_init(cmd_cfg);

	/* Convert given input to ascii, just in case. */
	atext = g_str_to_ascii(content, "C");

	if (!config_read_string(cmd_cfg, atext)) {
		/* Invalid input. */
		g_print("Invalid input.\n");
		goto wayout;
	}

	/*
	 * A command should clearly repsect the libconfig configuration grammar. In our context, it should be formed of the
	 * following elements:
	 *
	 * - the CMD_IN_KEYWORD keyword / setting
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
		g_print("Excess data.\n");
		goto wayout;
	}

	/* 2 - CMD_IN_KEYWORD keyword */
	in_keyword = config_lookup(cmd_cfg, CMD_IN_KEYWORD);

	if (!in_keyword) {
		g_print(CMD_IN_KEYWORD" keyword not detected. Not processing.\n");
		goto wayout;
	}

	/* 3 - The CMD_IN_KEYWORD setting should be a list, and contain at minimum of 2 keywords. */
	if (!config_setting_is_list(in_keyword)) {
		g_print("Unexpected command structure.\n");
		goto wayout;
	}

	if (config_setting_length(in_keyword) < 2) {
		g_print("At least an operation and a command ID are required.\n");
		goto wayout;
	}

	/* 4 - Command ID, should be gint and != 0. */
	id = config_setting_get_elem(in_keyword, 0);
	cmd_id = config_setting_get_int(id);
	if (cmd_id < 1) {
		g_print("Invalid command ID.\n");
		goto wayout;
	}

	/* 5 - Operation should not be an empty string. */
	op = config_setting_get_elem(in_keyword, 1);
	cmd_operation = config_setting_get_string(op);
	if (!cmd_operation) {
		g_print("Operation can not be valid.\n");
		goto wayout;
	}

	/* 6 - Operation name should consist at least of a single character. */
	lengths = strlen(cmd_operation);
	if (!lengths) {
		g_print("An operation name should consist at least of a single character.\n");
		goto wayout;
	}

	/* 7 - Operation name may consist of CMD_MAX_OP_NAME_LEN characters at most. */
	if (lengths > CMD_MAX_OP_NAME_LEN) {
		g_print("CMD_MAX_OP_NAME_LEN exceeded.\n");
		goto wayout;
	}

	if (from) {
		afrom = g_str_to_ascii(from, "C");

		lengths = strlen(afrom);

		if (lengths < 1 || lengths > CMD_MAX_FROM_LEN) {
			g_print("Invalid source identifier.\n");
			g_free(afrom);
			afrom = NULL;
			goto wayout;
		}

	}

	g_print("OK.\n");

	ocmd = g_malloc0(sizeof(struct command));
	ocmd->cmd = cmd_cfg;

	if (afrom)
		ocmd->cmd_source_id = afrom;

	/* Makes me feel more peaceful, but it's useless. */
	ocmd->answer = NULL;

	g_free(atext);
	return ocmd;

wayout:
	g_free(atext);
	config_destroy(cmd_cfg);
	g_free(cmd_cfg);
	cmd_cfg = NULL;
	return ocmd;
}
