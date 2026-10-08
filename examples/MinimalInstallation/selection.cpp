// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Extrait à reporter dans src/main.cpp.
 * Le watchdog, setup/loop et les ISR restent ceux du firmware principal.
 */

#include <OPC.h>

#include "MinimalInstallation.h"

namespace
{
    MinimalInstallation installation;
    OPC opc(installation);
}
