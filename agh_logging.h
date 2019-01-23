#ifndef __agh_logging_h__
#define __agh_logging_h__

#define agh_log(log_domain, log_level, message) \
	g_log_structured(log_domain, log_level, "CODE_FILE", __FILE__, "CODE_LINE", __LINE__, "MESSAGE", message);

#define agh_log_dbg(log_domain, message) agh_log(log_domain, G_LOG_LEVEL_DEBUG, message)
#define agh_log_info(log_domain, message) agh_log(log_domain, G_LOG_LEVEL_INFO, message)

GLogWriterOutput
g_agh_log_writer (GLogLevelFlags   log_level,
                      const GLogField *fields,
                      gsize            n_fields,
                      gpointer         user_data);

#endif
