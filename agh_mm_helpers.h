#ifndef __agh_mm_helpers_h__
#define __agh_mm_helpers_h__

gchar *agh_mm_common_build_capabilities_string(const MMModemCapability *capabilities, guint n_capabilities);
gchar *agh_mm_common_build_ports_string(const MMModemPortInfo *ports, guint n_ports);
gchar *agh_mm_unlock_retries_build_string(MMUnlockRetries *object);
gchar *agh_mm_common_build_mode_combinations_string(const MMModemModeCombination *modes, guint n_modes);
gchar *agh_mm_common_build_bands_string(const MMModemBand *bands, guint n_bands);

/* from mmcli */
const gchar *agh_mm_get_statechange_reason_string(MMModemStateChangeReason reason);

/* hack, to be fixed */
struct _MMUnlockRetriesPrivate {
	GHashTable *ht;
};

#endif
