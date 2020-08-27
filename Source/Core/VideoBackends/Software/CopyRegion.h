#pragma once

#include "Common/MathUtil.h"

#include <cmath>

namespace SW
{
// Modified from
// http://tech-algorithm.com/articles/nearest-neighbor-image-scaling/
template <typename T>
void CopyRegion(const T* const source, const MathUtil::Rectangle<int>& srcrect, T* destination,
                const MathUtil::Rectangle<int>& dstrect)
{
  double x_ratio = srcrect.GetWidth() / static_cast<double>(dstrect.GetWidth());
  double y_ratio = srcrect.GetHeight() / static_cast<double>(dstrect.GetHeight());
  for (int i = 0; i < dstrect.GetHeight(); i++)
  {
    for (int j = 0; j < dstrect.GetWidth(); j++)
    {
      int destination_x = j + dstrect.left;
      int destination_y = i + dstrect.top;
      int destination_offset = (destination_y * dstrect.GetWidth()) + destination_x;

      double src_x = std::round(destination_x * x_ratio) + srcrect.left;
      double src_y = std::round(destination_y * y_ratio) + srcrect.top;
      int src_offset = static_cast<int>((src_y * srcrect.GetWidth()) + src_x);

      destination[destination_offset] = source[src_offset];
    }
  }
}
}  // namespace SW
