#ifndef __modem_helpers_h__
#define __modem_helpers_h__

gchar *agh_mm_common_build_capabilities_string(const MMModemCapability *capabilities, guint n_capabilities);
gchar *agh_mm_common_build_ports_string(const MMModemPortInfo *ports, guint n_ports);

#endif
