/*
 * Extrait à reporter dans src/main.cpp.
 * Les fonctions setup/loop et leurs ISR restent celles du firmware principal.
 */

#include <OPC.h>

#include "MinimalInstallation.h"

namespace
{
    MinimalInstallation installation;
    OPC opc(installation);
}
