#!/usr/bin/env bash

set -euo pipefail

project_dir="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/../.."
    pwd
)"

build_dir="${project_dir}/.pio/host-tests"
binary="${build_dir}/opc-tests"
cxx="${CXX:-g++}"

mkdir -p "${build_dir}"

if ! command -v "${cxx}" >/dev/null 2>&1
then
    echo "Compilateur C++ introuvable: ${cxx}" >&2
    echo "Définir CXX ou installer g++ pour exécuter ces tests." >&2
    exit 127
fi

"${cxx}" \
    -std=c++17 \
    -Wall \
    -Wextra \
    -DOPC_HOST_TEST \
    -DOPC_BOARD_REV=2 \
    -I"${project_dir}/test/host/fakes" \
    -I"${project_dir}/test/host" \
    -I"${project_dir}/src" \
    "${project_dir}/test/host/test_main.cpp" \
    "${project_dir}/test/host/test_thermocouple.cpp" \
    "${project_dir}/test/host/test_pwm.cpp" \
    "${project_dir}/test/host/test_digital_input.cpp" \
    "${project_dir}/test/host/test_schedule.cpp" \
    "${project_dir}/test/host/test_measurement_status.cpp" \
    "${project_dir}/test/host/test_relay_timing.cpp" \
    "${project_dir}/test/host/test_input_filter.cpp" \
    "${project_dir}/test/host/test_fault_fallback.cpp" \
    "${project_dir}/test/host/test_home_setpoint.cpp" \
    "${project_dir}/test/host/test_limit_alarm.cpp" \
    "${project_dir}/test/host/test_manual_mode.cpp" \
    "${project_dir}/test/host/test_counters.cpp" \
    "${project_dir}/test/host/test_loop_break.cpp" \
    "${project_dir}/test/host/test_event_log.cpp" \
    "${project_dir}/test/host/test_logic.cpp" \
    "${project_dir}/test/host/test_comparator.cpp" \
    "${project_dir}/test/host/test_solar.cpp" \
    "${project_dir}/test/host/test_delay_timer.cpp" \
    "${project_dir}/test/host/test_inhibit.cpp" \
    "${project_dir}/test/host/test_condition_alarm.cpp" \
    "${project_dir}/test/host/test_cold_room.cpp" \
    "${project_dir}/test/host/test_minimal_installation.cpp" \
    "${project_dir}/test/host/test_three_point.cpp" \
    "${project_dir}/test/host/test_heating_curve.cpp" \
    "${project_dir}/test/host/test_heating_circuit.cpp" \
    "${project_dir}/src/Inputs/DigitalInput.cpp" \
    "${project_dir}/src/Drivers/DS3231.cpp" \
    "${project_dir}/src/Hardware/RTC.cpp" \
    "${project_dir}/src/Hardware/Sensor.cpp" \
    "${project_dir}/src/Installation.cpp" \
    "${project_dir}/src/hmi/Displayable.cpp" \
    "${project_dir}/src/hmi/DisplayTextCodec.cpp" \
    "${project_dir}/src/hmi/AlarmDisplay.cpp" \
    "${project_dir}/src/hmi/EventLogScreen.cpp" \
    "${project_dir}/src/hmi/HomeSetpointEditor.cpp" \
    "${project_dir}/src/hmi/MeasurementDisplay.cpp" \
    "${project_dir}/src/hmi/MenuBuilder.cpp" \
    "${project_dir}/src/hmi/ParameterEditor.cpp" \
    "${project_dir}/src/hmi/ParameterList.cpp" \
    "${project_dir}/src/Measurements/Humidity/HumidityBME.cpp" \
    "${project_dir}/src/Measurements/Measurement.cpp" \
    "${project_dir}/src/ProcessSnapshot.cpp" \
    "${project_dir}/src/Measurements/Pressure/PressureBME.cpp" \
    "${project_dir}/src/Measurements/Resistance.cpp" \
    "${project_dir}/src/Measurements/Temperature/TemperatureBME.cpp" \
    "${project_dir}/src/Measurements/Temperature/TemperatureRTD.cpp" \
    "${project_dir}/src/Measurements/Temperature/TemperatureTC.cpp" \
    "${project_dir}/src/Outputs/Actuator.cpp" \
    "${project_dir}/src/Outputs/ActuatorOnOff.cpp" \
    "${project_dir}/src/Outputs/ActuatorPWM.cpp" \
    "${project_dir}/src/Outputs/Output.cpp" \
    "${project_dir}/src/Outputs/RelayOutput.cpp" \
    "${project_dir}/src/Outputs/TimeProportionalActuator.cpp" \
    "${project_dir}/src/Outputs/ThreePointActuator.cpp" \
    "${project_dir}/src/Outputs/PWMOutput.cpp" \
    "${project_dir}/src/Physics/PT100.cpp" \
    "${project_dir}/src/Physics/Thermocouple.cpp" \
    "${project_dir}/src/Physics/Psychrometrics.cpp" \
    "${project_dir}/src/EventLog.cpp" \
    "${project_dir}/src/ProcessControl.cpp" \
    "${project_dir}/src/Regulator/PID.cpp" \
    "${project_dir}/src/Regulator/PIDAutoTune.cpp" \
    "${project_dir}/src/Regulator/Regulator.cpp" \
    "${project_dir}/src/Regulator/Alarm.cpp" \
    "${project_dir}/src/Regulator/Comparator.cpp" \
    "${project_dir}/src/Regulator/ConditionAlarm.cpp" \
    "${project_dir}/src/Regulator/DelayTimer.cpp" \
    "${project_dir}/src/Regulator/HeatingCurve.cpp" \
    "${project_dir}/src/Regulator/LimitAlarm.cpp" \
    "${project_dir}/src/Regulator/LogicCommand.cpp" \
    "${project_dir}/src/Regulator/LoopBreakAlarm.cpp" \
    "${project_dir}/src/Regulator/ScheduledSetpoint.cpp" \
    "${project_dir}/src/Regulator/SetpointRamp.cpp" \
    "${project_dir}/src/Regulator/Thermostat.cpp" \
    "${project_dir}/src/Regulator/TimeSchedule.cpp" \
    "${project_dir}/src/SystemWatchdog.cpp" \
    "${project_dir}/src/Templates/SolarInstallation.cpp" \
    "${project_dir}/src/Templates/ColdRoomInstallation.cpp" \
    "${project_dir}/src/Templates/HeatingCircuitInstallation.cpp" \
    "${project_dir}/examples/MinimalInstallation/MinimalInstallation.cpp" \
    -o "${binary}"

"${binary}"
