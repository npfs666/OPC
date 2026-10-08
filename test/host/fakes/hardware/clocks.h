// SPDX-FileCopyrightText: 2022-2026 GAOU
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

enum clock_index { clk_sys };

inline uint32_t clock_get_hz(clock_index)
{
    return 150000000;
}
