#pragma once
#include <DiscIO/VolumeGC.h>
namespace DiscIO
{
void ConvertDisc(std::string original_path, std::string dst_path, DiscIO::BlobType format,
                       int compression_level, bool delete_original);
}
