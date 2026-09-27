/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <tuple>
#include <vector>

namespace deskflow::physical_edge {

// Coordinates and rectangles use logical pixels and half-open intervals.
struct Rect
{
  int32_t x = 0;
  int32_t y = 0;
  int32_t width = 0;
  int32_t height = 0;
};

enum class Side : uint8_t
{
  Left,
  Right,
  Top,
  Bottom
};

struct Segment
{
  uint32_t barrierId = 0;
  size_t zoneIndex = 0;
  Side side = Side::Left;
  int32_t fixed = 0;
  int32_t begin = 0;
  int32_t end = 0; // exclusive
};

class Geometry
{
public:
  // A translation is defensible only when portal and EIS report the same
  // logical bounding dimensions. Scaling or rotation requires explicit output
  // correspondence, which these APIs do not currently provide.
  bool reset(const std::vector<Rect> &zones, Rect deskflowShape, uint32_t firstBarrierId = 1,
             const std::vector<Rect> &eisRegions = {})
  {
    m_segments.clear();
    m_valid = false;
    m_shape = deskflowShape;
    if (zones.empty() || deskflowShape.width <= 0 || deskflowShape.height <= 0 ||
        static_cast<int64_t>(deskflowShape.x) + deskflowShape.width > std::numeric_limits<int32_t>::max() ||
        static_cast<int64_t>(deskflowShape.y) + deskflowShape.height > std::numeric_limits<int32_t>::max())
      return false;

    int64_t left = std::numeric_limits<int64_t>::max();
    int64_t top = std::numeric_limits<int64_t>::max();
    int64_t right = std::numeric_limits<int64_t>::min();
    int64_t bottom = std::numeric_limits<int64_t>::min();
    for (const auto &zone : zones) {
      if (zone.width <= 0 || zone.height <= 0)
        return false;
      if (static_cast<int64_t>(zone.x) + zone.width > std::numeric_limits<int32_t>::max() ||
          static_cast<int64_t>(zone.y) + zone.height > std::numeric_limits<int32_t>::max())
        return false;
      left = std::min(left, static_cast<int64_t>(zone.x));
      top = std::min(top, static_cast<int64_t>(zone.y));
      right = std::max(right, static_cast<int64_t>(zone.x) + zone.width);
      bottom = std::max(bottom, static_cast<int64_t>(zone.y) + zone.height);
    }
    if (right - left != deskflowShape.width || bottom - top != deskflowShape.height)
      return false;

    m_portalBounds = {static_cast<int32_t>(left), static_cast<int32_t>(top), deskflowShape.width,
                      deskflowShape.height};
    if (!eisRegions.empty()) {
      const int64_t dx = static_cast<int64_t>(deskflowShape.x) - left;
      const int64_t dy = static_cast<int64_t>(deskflowShape.y) - top;
      std::vector<Rect> translated;
      for (const auto &zone : zones) {
        if (static_cast<int64_t>(zone.x) + dx < std::numeric_limits<int32_t>::min() ||
            static_cast<int64_t>(zone.x) + dx > std::numeric_limits<int32_t>::max() ||
            static_cast<int64_t>(zone.y) + dy < std::numeric_limits<int32_t>::min() ||
            static_cast<int64_t>(zone.y) + dy > std::numeric_limits<int32_t>::max())
          return false;
        translated.push_back({static_cast<int32_t>(zone.x + dx), static_cast<int32_t>(zone.y + dy),
                              zone.width, zone.height});
      }
      auto ordered = [](const Rect &a, const Rect &b) {
        return std::tie(a.x, a.y, a.width, a.height) < std::tie(b.x, b.y, b.width, b.height);
      };
      auto same = [](const Rect &a, const Rect &b) {
        return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
      };
      std::vector<Rect> regions = eisRegions;
      std::sort(translated.begin(), translated.end(), ordered);
      std::sort(regions.begin(), regions.end(), ordered);
      translated.erase(std::unique(translated.begin(), translated.end(), same), translated.end());
      regions.erase(std::unique(regions.begin(), regions.end(), same), regions.end());
      if (translated.size() != regions.size() || !std::equal(translated.begin(), translated.end(), regions.begin(), same))
        return false;
    }
    for (size_t i = 0; i < zones.size(); ++i) {
      const auto &z = zones[i];
      addExposed(zones, i, Side::Left, z.x, z.y, z.y + z.height);
      addExposed(zones, i, Side::Right, z.x + z.width, z.y, z.y + z.height);
      addExposed(zones, i, Side::Top, z.y, z.x, z.x + z.width);
      addExposed(zones, i, Side::Bottom, z.y + z.height, z.x, z.x + z.width);
    }
    // IDs are deterministic within this geometry generation. The controller
    // invalidates the whole map before replacing portal barriers.
    if (firstBarrierId == 0 || m_segments.size() > std::numeric_limits<uint32_t>::max() - firstBarrierId)
      return false;
    uint32_t id = firstBarrierId;
    for (auto &segment : m_segments)
      segment.barrierId = id++;
    m_valid = true;
    return true;
  }

