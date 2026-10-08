// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once



// Used in Sensor
#define MAX_RTD 3

// Maxixum array sizes
#define MAX_MEASUREMENTS 16 // Used in ProcessControl
#define MAX_DIGITAL_INPUTS 2 // Entrées numériques gérées par ProcessControl
#define MAX_REGULATORS 32   // Used in ProcessControl (blocs et alarmes compris)
#define MAX_ACTUATORS 16    // Used in ProcessControl
#define MAX_OUTPUTS 4       // Maximum outputs connected to one Actuator
#define MAX_REGISTERED_OUTPUTS 16 // Outputs managed by ProcessControl
#define MAX_PARAMETERS 192  // Used in ParameterList
#define MAX_ALARMS 8        // Alarmes de seuil suivies par ProcessControl



#ifndef OPC_BOARD_REV
#error "OPC_BOARD_REV doit être définie"
#endif

#if OPC_BOARD_REV == 1
#include "Boards/Pinout_v0.1.h"
#elif OPC_BOARD_REV == 2
#include "Boards/Pinout_v0.2.h"
#else
#error "Révision de PCB inconnue"
#endif
