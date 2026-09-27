/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Event.h"
#include <cstdint>

namespace deskflow {

struct PhysicalEdgeActivatedInfo : EventData
{
  uint32_t side = 0;     // 0=left, 1=right, 2=top, 3=bottom
  uint32_t fraction = 0; // whole-screen edge, millionths
};

struct PhysicalEdgeResultInfo : EventData
{
  uint32_t requestId = 0;
  bool approved = false;
};

} // namespace deskflow
