// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "common.h"

// Configuration keys stay stable; presentation resolves the current translator.
inline QString site_display_name(int index)
{
    if (index < 0 || index >= CONFIG_NUM_SITES)
        return {};
    return index == CONFIG_CUSTOM ? QCoreApplication::translate("Scrobble", "Custom") : SITE_NAME[index];
}

inline QString log_level_display_name(int level)
{
    switch (level) {
    case LOG_ERROR: return QCoreApplication::translate("Scrobble", "ERROR");
    case LOG_INFO: return QCoreApplication::translate("Scrobble", "INFO ");
    case LOG_DEBUG: return QCoreApplication::translate("Scrobble", "DEBUG");
    case LOG_TRACE: return QCoreApplication::translate("Scrobble", "TRACE");
    default: return {};
    }
}
