#include "InputCommon/ControllerInterface/OctagonalMouseJail.h"
#ifdef _WIN32
#include <Windows.h>
#endif

#include <array>
#include <cmath>
#include <utility>

#include "Core/Config/MainSettings.h"
#include "Core/Config/MouseSettings.h"
#include "Core/Core.h"
#include "Core/Host.h"

namespace ciface
{
OctagonalMouseJail::OctagonalMouseJail()
{
  UpdateRenderWindowInfo();

  RefreshConfigValues();
}

Point OctagonalMouseJail::LockMouseInJail(const PrimaryPrimitive& point_to_lock_x,
                                          const PrimaryPrimitive& point_to_lock_y) const
{
  Point mouse_point = {point_to_lock_x, point_to_lock_y};
  if (!((::Core::GetState() == ::Core::State::Running ||
         ::Core::GetState() == ::Core::State::Paused) &&
        Host_RendererHasFocus() && m_jail_enabled && !m_non_client_area_interaction))
  {
    return mouse_point;
  }

  if (m_render_window.width == 0 || m_render_window.height == 0)
    return mouse_point;

  // Start of locking

  if (IsPointInsideOctagon(mouse_point))
  {
    return mouse_point;
  }
  else
  {
    SnapMouseToJail(mouse_point);
  }

  return mouse_point;
}

ExtendedWindowInfo OctagonalMouseJail::GetExtendedRenderWindowInfo() const
{
  return m_render_window;
}

std::array<Point, 8> OctagonalMouseJail::GenerateOctagonPoints() const
{
  std::array<Point, 8> octagon_points;

  PrimaryPrimitive fraction_of_screen_to_lock_mouse_in_x =
      m_render_window.width / (m_sensitivity * m_render_window.ratio);
  PrimaryPrimitive fraction_of_screen_to_lock_mouse_in_y = m_render_window.height / m_sensitivity;

  // I asked on the game cube controller modder discord and someone said 0.7 and my personal
  // eyeballing was 0.75 so I am inclined to believe them.
  constexpr PrimaryPrimitive percentage_reduction_for_corner_gates = 0.7;

  // Southernmost point is cardinal direction and we want to make sure we're inputting the maximum
  // value in the negative Y direction so we floor the calculation to make sure the point the
  // mouse gets held in is the max-value cardinal direction. erring on the side of too far down
  // instead of not far enough. Erring on the side of too far will be repeated for the cardinal
  // directions.
  octagon_points[NORTH] =
      Point{m_render_window.center.x,
            std::floor(m_render_window.center.y - fraction_of_screen_to_lock_mouse_in_y)};

  octagon_points[NORTH_EAST] =
      Point{m_render_window.center.x +
                (fraction_of_screen_to_lock_mouse_in_x * percentage_reduction_for_corner_gates),
            m_render_window.center.y -
                (fraction_of_screen_to_lock_mouse_in_y * percentage_reduction_for_corner_gates)};

  octagon_points[EAST] = Point{

      std::ceil(m_render_window.center.x + fraction_of_screen_to_lock_mouse_in_x),
      m_render_window.center.y};

  octagon_points[SOUTH_EAST] =
      Point{m_render_window.center.x +
                (fraction_of_screen_to_lock_mouse_in_x * percentage_reduction_for_corner_gates),
            m_render_window.center.y +
                (fraction_of_screen_to_lock_mouse_in_y * percentage_reduction_for_corner_gates)};

  octagon_points[SOUTH] =
      Point{m_render_window.center.x,

            std::ceil(m_render_window.center.y + fraction_of_screen_to_lock_mouse_in_y)};

  octagon_points[SOUTH_WEST] =
      Point{m_render_window.center.x -
                (fraction_of_screen_to_lock_mouse_in_x * percentage_reduction_for_corner_gates),
            m_render_window.center.y +
                (fraction_of_screen_to_lock_mouse_in_y * percentage_reduction_for_corner_gates)};

  octagon_points[WEST] = Point{

      std::floor(m_render_window.center.x - fraction_of_screen_to_lock_mouse_in_x),
      m_render_window.center.y};

  octagon_points[NORTH_WEST] =
      Point{m_render_window.center.x -
                (fraction_of_screen_to_lock_mouse_in_x * percentage_reduction_for_corner_gates),
            m_render_window.center.y -
                (fraction_of_screen_to_lock_mouse_in_y * percentage_reduction_for_corner_gates)};
  return octagon_points;
}

bool OctagonalMouseJail::IsPointInsideOctagon(const Point& mouse_point) const
{
  //  This function uses a raycasting method to figure out if a point is  inside the octagon
  //  casting a ray horizontally in one direction from the mouse point and if the
  //  casted ray intersects with the edges of the octagon an even number of times (0 counts)
  //  the point is outside of the octagon. If the ray intersects an odd number of times, the
  //  ray is inside of the polygon. Jordan Curve Thereom is the name I was told
  //  https://erich.realtimerendering.com/ptinpoly/
  unsigned int number_of_intersections = 0;

  for (int i = 0; i < 8; i++)
  {
    int next_index = i + 1;
    if (i == 7)
      next_index = 0;

    PrimaryPrimitive rise = m_octagon_points[next_index].y - m_octagon_points[i].y;
    PrimaryPrimitive run = m_octagon_points[next_index].x - m_octagon_points[i].x;
    PrimaryPrimitive slope = rise / run;

    PrimaryPrimitive temp_x =
        ((mouse_point.y - m_octagon_points[i].y) / slope) + m_octagon_points[i].x;

    if (temp_x < mouse_point.x)
    {
      continue;
    }

    if (i < 2 || i > 5)
    {
      if (temp_x >= m_octagon_points[i].x && temp_x < m_octagon_points[next_index].x)
      {
        number_of_intersections++;
      }
    }
    else if (i >= 2 && i <= 5)
    {
      if (temp_x <= m_octagon_points[i].x && temp_x > m_octagon_points[next_index].x)
      {
        number_of_intersections++;
      }
    }
  }

  if (number_of_intersections % 2 == 0)
  {
    return false;
  }
  else
  {
    return true;
  }
}

PrimaryPrimitive OctagonalMouseJail::CalculateDistanceBetweenPoints(const Point& first_point,
                                                                    const Point& second_point) const
{
  // Pythagorean theoreom used to calculate the distance between points
  // the hypotenuse of the right triangle is the distance.
  // the x_difference and y_difference are the two legs of the triangle.
  PrimaryPrimitive x_difference = first_point.x - second_point.x;
  PrimaryPrimitive y_difference = first_point.y - second_point.y;

  return std::sqrt((x_difference * x_difference) + (y_difference * y_difference));
}

long OctagonalMouseJail::FindSecondLinePoint(const Point& mouse_point,
                                             long _index_of_closest_octagon_point) const
{
  if (mouse_point.y < m_octagon_points[_index_of_closest_octagon_point].y)
  {
    switch (_index_of_closest_octagon_point)
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
  if (mouse_point.x < m_octagon_points[_index_of_closest_octagon_point].x)
  {
    switch (_index_of_closest_octagon_point)
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
  if (mouse_point.y > m_octagon_points[_index_of_closest_octagon_point].y)
  {
    switch (_index_of_closest_octagon_point)
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
  if (mouse_point.x > m_octagon_points[_index_of_closest_octagon_point].x)
  {
    switch (_index_of_closest_octagon_point)
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

void OctagonalMouseJail::SnapMouseToJail(Point& mouse_point) const
{
  // Snaps to the edge of the jail in almost all cases. Exception is if
  // the mouse is within the snapping distance of an octagon_point.

  std::array<long, 8> distance_to_octagon_points = {};
  for (int i = 0; i < 8; i++)
  {
    distance_to_octagon_points[i] =
        CalculateDistanceBetweenPoints(mouse_point, m_octagon_points[i]);
  }

  long min_distance = distance_to_octagon_points[0];
  long index_of_closest_octagon_point = 0;
  for (int i = 0; i < 8; i++)
  {
    if (distance_to_octagon_points[i] < min_distance)
    {
      min_distance = distance_to_octagon_points[i];
      index_of_closest_octagon_point = i;
    }
  }

  if (CalculateDistanceBetweenPoints(
          mouse_point, m_octagon_points[index_of_closest_octagon_point]) < m_snapping_distance)
  {
    mouse_point = m_octagon_points[index_of_closest_octagon_point];
    return;
  }

  long index_of_second_line_point =
      FindSecondLinePoint(mouse_point, index_of_closest_octagon_point);

  PrimaryPrimitive rise = m_octagon_points[index_of_second_line_point].y -
                          m_octagon_points[index_of_closest_octagon_point].y;
  PrimaryPrimitive run = m_octagon_points[index_of_second_line_point].x -
                         m_octagon_points[index_of_closest_octagon_point].x;
  PrimaryPrimitive slope_of_octagon_line = rise / run;
  PrimaryPrimitive y_intercept_of_octagon_line =
      slope_of_octagon_line * (0 - m_octagon_points[index_of_closest_octagon_point].x) +
      m_octagon_points[index_of_closest_octagon_point].y;

  rise = m_render_window.center.y - mouse_point.y;
  run = m_render_window.center.x - mouse_point.x;
  PrimaryPrimitive slope_of_line_from_center_to_mouse = rise / run;
  constexpr char distance_from_y_axis_where_math_breaks_down = 1.0;
  if (std::abs(run) < distance_from_y_axis_where_math_breaks_down)
  {
    if (mouse_point.y < m_render_window.center.y)
    {
      mouse_point = m_octagon_points[NORTH];
    }
    if (mouse_point.y > m_render_window.center.y)
    {
      mouse_point = m_octagon_points[SOUTH];
    }
    return;
  }
  PrimaryPrimitive y_intercept_of_line_from_center_to_mouse =
      slope_of_line_from_center_to_mouse * (0 - m_render_window.center.x) +
      m_render_window.center.y;

  Point point_of_intersection = {};

  point_of_intersection.x =
      std::round(((y_intercept_of_octagon_line - y_intercept_of_line_from_center_to_mouse) /
                  (slope_of_line_from_center_to_mouse - slope_of_octagon_line)));
  point_of_intersection.y =
      std::round((slope_of_octagon_line * point_of_intersection.x) + y_intercept_of_octagon_line);

  mouse_point = point_of_intersection;
}

void OctagonalMouseJail::UpdateRenderWindowInfo()
{
  WindowInfo render_window_info = Host_GetRenderWindowInfo();
  m_render_window.handle = render_window_info.handle;
  m_render_window.top = render_window_info.top;
  m_render_window.left = render_window_info.left;
  m_render_window.bottom = render_window_info.bottom;
  m_render_window.right = render_window_info.right;
  m_render_window.width = m_render_window.right - m_render_window.left;
  m_render_window.height = m_render_window.bottom - m_render_window.top;
  m_render_window.center.y = (m_render_window.height / 2.0) + m_render_window.top;
  m_render_window.center.x = (m_render_window.width / 2.0) + m_render_window.left;
  if (m_render_window.height == 0)
    m_render_window.ratio = 0;
  else
    m_render_window.ratio = m_render_window.width / m_render_window.height;
  if (m_render_window.handle == nullptr)
    m_octagon_points = std::array<Point, 8>{};
  else
    m_octagon_points = GenerateOctagonPoints();
}

void OctagonalMouseJail::RefreshConfigValues()
{
  m_sensitivity = Config::Get(Config::MOUSE_SENSITIVITY);
  m_snapping_distance = Config::Get(Config::MOUSE_SNAPPING_DISTANCE);
  m_jail_enabled = Config::Get(Config::MOUSE_OCTAGONAL_JAIL_ENABLED);
}
}  // namespace ciface
