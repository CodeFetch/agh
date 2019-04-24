/*
 * A lot of coe in here has been copied from GLib itself, under the terms of the LGPL. We wanted to preserve some useful
 * functionalities, like G_MESSAGES_DEBUG environment variable processing.
 * What we are interested is mainly "overriding" g_log_writer_standard_streams with a function of our own. This would allow
 * for saving some code and maintenance.
 *
 * Note that the code in here may be pesent in a modified form: any bugs should be considered as introduced by me.
 *
 * Changes include:
 *  - removed systemd journal support
*/

#include <glib.h>
#include <stdio.h>
#include <syslog.h>

#include "agh_logging.h"

#define LOCAL_GLIB_FORMAT_UNSIGNED_BUFSIZE ((GLIB_SIZEOF_LONG * 3) + 3)
#define LOCAL_GLIB_STRING_BUFFER_SIZE (LOCAL_GLIB_FORMAT_UNSIGNED_BUFSIZE + 32)
#define LOCAL_DEFAULT_LEVELS (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING | G_LOG_LEVEL_MESSAGE)
#define LOCAL_INFO_LEVELS (G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG)
#define LOCAL_ALERT_LEVELS    (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING)
#define LOCAL_CHAR_IS_SAFE(wc) (!((wc < 0x20 && wc != '\t' && wc != '\n' && wc != '\r') || \
			    (wc == 0x7f) || \
			    (wc >= 0x80 && wc < 0xa0)))

static int glib_to_syslog_priority (GLogLevelFlags level) {
	switch (level) {
		case G_LOG_LEVEL_ERROR:
			return LOG_CRIT;
		case G_LOG_LEVEL_CRITICAL:
			return LOG_ERR;
		case G_LOG_LEVEL_WARNING:
			return LOG_WARNING;
		case G_LOG_LEVEL_MESSAGE:
			return LOG_NOTICE;
		case G_LOG_LEVEL_DEBUG:
			return LOG_DEBUG;
		default:
			return LOG_INFO;
	}
}

static const gchar *
log_level_to_color (GLogLevelFlags log_level,
                    gboolean       use_color)
{
  /* we may not call _any_ GLib functions here */

  if (!use_color)
    return "";

  if (log_level & G_LOG_LEVEL_ERROR)
    return "\033[1;31m"; /* red */
  else if (log_level & G_LOG_LEVEL_CRITICAL)
    return "\033[1;35m"; /* magenta */
  else if (log_level & G_LOG_LEVEL_WARNING)
    return "\033[1;33m"; /* yellow */
  else if (log_level & G_LOG_LEVEL_MESSAGE)
    return "\033[1;32m"; /* green */
  else if (log_level & G_LOG_LEVEL_INFO)
    return "\033[1;32m"; /* green */
  else if (log_level & G_LOG_LEVEL_DEBUG)
    return "\033[1;32m"; /* green */

  /* No color for custom log levels. */
  return "";
}

/* GLib comment:
 * For a radix of 8 we need at most 3 output bytes for 1 input
 * byte. Additionally we might need up to 2 output bytes for the
 * readix prefix and 1 byte for the trailing NULL.
 */
#define LOCAL_FORMAT_UNSIGNED_BUFSIZE ((GLIB_SIZEOF_LONG * 3) + 3)

static void
format_unsigned (gchar  *buf,
		 gulong  num,
		 guint   radix)
{
  gulong tmp;
  gchar c;
  gint i, n;

  /* we may not call _any_ GLib functions here (or macros like g_return_if_fail()) */

  if (radix != 8 && radix != 10 && radix != 16)
    {
      *buf = '\000';
      return;
    }
  
  if (!num)
    {
      *buf++ = '0';
      *buf = '\000';
      return;
    } 
  
  if (radix == 16)
    {
      *buf++ = '0';
      *buf++ = 'x';
    }
  else if (radix == 8)
    {
      *buf++ = '0';
    }
	
  n = 0;
  tmp = num;
  while (tmp)
    {
      tmp /= radix;
      n++;
    }

  i = n;

  /* Again we can't use g_assert; actually this check should _never_ fail. */
  if (n > LOCAL_FORMAT_UNSIGNED_BUFSIZE - 3)
    {
      *buf = '\000';
      return;
    }

  while (num)
    {
      i--;
      c = (num % radix);
      if (c < 10)
	buf[i] = c + '0';
      else
	buf[i] = c + 'a' - 10;
      num /= radix;
    }
  
  buf[n] = '\000';
}

static const gchar *
color_reset (gboolean use_color)
{
  /* we may not call _any_ GLib functions here */

  if (!use_color)
    return "";

  return "\033[0m";
}