  [[nodiscard]] bool valid() const { return m_valid; }
  [[nodiscard]] const std::vector<Segment> &segments() const { return m_segments; }
  [[nodiscard]] const Segment *find(uint32_t id) const
  {
    if (!m_valid || m_segments.empty() || id < m_segments.front().barrierId ||
        id > m_segments.back().barrierId)
      return nullptr;
    return &m_segments[id - m_segments.front().barrierId];
  }

  // The wire fraction is the one Server::mapToFraction() would compute from
  // the same logical Deskflow screen shape, not a segment-local fraction.
  [[nodiscard]] std::optional<double> fraction(uint32_t id, double portalX, double portalY) const
  {
    const auto *s = find(id);
    if (!s || !std::isfinite(portalX) || !std::isfinite(portalY))
      return std::nullopt;
    const bool vertical = s->side == Side::Left || s->side == Side::Right;
    const double fixed = vertical ? portalX : portalY;
    const double along = vertical ? portalY : portalX;
    // Activation can be just outside the barrier; allow one logical pixel in
    // the normal direction, but never accept a different segment.
    if (std::abs(fixed - s->fixed) > 1.0 || along < s->begin || along >= s->end)
      return std::nullopt;
    const int64_t offset = vertical ? static_cast<int64_t>(m_shape.y) - m_portalBounds.y
                                    : static_cast<int64_t>(m_shape.x) - m_portalBounds.x;
    const double logical = along + static_cast<double>(offset);
    const double origin = vertical ? m_shape.y : m_shape.x;
    const double length = vertical ? m_shape.height : m_shape.width;
    const double result = (logical - origin + 0.5) / length;
    if (!std::isfinite(result) || result < 0.0 || result > 1.0)
      return std::nullopt;
    return result;
  }

private:
  void addExposed(const std::vector<Rect> &zones, size_t index, Side side, int32_t fixed, int32_t begin, int32_t end)
  {
    std::vector<int32_t> cuts{begin, end};
    const int64_t outside = static_cast<int64_t>(fixed) +
        ((side == Side::Left || side == Side::Top) ? -1 : 0);
    for (size_t i = 0; i < zones.size(); ++i) {
      if (i == index)
        continue;
      const auto &z = zones[i];
      const bool vertical = side == Side::Left || side == Side::Right;
      const int64_t lo = vertical ? z.x : z.y;
      const int64_t hi = lo + (vertical ? z.width : z.height);
      if (outside < lo || outside >= hi)
        continue;
      const int64_t a = vertical ? z.y : z.x;
      const int64_t b = a + (vertical ? z.height : z.width);
      if (a > begin && a < end)
        cuts.push_back(static_cast<int32_t>(a));
      if (b > begin && b < end)
        cuts.push_back(static_cast<int32_t>(b));
    }
    std::sort(cuts.begin(), cuts.end());
    cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
    for (size_t c = 1; c < cuts.size(); ++c) {
      const auto a = cuts[c - 1];
      const auto b = cuts[c];
      if (a == b)
        continue;
      const int64_t sample = a;
      const bool vertical = side == Side::Left || side == Side::Right;
      bool covered = false;
      for (size_t i = 0; i < zones.size(); ++i) {
        if (i == index)
          continue;
        const auto &z = zones[i];
        const int64_t fixedLo = vertical ? z.x : z.y;
        const int64_t fixedHi = fixedLo + (vertical ? z.width : z.height);
        const int64_t alongLo = vertical ? z.y : z.x;
        const int64_t alongHi = alongLo + (vertical ? z.height : z.width);
        if (outside >= fixedLo && outside < fixedHi && sample >= alongLo && sample < alongHi) {
          covered = true;
          break;
        }
      }
      if (!covered)
        m_segments.push_back({0, index, side, fixed, a, b});
    }
  }

  bool m_valid = false;
  Rect m_shape;
  Rect m_portalBounds;
  std::vector<Segment> m_segments;
};

} // namespace deskflow::physical_edge
