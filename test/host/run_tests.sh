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
    -I"${project_dir}/test/host/fakes" \
    -I"${project_dir}/test/host" \
    -I"${project_dir}/src" \
    "${project_dir}/test/host/test_main.cpp" \
    "${project_dir}/src/hmi/Displayable.cpp" \
    "${project_dir}/src/hmi/ParameterEditor.cpp" \
    "${project_dir}/src/hmi/ParameterList.cpp" \
    "${project_dir}/src/Measurements/Humidity/HumidityBME.cpp" \
    "${project_dir}/src/Measurements/Measurement.cpp" \
    "${project_dir}/src/Measurements/MeasurementSnapshot.cpp" \
    "${project_dir}/src/Measurements/Pressure/PressureBME.cpp" \
    "${project_dir}/src/Measurements/Temperature/TemperatureBME.cpp" \
    "${project_dir}/src/Outputs/Actuator.cpp" \
    "${project_dir}/src/Outputs/Output.cpp" \
    "${project_dir}/src/Physics/PT100.cpp" \
    "${project_dir}/src/Physics/Psychrometrics.cpp" \
    "${project_dir}/src/ProcessControl.cpp" \
    "${project_dir}/src/Regulator/PID.cpp" \
    "${project_dir}/src/Regulator/Regulator.cpp" \
    "${project_dir}/src/Regulator/SolarRegulator.cpp" \
    "${project_dir}/src/Regulator/Thermostat.cpp" \
    -o "${binary}"

"${binary}"
