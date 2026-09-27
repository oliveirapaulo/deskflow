/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>

struct PhysicalEdgePoint
{
  int32_t x = 0;
  int32_t y = 0;
};

// Reconstructs the server's source-edge point from the same whole-screen
// fraction that Server::mapToFraction() uses.  Side is 0..3 (L/R/T/B).
inline std::optional<PhysicalEdgePoint> physicalEdgePoint(
    int32_t sx, int32_t sy, int32_t width, int32_t height, uint32_t side, uint32_t fraction)
{
  if (width <= 0 || height <= 0 || side > 3 || fraction > 1000000)
    return std::nullopt;
  const int64_t right = static_cast<int64_t>(sx) + width;
  const int64_t bottom = static_cast<int64_t>(sy) + height;
  if (static_cast<int64_t>(sx) - 1 < std::numeric_limits<int32_t>::min() ||
      static_cast<int64_t>(sy) - 1 < std::numeric_limits<int32_t>::min() ||
      right > std::numeric_limits<int32_t>::max() || bottom > std::numeric_limits<int32_t>::max())
    return std::nullopt;
  PhysicalEdgePoint point;
  if (side < 2) {
    point.x = side == 0 ? sx - 1 : static_cast<int32_t>(right);
    point.y = sy + std::min(height - 1, static_cast<int32_t>(static_cast<int64_t>(fraction) * height / 1000000));
  } else {
    point.x = sx + std::min(width - 1, static_cast<int32_t>(static_cast<int64_t>(fraction) * width / 1000000));
    point.y = side == 2 ? sy - 1 : static_cast<int32_t>(bottom);
  }
  return point;
}
