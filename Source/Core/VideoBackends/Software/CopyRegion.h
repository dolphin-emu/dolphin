#pragma once

#include "Common/Assert.h"
#include "Common/MathUtil.h"

#include <cmath>

namespace SW
{
// Copies a region of source to a region of destination, performing nearest-neighbor rescaling
template <typename T>
void CopyRegion(const T* const source, const MathUtil::Rectangle<int>& srcrect, const int src_width,
                const int src_height, T* destination, const MathUtil::Rectangle<int>& dstrect,
                const int dst_width, const int dst_height)
{
  ASSERT(srcrect.top >= 0 && srcrect.bottom <= src_height);
  ASSERT(srcrect.left >= 0 && srcrect.right <= src_width);
  ASSERT(dstrect.top >= 0 && dstrect.bottom <= dst_height);
  ASSERT(dstrect.left >= 0 && dstrect.right <= dst_width);

  int copy_width = dstrect.GetWidth();
  int copy_height = dstrect.GetHeight();

  double x_ratio = srcrect.GetWidth() / static_cast<double>(dstrect.GetWidth());
  double y_ratio = srcrect.GetHeight() / static_cast<double>(dstrect.GetHeight());

  for (int y_off = 0; y_off < copy_height; y_off++)
  {
    for (int x_off = 0; x_off < copy_width; x_off++)
    {
      int dst_x = dstrect.left + x_off;
      int dst_y = dstrect.top + y_off;
      int dst_offset = (dst_y * dst_width) + dst_x;

      int src_x = srcrect.left + static_cast<int>(std::round(x_off * x_ratio));
      int src_y = srcrect.top + static_cast<int>(std::round(y_off * y_ratio));
      int src_offset = (src_y * src_width) + src_x;

      destination[dst_offset] = source[src_offset];
    }
  }
}

// Copies the entire image from source to destination, performing nearest-neighbor rescaling
template <typename T>
void CopyRegion(const T* const source, const int src_width, const int src_height, T* destination,
                const int dst_width, const int dst_height)
{
  MathUtil::Rectangle<int> srcrect{0, 0, src_width, src_height};
  MathUtil::Rectangle<int> dstrect{0, 0, dst_width, dst_height};
  CopyRegion(source, srcrect, src_width, src_height, destination, dstrect, dst_width, dst_height);
}
}  // namespace SW
