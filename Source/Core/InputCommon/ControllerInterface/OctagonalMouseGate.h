#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include "Common/Matrix.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

namespace ciface
{
class OctagonalMouseGate
{
public:
  OctagonalMouseGate();

  struct LockMouseInGateReturnValue
  {
    int x;
    int y;
  };
  std::optional<LockMouseInGateReturnValue> LockMouseInGate(int left, int right, int top,
                                                            int bottom, int point_to_lock_x,
                                                            int point_to_lock_y, int radius,
                                                            double snapping_distance);

  Common::TVec2<ControlState> CalculateRelativeCursorPosition(int x, int y, double scale_x,
                                                              double scale_y) const;

private:
  struct Point
  {
    double x;
    double y;
  };

  std::array<Point, 8> m_octagon_points{};
  Point m_center{};
  int m_radius = 0;
  int m_top = 0;
  int m_left = 0;
  int m_bottom = 0;
  int m_right = 0;
  int m_width = 0;
  int m_height = 0;

  // Indices into m_octagon_points array.
  static constexpr size_t SOUTH = 0;
  static constexpr size_t SOUTH_EAST = 1;
  static constexpr size_t EAST = 2;
  static constexpr size_t NORTH_EAST = 3;
  static constexpr size_t NORTH = 4;
  static constexpr size_t NORTH_WEST = 5;
  static constexpr size_t WEST = 6;
  static constexpr size_t SOUTH_WEST = 7;

  void UpdateCache(int radius, int left, int right, int top, int bottom);
  std::array<Point, 8> GenerateOctagonPoints() const;
  bool IsPointInsideOctagon(const Point& mouse_point) const;
  double CalculateDistanceBetweenPoints(const Point& first_point, const Point& second_point) const;
  size_t FindSecondLinePoint(const Point& mouse_point, size_t index_of_closest_octagon_point) const;
  void SnapMouseToGate(Point& mouse_point, double snapping_distance) const;
};
}  // namespace ciface