static FILE *
mklevel_prefix (gchar          level_prefix[LOCAL_GLIB_STRING_BUFFER_SIZE],
                GLogLevelFlags log_level,
                gboolean       use_color)
{
  gboolean to_stdout = TRUE;

  /* we may not call _any_ GLib functions here */

  strcpy (level_prefix, log_level_to_color (log_level, use_color));

  switch (log_level & G_LOG_LEVEL_MASK)
    {
    case G_LOG_LEVEL_ERROR:
      strcat (level_prefix, "ERROR");
      to_stdout = FALSE;
      break;
    case G_LOG_LEVEL_CRITICAL:
      strcat (level_prefix, "CRITICAL");
      to_stdout = FALSE;
      break;
    case G_LOG_LEVEL_WARNING:
      strcat (level_prefix, "WARNING");
      to_stdout = FALSE;
      break;
    case G_LOG_LEVEL_MESSAGE:
      strcat (level_prefix, "Message");
      to_stdout = FALSE;
      break;
    case G_LOG_LEVEL_INFO:
      strcat (level_prefix, "INFO");
      break;
    case G_LOG_LEVEL_DEBUG:
      strcat (level_prefix, "DEBUG");
      break;
    default:
      if (log_level)
	{
	  strcat (level_prefix, "LOG-");
	  format_unsigned (level_prefix + 4, log_level & G_LOG_LEVEL_MASK, 16);
	}
      else
	strcat (level_prefix, "LOG");
      break;
    }

  strcat (level_prefix, color_reset (use_color));

  if (log_level & G_LOG_FLAG_RECURSION)
    strcat (level_prefix, " (recursed)");
  if (log_level & LOCAL_ALERT_LEVELS)
    strcat (level_prefix, " **");

  return to_stdout ? stdout : stderr;
}

static gchar*
strdup_convert (const gchar *string,
		const gchar *charset)
{
  if (!g_utf8_validate (string, -1, NULL))
    {
      GString *gstring = g_string_new ("[Invalid UTF-8] ");
      guchar *p;

      for (p = (guchar *)string; *p; p++)
	{
	  if (LOCAL_CHAR_IS_SAFE(*p) &&
	      !(*p == '\r' && *(p + 1) != '\n') &&
	      *p < 0x80)
	    g_string_append_c (gstring, *p);
	  else
	    g_string_append_printf (gstring, "\\x%02x", (guint)(guchar)*p);
	}
      
      return g_string_free (gstring, FALSE);
    }
  else
    {
      GError *err = NULL;
      
      gchar *result = g_convert_with_fallback (string, -1, charset, "UTF-8", "?", NULL, NULL, &err);
      if (result)
	return result;
      else
	{
	  /* Not thread-safe, but doesn't matter if we print the warning twice
	   */
	  static gboolean warned = FALSE; 
	  if (!warned)
	    {
	      warned = TRUE;
	      fprintf (stderr, "GLib: Cannot convert message: %s\n", err->message);
	    }
	  g_error_free (err);
	  
	  return g_strdup (string);
	}
    }
}

static void
escape_string (GString *string)
{
  const char *p = string->str;
  gunichar wc;

  while (p < string->str + string->len)
    {
      gboolean safe;
	    
      wc = g_utf8_get_char_validated (p, -1);
      if (wc == (gunichar)-1 || wc == (gunichar)-2)  
	{
	  gchar *tmp;
	  guint pos;

	  pos = p - string->str;

	  /* Emit invalid UTF-8 as hex escapes 
           */
	  tmp = g_strdup_printf ("\\x%02x", (guint)(guchar)*p);
	  g_string_erase (string, pos, 1);
	  g_string_insert (string, pos, tmp);

	  p = string->str + (pos + 4); /* Skip over escape sequence */

	  g_free (tmp);
	  continue;
	}
      if (wc == '\r')
	{
	  safe = *(p + 1) == '\n';
	}
      else
	{
	  safe = LOCAL_CHAR_IS_SAFE (wc);
	}
      
      if (!safe)
	{
	  gchar *tmp;
	  guint pos;

	  pos = p - string->str;
	  
	  /* Largest char we escape is 0x0a, so we don't have to worry
	   * about 8-digit \Uxxxxyyyy
	   */
	  tmp = g_strdup_printf ("\\u%04x", wc); 
	  g_string_erase (string, pos, g_utf8_next_char (p) - p);
	  g_string_insert (string, pos, tmp);
	  g_free (tmp);

	  p = string->str + (pos + 6); /* Skip over escape sequence */
	}
      else
	p = g_utf8_next_char (p);
    }
}

