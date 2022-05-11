#pragma once

#include <array>
#include <utility>

#include "Core/HotkeyManager.h"

// There is an example of how to use this code in DInputKeyboardAndMouse.cpp in the
// UpdateCursorInput() function.
namespace ciface
{
struct Point
{
  double x;
  double y;
};
struct ExtendedWindowInfo
{
  void* handle;
  double top;
  double left;
  double bottom;
  double right;
  double width;
  double height;
  double ratio;
  Point center;
};
using PrimaryPrimitive = double;

class OctagonalMouseJail
{
public:
  OctagonalMouseJail();

  static OctagonalMouseJail& GetInstance()
  {
    static OctagonalMouseJail octagonal_mouse_jail;
    return octagonal_mouse_jail;
  }

  Point LockMouseInJail(const PrimaryPrimitive& point_to_lock_x,
                        const PrimaryPrimitive& point_to_lock_y) const;

  ExtendedWindowInfo GetExtendedRenderWindowInfo() const;
  void UpdateRenderWindowInfo();

  bool m_player_gets_center = false;
  bool m_player_requested_center = false;
  bool m_non_client_area_interaction = true;

  void RefreshConfigValues();

private:
  std::array<Point, 8> m_octagon_points{};
  ExtendedWindowInfo m_render_window{};
  PrimaryPrimitive m_sensitivity = 1.0;
  PrimaryPrimitive m_snapping_distance = 0.0;
  bool m_jail_enabled = false;

  enum OctagonPoint
  {
    SOUTH,
    SOUTH_EAST,
    EAST,
    NORTH_EAST,
    NORTH,
    NORTH_WEST,
    WEST,
    SOUTH_WEST
  };

private:
  std::array<Point, 8> GenerateOctagonPoints() const;
  bool IsPointInsideOctagon(const Point& mouse_point) const;
  PrimaryPrimitive CalculateDistanceBetweenPoints(const Point& first_point,
                                                  const Point& second_point) const;
  long FindSecondLinePoint(const Point& mouse_point, long _index_of_closest_octagon_point) const;
  void SnapMouseToJail(Point& mouse_point) const;
};
}  // namespace ciface
