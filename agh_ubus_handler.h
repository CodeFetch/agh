#ifndef __agh_ubus_handler_H__
#define __agh_ubus_handler_H__
#include "libubus.h"

/* Operations. */
#define AGH_CMD_UBUS "ubus"
/* End of operations. */

/* AGH_CMD_UBUS subcommands. */
#define AGH_CMD_UBUS_LIST "list"
#define AGH_CMD_UBUS_CALL "call"
#define AGH_CMD_UBUS_LISTEN "events"
#define AGH_CMD_UBUS_LOGSTREAM "logstream"
/* End of AGH_CMD_UBUS subcommands. */

/* AGH_CMD_UBUS_LISTEN subcommands */
#define AGH_CMD_UBUS_LISTEN_ADD "add"
#define AGH_CMD_UBUS_LISTEN_STOP "reset"
/* end of AGH_CMD_UBUS_LISTEN subcommands */

/* AGH_CMD_UBUS_LOGSTREAM subcommands. */
#define AGH_CMD_UBUS_LOGSTREAM_ACTIVATE "+"
#define AGH_CMD_UBUS_LOGSTREAM_DEACTIVATE "-"
/* End of AGH_CMD_UBUS_LOGSTREAM subcommands. */

/* Errors. */
#define AGH_UBUS_HANDLER_INVALID_SUBCOMMAND "INVALID_SUBCOMMAND"
#define AGH_UBUS_HANDLER_NO_CONNECTION "NO_CONNECTION"
#define AGH_UBUS_HANDLER_MISSING_SUBCOMMAND "MISSING_SUBCOMMAND"
#define AGH_UBUS_HANDLER_NO_DATA "NO_DATA"
#define AGH_UBUS_HANDLER_MISSING_PATH "MISSING_PATH"
#define AGH_UBUS_HANDLER_MISSING_METHOD "MISSING_METHOD"
#define AGH_UBUS_HANDLER_BBUF_INIT_ERROR "UBUS_BBUF_INIT_ERROR"
#define AGH_UBUS_HANDLER_INVALID_JSON_MESSAGE "UBUS_INVALID_JSON"
#define AGH_UBUS_HANDLER_METHOD_NOT_FOUND "UBUS_METHOD_NOT_FOUND"
#define AGH_UBUS_HANDLER_EVENTS_NOT_ENABLED "EVENT_REPORTING_NOT_ENABLED"
#define AGH_UBUS_HANDLER_EVENTS_EVENT_REGISTRATION_FAILED "REG_FAILED"
#define AGH_UBUS_HANDLER_EVENTS_EVENT_REGISTRATION_OK "OK"
#define AGH_UBUS_HANDLER_EVENTS_EVENT_UNREGISTRATION_FAILED "UNREG_FAILED"
#define AGH_UBUS_HANDLER_EVENTS_EVENT_UNREGISTRATION_OK "UNREG_OK"
#define AGH_UBUS_HANDLER_EVENTS_UNKNOWN_SUBCOMMAND AGH_UBUS_HANDLER_INVALID_SUBCOMMAND
#define AGH_UBUS_HANDLER_LOGSTREAM_MISSING_SUBCOMMAND "MISSING_SUBCOMMAND"
#define AGH_UBUS_HANDLER_LOGSTREAM_INVALID_SUBCOMMAND AGH_UBUS_HANDLER_INVALID_SUBCOMMAND
#define AGH_UBUS_HANDLER_LOGSTREAM_INTERNAL_ERROR "INTERNAL_ERROR"
#define AGH_UBUS_HANDLER_LOGSTREAM_ALREADY_ACTIVE "ALREADY_ACTIVE"
#define AGH_UBUS_HANDLER_LOGSTREAM_ALREADY_DEACTIVATED "ALREADY_INACTIVE"
#define AGH_UBUS_HANDLER_LOGSTREAM_OK "OK"
/* End of errors. */

/* ubus events event name */
#define AGH_UBUS_HANDLER_UBUS_EVENTs_NAME "UBUS_EVENT"

gpointer agh_core_ubus_cmd_handle(gpointer data, gpointer hmessage);

void agh_ubus_handler_list(struct agh_ubus_ctx *uctx, struct command *cmd);
void agh_ubus_handler_call(struct agh_ubus_ctx *uctx, struct command *cmd);
void agh_ubus_handler_listen(struct agh_ubus_ctx *uctx, struct command *cmd);
void agh_ubus_handler_receive_event(struct ubus_context *ctx, struct ubus_event_handler *ev, const char *type, struct blob_attr *msg);
void agh_ubus_handler_logstream(struct agh_ubus_ctx *uctx, struct command *cmd);

#endif
