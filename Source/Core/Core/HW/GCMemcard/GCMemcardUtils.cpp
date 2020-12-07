#include "Core/HW/GCMemcard/GCMemcardUtils.h"

#include "Core/HW/GCMemcard/GCMemcard.h"

namespace Memcard
{
bool HasSameIdentity(const DEntry& lhs, const DEntry& rhs)
{
  // The Gamecube BIOS identifies two files as being 'the same' (that is, disallows copying from one
  // card to another when both contain a file like it) when the full array of all of m_gamecode,
  // m_makercode, and m_filename match between them.

  // However, despite that, it seems like the m_filename should be treated as a nullterminated
  // string instead, because:
  // - Games seem to identify their saves regardless of what bytes appear after the first null byte.
  // - If you have two files that match except for bytes after the first null in m_filename, the
  //   BIOS behaves oddly if you attempt to copy the files, as it seems to clear out those extra
  //   non-null bytes. See below for details.

  // Specifically, the following chain of actions fails with a rather vague 'The data may not have
  // been copied.' error message:
  // - Have two memory cards with one save file each.
  // - The two save files should have identical gamecode and makercode, as well as an equivalent
  //   filename up until and including the first null byte.
  // - On Card A have all remaining bytes of the filename also be null.
  // - On Card B have at least one of the remaining bytes be non-null.
  // - Copy the file on Card B to Card A.
  // The BIOS will abort halfway through the copy process and declare Card B as unusable until you
  // eject and reinsert it, and leave a "Broken File000" file on Card A, though note that the file
  // is not visible and will be cleaned up when reinserting the card while still within the BIOS.
  // Additionally, either during or after the copy process, the bytes after the first null on Card B
  // are changed to null, which is presumably why the copy process ends up failing as Card A would
  // then have two identical files. For reference, the Wii System Menu behaves exactly the same.

  // With all that in mind, even if it mismatches the comparison behavior of the BIOS, we treat
  // m_filename as a nullterminated string for determining if two files identify as the same, as not
  // doing so would cause more harm and confusion that good in practice.

  if (lhs.m_gamecode != rhs.m_gamecode)
    return false;
  if (lhs.m_makercode != rhs.m_makercode)
    return false;

  for (size_t i = 0; i < lhs.m_filename.size(); ++i)
  {
    const u8 a = lhs.m_filename[i];
    const u8 b = rhs.m_filename[i];
    if (a == 0)
      return b == 0;
    if (a != b)
      return false;
  }

  return true;
}
}  // namespace Memcard
