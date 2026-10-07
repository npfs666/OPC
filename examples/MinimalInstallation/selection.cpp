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
