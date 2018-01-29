#ifndef __callbacks_h__
#define __callbacks_h__

#include <glib.h>
#include "agh.h"

int agh_unix_signals_cb_dispatch(gpointer data);
int agh_timeout_cb_dispatch(gpointer data);

#endif
