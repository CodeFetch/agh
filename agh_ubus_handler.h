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
/* End of AGH_CMD_UBUS subcommands. */

/* AGH_CMD_UBUS_LISTEN subcommands */
#define AGH_CMD_UBUS_LISTEN_ADD "add"
#define AGH_CMD_UBUS_LISTEN_STOP "reset"
/* end of AGH_CMD_UBUS_LISTEN subcommands */

/* Errors. */
#define AGH_UBUS_INVALID_SUBCOMMAND "UBUS_INVALID_COMMAND"
#define AGH_UBUS_NO_CONNECTION "UBUS_NO_CONNECTION"
#define AGH_UBUS_MISSING_SUBCOMMAND "OK"
#define AGH_UBUS_NO_DATA "UBUS_NO_DATA"
#define AGH_UBUS_MISSING_PATH "MISSING_PATH"
#define AGH_UBUS_MISSING_METHOD "MISSING_METHOD"
#define AGH_UBUS_BBUF_INIT_ERROR "BBUF_INIT_ERROR"
#define AGH_UBUS_INVALID_JSON_MESSAGE "INVALID_JSON"
#define AGH_UBUS_METHOD_NOT_FOUND "METHOD_NOT_FOUND"
#define AGH_UBUS_HANDLER_EVENTS_NOT_ENABLED "EVENT_NOT_ENABLED"
#define AGH_UBUS_HANDLER_EVENTS_EVENT_REGISTRATION_FAILED "REG_FAILED"
#define AGH_UBUS_HANDLER_EVENTS_EVENT_REGISTRATION_OK "OK"
#define AGH_UBUS_HANDLER_EVENTS_EVENT_UNREGISTRATION_FAILED "unreg_failed"
#define AGH_UBUS_HANDLER_EVENTS_EVENT_UNREGISTRATION_OK "unreg_OK"
#define AGH_UBUS_HANDLER_EVENTS_UNKNOWN_SUBCOMMAND AGH_UBUS_INVALID_SUBCOMMAND
/* End of errors. */

/* ubus events event name */
#define AGH_UBUS_HANDLER_UBUS_EVENTs_NAME "UBUS_EVENT"

gpointer agh_core_ubus_cmd_handle(gpointer data, gpointer hmessage);

void agh_ubus_list(struct agh_ubus_ctx *uctx, struct command *cmd);
void agh_ubus_list_receive_results(struct ubus_context *ctx, struct ubus_object_data *obj, gpointer data);

void agh_ubus_call(struct agh_ubus_ctx *uctx, struct command *cmd);
void agh_ubus_listen(struct agh_ubus_ctx *uctx, struct command *cmd);
void agh_ubus_handler_receive_event(struct ubus_context *ctx, struct ubus_event_handler *ev, const char *type, struct blob_attr *msg);

#endif
