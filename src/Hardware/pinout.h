#pragma once



// Used in RTDSensor
#define MAX_RTD 3
#define ALPHA 0.6f

// Maxixum array sizes
#define MAX_MEASUREMENTS 16 // Used in ProcessControl
#define MAX_REGULATORS 16   // Used in ProcessControl
#define MAX_ACTUATORS 16    // Used in ProcessControl
#define MAX_OUTPUTS 4       // Maximum outputs connected to one Actuator
#define MAX_REGISTERED_OUTPUTS 16 // Outputs managed by ProcessControl
#define MAX_PARAMETERS 64   // Used in ParameterList



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