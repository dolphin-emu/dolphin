#include "InputCommon/ControllerInterface/OctagonalMouseGate.h"

#include <array>
#include <cmath>
#include <optional>

#include "Core/Config/MainSettings.h"

namespace ciface
{
OctagonalMouseGate::OctagonalMouseGate() = default;

std::optional<OctagonalMouseGate::LockMouseInGateReturnValue>
OctagonalMouseGate::LockMouseInGate(int left, int right, int top, int bottom, int point_to_lock_x,
                                    int point_to_lock_y, int radius, double snapping_distance)
{
  if (radius == 0)
    return std::nullopt;

  // check if cached data need to be updated
  if (m_radius != radius || left != m_left || right != m_right || top != m_top ||
      bottom != m_bottom)
  {
    UpdateCache(radius, left, right, top, bottom);
  }

  if (m_width == 0 || m_height == 0)
    return std::nullopt;

  // Start of locking
  OctagonalMouseGate::LockMouseInGateReturnValue rv{point_to_lock_x, point_to_lock_y};

  Point mouse_point{static_cast<double>(point_to_lock_x), static_cast<double>(point_to_lock_y)};
  if (!IsPointInsideOctagon(mouse_point))
  {
    SnapMouseToGate(mouse_point, snapping_distance);
    rv.x = static_cast<int>(mouse_point.x);
    rv.y = static_cast<int>(mouse_point.y);
  }

  return rv;
}

Common::TVec2<ControlState>
OctagonalMouseGate::CalculateRelativeCursorPosition(int x, int y, double scale_x,
                                                    double scale_y) const
{
  const auto center_x = m_center.x;
  const auto center_y = m_center.y;
  const auto radius = m_radius;
  const auto diameter = m_radius * 2.0;
  const ControlState relative_x = ((ControlState(x) - (center_x - radius)) / diameter) * 2.0 - 1.0;
  const ControlState relative_y = ((ControlState(y) - (center_y - radius)) / diameter) * 2.0 - 1.0;
  return Common::TVec2<ControlState>(relative_x * scale_x, relative_y * scale_y);
}

std::array<OctagonalMouseGate::Point, 8> OctagonalMouseGate::GenerateOctagonPoints() const
{
  std::array<Point, 8> octagon_points;

  const double radius = m_radius;
  const double x = m_center.x;
  const double y = m_center.y;

  // I asked on the game cube controller modder discord and someone said 0.7 and my personal
  // eyeballing was 0.75 so I am inclined to believe them.
  constexpr double percentage_reduction_for_corner_gates = 0.7;
  const double corner = radius * percentage_reduction_for_corner_gates;

  // Southernmost point is cardinal direction and we want to make sure we're inputting the maximum
  // value in the negative Y direction so we floor the calculation to make sure the point the
  // mouse gets held in is the max-value cardinal direction. erring on the side of too far down
  // instead of not far enough. Erring on the side of too far will be repeated for the cardinal
  // directions.
  octagon_points[NORTH] = Point{x, std::floor(y - radius)};
  octagon_points[NORTH_EAST] = Point{x + corner, y - corner};
  octagon_points[EAST] = Point{std::ceil(x + radius), y};
  octagon_points[SOUTH_EAST] = Point{x + corner, y + corner};
  octagon_points[SOUTH] = Point{x, std::ceil(y + radius)};
  octagon_points[SOUTH_WEST] = Point{x - corner, y + corner};
  octagon_points[WEST] = Point{std::floor(x - radius), y};
  octagon_points[NORTH_WEST] = Point{x - corner, y - corner};

  return octagon_points;
}

bool OctagonalMouseGate::IsPointInsideOctagon(const Point& mouse_point) const
{
  //  This function uses a raycasting method to figure out if a point is  inside the octagon
  //  casting a ray horizontally in one direction from the mouse point and if the
  //  casted ray intersects with the edges of the octagon an even number of times (0 counts)
  //  the point is outside of the octagon. If the ray intersects an odd number of times, the
  //  ray is inside of the polygon. Jordan Curve Thereom is the name I was told
  //  https://erich.realtimerendering.com/ptinpoly/
  unsigned int number_of_intersections = 0;

  for (size_t i = 0; i < 8; ++i)
  {
    const size_t next_index = (i + 1) % 8;

    const double rise = m_octagon_points[next_index].y - m_octagon_points[i].y;
    const double run = m_octagon_points[next_index].x - m_octagon_points[i].x;
    const double slope = rise / run;

    const double temp_x = ((mouse_point.y - m_octagon_points[i].y) / slope) + m_octagon_points[i].x;

    if (temp_x < mouse_point.x)
      continue;

    if (i < 2 || i > 5)
    {
      if (temp_x >= m_octagon_points[i].x && temp_x < m_octagon_points[next_index].x)
        ++number_of_intersections;
    }
    else if (i >= 2 && i <= 5)
    {
      if (temp_x <= m_octagon_points[i].x && temp_x > m_octagon_points[next_index].x)
        ++number_of_intersections;
    }
  }

  if (number_of_intersections % 2 == 0)
    return false;
  else
    return true;
}

double OctagonalMouseGate::CalculateDistanceBetweenPoints(const Point& first_point,
                                                          const Point& second_point) const
{
  // Pythagorean theoreom used to calculate the distance between points
  // the hypotenuse of the right triangle is the distance.
  // the x_difference and y_difference are the two legs of the triangle.
  double x_difference = first_point.x - second_point.x;
  double y_difference = first_point.y - second_point.y;

  return std::sqrt((x_difference * x_difference) + (y_difference * y_difference));
}

size_t OctagonalMouseGate::FindSecondLinePoint(const Point& mouse_point,
                                               size_t index_of_closest_octagon_point) const
{
  if (mouse_point.y < m_octagon_points[index_of_closest_octagon_point].y)
  {
    switch (index_of_closest_octagon_point)
    {
    case SOUTH:
      break;
    case SOUTH_EAST:
      break;
    case EAST:
      return NORTH_EAST;
    case NORTH_EAST:
      break;
    case NORTH:
      break;
    case NORTH_WEST:
      break;
    case WEST:
      return NORTH_WEST;
    case SOUTH_WEST:
      break;
    }
  }
  if (mouse_point.x < m_octagon_points[index_of_closest_octagon_point].x)
  {
    switch (index_of_closest_octagon_point)
    {
    case SOUTH:
      return SOUTH_WEST;
    case SOUTH_EAST:
      return SOUTH;
    case EAST:
      break;
    case NORTH_EAST:
      return NORTH;
    case NORTH:
      return NORTH_WEST;
    case NORTH_WEST:
      return WEST;
    case WEST:
      break;
    case SOUTH_WEST:
      return WEST;
    }
  }
  if (mouse_point.y > m_octagon_points[index_of_closest_octagon_point].y)
  {
    switch (index_of_closest_octagon_point)
    {
    case SOUTH:
      break;
    case SOUTH_EAST:
      break;
    case EAST:
      return SOUTH_EAST;
    case NORTH_EAST:
      break;
    case NORTH:
      break;
    case NORTH_WEST:
      break;
    case WEST:
      return SOUTH_WEST;
    case SOUTH_WEST:
      break;
    }
  }
  if (mouse_point.x > m_octagon_points[index_of_closest_octagon_point].x)
  {
    switch (index_of_closest_octagon_point)
    {
    case SOUTH:
      return SOUTH_EAST;
    case SOUTH_EAST:
      return EAST;
    case EAST:
      break;
    case NORTH_EAST:
      return EAST;
    case NORTH:
      return NORTH_EAST;
    case NORTH_WEST:
      return NORTH;
    case WEST:
      break;
    case SOUTH_WEST:
      return SOUTH;
    }
  }
  return 0;
}

void OctagonalMouseGate::SnapMouseToGate(Point& mouse_point, double snapping_distance) const
{
  // Snaps to the edge of the gate in almost all cases. Exception is if
  // the mouse is within the snapping distance of an octagon_point.

  std::array<double, 8> distance_to_octagon_points = {};
  for (size_t i = 0; i < 8; ++i)
  {
    distance_to_octagon_points[i] =
        CalculateDistanceBetweenPoints(mouse_point, m_octagon_points[i]);
  }

  double min_distance = distance_to_octagon_points[0];
  size_t index_of_closest_octagon_point = 0;
  for (size_t i = 0; i < 8; ++i)
  {
    if (distance_to_octagon_points[i] < min_distance)
    {
      min_distance = distance_to_octagon_points[i];
      index_of_closest_octagon_point = i;
    }
  }

  if (snapping_distance > 0.0 &&
      CalculateDistanceBetweenPoints(
          mouse_point, m_octagon_points[index_of_closest_octagon_point]) < snapping_distance)
  {
    mouse_point = m_octagon_points[index_of_closest_octagon_point];
    return;
  }

  const size_t index_of_second_line_point =
      FindSecondLinePoint(mouse_point, index_of_closest_octagon_point);

  double rise = m_octagon_points[index_of_second_line_point].y -
                m_octagon_points[index_of_closest_octagon_point].y;
  double run = m_octagon_points[index_of_second_line_point].x -
               m_octagon_points[index_of_closest_octagon_point].x;
  const double slope_of_octagon_line = rise / run;
  const double y_intercept_of_octagon_line =
      slope_of_octagon_line * (-m_octagon_points[index_of_closest_octagon_point].x) +
      m_octagon_points[index_of_closest_octagon_point].y;

  rise = m_center.y - mouse_point.y;
  run = m_center.x - mouse_point.x;
  const double slope_of_line_from_center_to_mouse = rise / run;
  constexpr double distance_from_y_axis_where_math_breaks_down = 1.0;
  if (std::abs(run) < distance_from_y_axis_where_math_breaks_down)
  {
    if (mouse_point.y < m_center.y)
      mouse_point = m_octagon_points[NORTH];
    if (mouse_point.y > m_center.y)
      mouse_point = m_octagon_points[SOUTH];
    return;
  }
  const double y_intercept_of_line_from_center_to_mouse =
      slope_of_line_from_center_to_mouse * (0 - m_center.x) + m_center.y;

  Point point_of_intersection = {};

  point_of_intersection.x =
      std::round(((y_intercept_of_octagon_line - y_intercept_of_line_from_center_to_mouse) /
                  (slope_of_line_from_center_to_mouse - slope_of_octagon_line)));
  point_of_intersection.y =
      std::round((slope_of_octagon_line * point_of_intersection.x) + y_intercept_of_octagon_line);

  mouse_point = point_of_intersection;
}

void OctagonalMouseGate::UpdateCache(int radius, int left, int right, int top, int bottom)
{
  m_radius = radius;
  m_top = top;
  m_left = left;
  m_bottom = bottom;
  m_right = right;
  m_width = right - left;
  m_height = bottom - top;
  m_center.y = (m_height * 0.5) + top;
  m_center.x = (m_width * 0.5) + left;

  if (m_radius > 0 && m_width > 0 && m_height > 0)
    m_octagon_points = GenerateOctagonPoints();
  else
    m_octagon_points = std::array<Point, 8>{};
}
}  // namespace ciface