static gchar *
agh_g_log_writer_format_fields (GLogLevelFlags   log_level,
                            const GLogField *fields,
                            gsize            n_fields,
                            gboolean         use_color)
{
  gsize i;
  const gchar *message = NULL;
  const gchar *log_domain = NULL;
  const gchar *filename = NULL;
  gint line_num = 0;
  gchar level_prefix[LOCAL_GLIB_STRING_BUFFER_SIZE];
  GString *gstring;
  gint64 now;
  time_t now_secs;
  struct tm *now_tm;
  gchar time_buf[128];

  /* Extract some fields. */
  for (i = 0; (message == NULL || log_domain == NULL || filename == NULL || !line_num) && i < n_fields; i++)
    {
      const GLogField *field = &fields[i];

      if (g_strcmp0 (field->key, "MESSAGE") == 0)
        message = field->value;
      if (g_strcmp0 (field->key, "GLIB_DOMAIN") == 0)
        log_domain = field->value;
      if (g_strcmp0 (field->key, "CODE_FILE") == 0)
      	filename = field->value;
      if (g_strcmp0 (field->key, "CODE_LINE") == 0)
      	line_num = (gint)field->value;
    }

  /* Format things. */
  mklevel_prefix (level_prefix, log_level, use_color);

  gstring = g_string_new (NULL);
  if (log_level & LOCAL_ALERT_LEVELS)
    g_string_append (gstring, "\n");
  if (!log_domain)
    g_string_append (gstring, "!**! ");

  if (log_domain != NULL)
    {
      g_string_append_printf (gstring, "!%s! ",log_domain);
    }
  g_string_append_c (gstring, '[');
  g_string_append (gstring, level_prefix);

  g_string_append (gstring, "] ");

  /* Timestamp */
  now = g_get_real_time ();
  now_secs = (time_t) (now / 1000000);
  now_tm = localtime (&now_secs);
  strftime (time_buf, sizeof (time_buf), "%H:%M:%S", now_tm);

  g_string_append_printf (gstring, "%s%s.%03d%s @ ",
                          use_color ? "\033[34m" : "",
                          time_buf, (gint) ((now / 1000) % 1000),
                          color_reset (use_color));

  if (filename == NULL) {
  	g_string_append (gstring, "(*unknown file*)");
  }
  else
  {
  	g_string_append (gstring, filename);
  }

  if (line_num)
  	g_string_append_printf (gstring, ":%" G_GINT16_FORMAT" ",line_num);
  else
  	g_string_append (gstring, ":*unknown line* ");

  if (message == NULL)
    {
      g_string_append (gstring, ":-) (NULL) message");
    }
  else
    {
      GString *msg;
      const gchar *charset;

      msg = g_string_new (message);
      escape_string (msg);

      if (g_get_charset (&charset))
        {
          /* charset is UTF-8 already */
          g_string_append_printf (gstring, ":-) %s", msg->str);
        }
      else
        {
          gchar *lstring = strdup_convert (msg->str, charset);
          g_string_append_printf (gstring, ":-) %s", lstring);
          g_free (lstring);
        }

      g_string_free (msg, TRUE);
    }

  return g_string_free (gstring, FALSE);
}

static GLogWriterOutput
agh_g_log_writer_syslog (GLogLevelFlags   log_level,
                               const GLogField *fields,
                               gsize            n_fields,
                               gpointer         user_data __attribute__((unused)) )
{
  gchar *out = NULL;  /* in the current locale’s character set */

  g_return_val_if_fail (fields != NULL, G_LOG_WRITER_UNHANDLED);
  g_return_val_if_fail (n_fields > 0, G_LOG_WRITER_UNHANDLED);

  out = agh_g_log_writer_format_fields (log_level, fields, n_fields, FALSE);
  syslog(glib_to_syslog_priority(log_level), "%s", out);
  g_free (out);

  return G_LOG_WRITER_HANDLED;
}

GLogWriterOutput
agh_g_log_writer (GLogLevelFlags   log_level,
                      const GLogField *fields,
                      gsize            n_fields,
                      gpointer         user_data)
{
  g_return_val_if_fail (fields != NULL, G_LOG_WRITER_UNHANDLED);
  g_return_val_if_fail (n_fields > 0, G_LOG_WRITER_UNHANDLED);

  /* Disable debug message output unless specified in G_MESSAGES_DEBUG. */
  if (!(log_level & LOCAL_DEFAULT_LEVELS) && !(log_level >> G_LOG_LEVEL_USER_SHIFT))
    {
      const gchar *domains, *log_domain = NULL;
      gsize i;

      domains = g_getenv ("G_MESSAGES_DEBUG");

      if ((log_level & LOCAL_INFO_LEVELS) == 0 ||
          domains == NULL)
        return G_LOG_WRITER_HANDLED;

      for (i = 0; i < n_fields; i++)
        {
          if (g_strcmp0 (fields[i].key, "GLIB_DOMAIN") == 0)
            {
              log_domain = fields[i].value;
              break;
            }
        }

      if (strcmp (domains, "all") != 0 &&
          (log_domain == NULL || !strstr (domains, log_domain)))
        return G_LOG_WRITER_HANDLED;
    }

  if (agh_g_log_writer_syslog (log_level, fields, n_fields, user_data) ==
      G_LOG_WRITER_HANDLED)
    goto handled;

  return G_LOG_WRITER_UNHANDLED;

handled:
  return G_LOG_WRITER_HANDLED;
}

void agh_logging_init(void) {
	openlog(G_LOG_DOMAIN, LOG_CONS | LOG_PID | LOG_PERROR, LOG_DAEMON);
	g_log_set_writer_func(agh_g_log_writer, NULL, NULL);
	return;
}

void agh_logging_deinit(void) {
	closelog();
	return;
}
