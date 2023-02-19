#include "rc_hash.h"

#include "../test_framework.h"
#include "data.h"
#include "mock_filereader.h"

#include <stdlib.h>

extern struct rc_hash_cdreader* cdreader;

/* as defined in cdreader.c */
typedef struct cdrom_t
{
  void* file_handle;
  int sector_size;
  int sector_header_size;
  int64_t file_track_offset;
  int track_first_sector;
  int track_pregap_sectors;
#ifndef NDEBUG
  uint32_t track_id;
#endif
} cdrom_t;

static const unsigned char sync_pattern[] = {
  0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
};

static char cue_single_track[] =
  "FILE \"game.bin\" BINARY\n"
  "  TRACK 01 MODE2/2352\n"
  "    INDEX 01 00:00:00\n";

static char cue_single_bin_multiple_data[] =
  "FILE \"game.bin\" BINARY\n"
  "  TRACK 01 AUDIO\n"
  "    INDEX 01 00:00:00\n"
  "  TRACK 02 MODE1/2352\n"
  "    PREGAP 00:03:00\n"
  "    INDEX 01 00:55:45\n"
  "  TRACK 03 MODE1/2352\n"
  "    INDEX 01 11:30:74\n"
  "  TRACK 04 MODE1/2352\n"
  "    INDEX 01 13:31:51\n"
  "  TRACK 05 MODE1/2352\n"
  "    INDEX 01 13:48:56\n"
  "  TRACK 06 MODE1/2352\n"
  "    INDEX 01 34:48:19\n"
  "  TRACK 07 MODE1/2352\n"
  "    INDEX 01 50:42:74\n"
  "  TRACK 08 MODE1/2352\n"
  "    INDEX 01 55:20:74\n"
  "  TRACK 09 MODE1/2352\n"
  "    INDEX 01 56:25:67\n"
  "  TRACK 10 MODE1/2352\n"
  "    INDEX 01 59:04:08\n"
  "  TRACK 11 MODE1/2352\n"
  "    INDEX 01 61:17:18\n"
  "  TRACK 12 MODE1/2352\n"
  "    INDEX 01 62:44:33\n"
  "  TRACK 13 AUDIO\n"
  "    PREGAP 00:02:00\n"
  "    INDEX 01 66:24:37\n";

static char cue_multiple_bin_multiple_data[] =
  "FILE \"track1.bin\" BINARY\n"
  "  TRACK 01 AUDIO\n"
  "    INDEX 01 00:00:00\n"
  "FILE \"track2.bin\" BINARY\n"
  "  TRACK 02 MODE1/2352\n"
  "    INDEX 00 00:00:00\n"
  "    INDEX 01 00:03:00\n"
  "FILE \"track3.bin\" BINARY\n"
  "  TRACK 03 MODE1/2352\n"
  "    INDEX 00 00:00:00\n"
  "    INDEX 01 00:02:00\n"
  "FILE \"track4.bin\" BINARY\n"
  "  TRACK 04 AUDIO\n"
  "    INDEX 00 00:00:00\n";

static char gdi_three_tracks[] =
  "3\n"
  "1 0 4 2352 track01.bin 0\n"
  "2 600 0 2352 track02.raw 0\n"
  "3 45000 4 2352 track03.bin 0";

static char gdi_many_tracks[] =
  "26\n"
  "1 0 4 2352 track01.bin 0\n"
  "2 450 0 2352 track02.raw 0\n"
  "3 45000 4 2352 track03.bin 0\n"
  "4 370673 0 2352 track04.raw 0\n"
  "5 371347 0 2352 track05.raw 0\n"
  "6 372014 0 2352 track06.raw 0\n"
  "7 372915 0 2352 track07.raw 0\n"
  "8 373626 0 2352 track08.raw 0\n"
  "9 379011 0 2352 track09.raw 0\n"
  "10 384738 0 2352 track10.raw 0\n"
  "11 390481 0 2352 track11.raw 0\n"
  "12 395473 0 2352 track12.raw 0\n"
  "13 398926 0 2352 track13.raw 0\n"
  "14 404448 0 2352 track14.raw 0\n"
  "15 425246 0 2352 track15.raw 0\n"
  "16 445520 0 2352 track16.raw 0\n"
  "17 466032 0 2352 track17.raw 0\n"
  "18 474231 0 2352 track18.raw 0\n"
  "19 485598 0 2352 track19.raw 0\n"
  "20 486386 0 2352 track20.raw 0\n"
  "21 487098 0 2352 track21.raw 0\n"
  "22 487822 0 2352 track22.raw 0\n"
  "23 498356 0 2352 track23.raw 0\n"
  "24 508297 0 2352 track24.raw 0\n"
  "25 527383 0 2352 track25.raw 0\n"
  "26 548106 4 2352 track26.bin 0\n";

static void test_open_cue_track_2()
{
  cdrom_t* track_handle;

  mock_file_text(0, "game.cue", cue_single_bin_multiple_data);
  mock_empty_file(1, "game.bin", 718310208);

  track_handle = (cdrom_t*)cdreader->open_track("game.cue", 2);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "game.bin");
  ASSERT_NUM64_EQUALS(track_handle->file_track_offset, 9807840); /* track 2: 0x95A7E0 */
  ASSERT_NUM_EQUALS(track_handle->sector_size, 2352);
  ASSERT_NUM_EQUALS(track_handle->sector_header_size, 16);

  cdreader->close_track(track_handle);
}

static void test_open_cue_track_12()
{
  cdrom_t* track_handle;

  mock_file_text(0, "game.cue", cue_single_bin_multiple_data);
  mock_empty_file(1, "game.bin", 718310208);

  track_handle = (cdrom_t*)cdreader->open_track("game.cue", 12);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "game.bin");
  ASSERT_NUM64_EQUALS(track_handle->file_track_offset, 664047216); /* track 12: 0x27948E70 */
  ASSERT_NUM_EQUALS(track_handle->sector_size, 2352);
  ASSERT_NUM_EQUALS(track_handle->sector_header_size, 16);

  cdreader->close_track(track_handle);
}

static void test_open_cue_track_14()
{
  cdrom_t* track_handle;

  mock_file_text(0, "game.cue", cue_single_bin_multiple_data);
  mock_empty_file(1, "game.bin", 718310208);

  /* only 13 tracks */
  track_handle = (cdrom_t*)cdreader->open_track("game.cue", 14);
  ASSERT_PTR_NULL(track_handle);
}

static void test_open_cue_track_missing_bin()
{
  cdrom_t* track_handle;

  mock_file_text(0, "game.cue", cue_single_bin_multiple_data);

  track_handle = (cdrom_t*)cdreader->open_track("game.cue", 2);
  ASSERT_PTR_NULL(track_handle);
}

static void test_open_gdi_track_3()
{
  cdrom_t* track_handle;

  mock_file_text(0, "game.gdi", gdi_three_tracks);
  mock_empty_file(1, "track03.bin", 1185760800);

  track_handle = (cdrom_t*)cdreader->open_track("game.gdi", 3);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "track03.bin");
  ASSERT_NUM64_EQUALS(track_handle->track_pregap_sectors, 0);
  ASSERT_NUM_EQUALS(track_handle->track_first_sector, 45000);
  ASSERT_NUM_EQUALS(track_handle->sector_size, 2352);
  ASSERT_NUM_EQUALS(track_handle->sector_header_size, 16);

  cdreader->close_track(track_handle);
}

static void test_open_gdi_track_3_quoted()
{
  const char gdi_contents[] =
	"3\n"
	"1 0 4 2352 \"track 01.bin\" 0\n"
	"2 600 0 2352 \"track 02.raw\" 0\n"
	"3 45000 4 2352 \"track 03.bin\" 0";

  cdrom_t* track_handle;

  mock_file_text(0, "game.gdi", gdi_contents);
  mock_empty_file(1, "track 03.bin", 1185760800);

  track_handle = (cdrom_t*)cdreader->open_track("game.gdi", 3);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "track 03.bin");
  ASSERT_NUM64_EQUALS(track_handle->track_pregap_sectors, 0);
  ASSERT_NUM_EQUALS(track_handle->track_first_sector, 45000);
  ASSERT_NUM_EQUALS(track_handle->sector_size, 2352);
  ASSERT_NUM_EQUALS(track_handle->sector_header_size, 16);

  cdreader->close_track(track_handle);
}

static void test_open_gdi_track_3_extra_whitespace()
{
  const char gdi_contents[] =
	"3\n\n"
	"  1       0   4   2352   \"track 01.bin\"   0\n\n"
	"  2     600   0   2352   \"track 02.raw\"   0\n\n"
	"  3   45000   4   2352   \"track 03.bin\"   0\n\n";

  cdrom_t* track_handle;

  mock_file_text(0, "game.gdi", gdi_contents);
  mock_empty_file(1, "track 03.bin", 1185760800);

  track_handle = (cdrom_t*)cdreader->open_track("game.gdi", 3);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "track 03.bin");
  ASSERT_NUM64_EQUALS(track_handle->track_pregap_sectors, 0);
  ASSERT_NUM_EQUALS(track_handle->track_first_sector, 45000);
  ASSERT_NUM_EQUALS(track_handle->sector_size, 2352);
  ASSERT_NUM_EQUALS(track_handle->sector_header_size, 16);

  cdreader->close_track(track_handle);
}

static void test_open_gdi_track_last()
{
  cdrom_t* track_handle;

  mock_file_text(0, "game.gdi", gdi_many_tracks);
  mock_empty_file(1, "track26.bin", 2457600);

  track_handle = (cdrom_t*)cdreader->open_track("game.gdi", RC_HASH_CDTRACK_LAST);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "track26.bin");
  ASSERT_NUM64_EQUALS(track_handle->track_pregap_sectors, 0);
  ASSERT_NUM_EQUALS(track_handle->track_first_sector, 548106);
  ASSERT_NUM_EQUALS(track_handle->sector_size, 2352);
  ASSERT_NUM_EQUALS(track_handle->sector_header_size, 16);

  cdreader->close_track(track_handle);
}

static void test_open_cue_track_largest_data()
{
  cdrom_t* track_handle;

  mock_file_text(0, "game.cue", cue_single_bin_multiple_data);
  mock_empty_file(1, "game.bin", 718310208);

  track_handle = (cdrom_t*)cdreader->open_track("game.cue", RC_HASH_CDTRACK_LARGEST);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "game.bin");
  ASSERT_NUM64_EQUALS(track_handle->file_track_offset, 146190912); /* track 5: 0x8B6B240 */
  ASSERT_NUM_EQUALS(track_handle->sector_size, 2352);
  ASSERT_NUM_EQUALS(track_handle->sector_header_size, 16);

  cdreader->close_track(track_handle);
}

static void test_open_cue_track_largest_data_multiple_bin()
{
  cdrom_t* track_handle;

  mock_file_text(0, "game.cue", cue_multiple_bin_multiple_data);
  mock_empty_file(1, "track2.bin", 406423248);
  mock_empty_file(2, "track3.bin", 11553024);

  track_handle = (cdrom_t*)cdreader->open_track("game.cue", RC_HASH_CDTRACK_LARGEST);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "track2.bin");
  ASSERT_NUM64_EQUALS(track_handle->file_track_offset, 0);
  ASSERT_NUM_EQUALS(track_handle->track_pregap_sectors, 225); /* INDEX 01 00:03:00 */
  ASSERT_NUM_EQUALS(track_handle->sector_size, 2352);
  ASSERT_NUM_EQUALS(track_handle->sector_header_size, 16);

  cdreader->close_track(track_handle);
}

static void test_open_cue_track_largest_data_backwards_compatibility()
{
  cdrom_t* track_handle;
	
  mock_file_text(0, "game.cue", cue_single_bin_multiple_data);
  mock_empty_file(1, "game.bin", 718310208);

  /* before defining the enum, 0 meant largest */
  track_handle = (cdrom_t*)cdreader->open_track("game.cue", 0);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "game.bin");
  ASSERT_NUM64_EQUALS(track_handle->file_track_offset, 146190912); /* track 5: 0x8B6B240 */
  ASSERT_NUM_EQUALS(track_handle->sector_size, 2352);
  ASSERT_NUM_EQUALS(track_handle->sector_header_size, 16);

  cdreader->close_track(track_handle);
}

static void test_open_cue_track_largest_data_last_track()
{
  cdrom_t* track_handle;
  const char cue[] =
	  "FILE \"game.bin\" BINARY\n"
	  "  TRACK 01 AUDIO\n"
	  "    INDEX 01 00:00:00\n"
	  "  TRACK 02 MODE1/2352\n"
	  "    PREGAP 00:03:00\n"
	  "    INDEX 01 00:55:45\n"
	  "  TRACK 03 MODE1/2352\n"
	  "    INDEX 01 11:30:74\n"
	  "  TRACK 04 MODE1/2352\n"
	  "    INDEX 01 13:31:51\n"
	  "  TRACK 05 MODE1/2352\n"
	  "    INDEX 01 13:48:56\n";

  mock_file_text(0, "game.cue", cue);
  mock_empty_file(1, "game.bin", 718310208);

  track_handle = (cdrom_t*)cdreader->open_track("game.cue", RC_HASH_CDTRACK_LARGEST);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "game.bin");
  ASSERT_NUM64_EQUALS(track_handle->file_track_offset, 146190912); /* track 5: 0x8B6B240 (13:48:56) */
  ASSERT_NUM_EQUALS(track_handle->sector_size, 2352);
  ASSERT_NUM_EQUALS(track_handle->sector_header_size, 16);

  cdreader->close_track(track_handle);
}

static void test_open_cue_track_largest_data_index0s()
{
  cdrom_t* track_handle;
  const char cue[] =
	  "FILE \"game.bin\" BINARY\n"
	  "  TRACK 01 AUDIO\n"
	  "    INDEX 01 00:00:00\n"
	  "  TRACK 02 MODE1/2352\n"
	  "    INDEX 00 00:44:65\n"
	  "    INDEX 01 00:47:65\n"
	  "  TRACK 03 AUDIO\n"
	  "    INDEX 00 01:19:52\n"
	  "    INDEX 01 01:21:52\n";

  mock_file_text(0, "game.cue", cue);
  mock_empty_file(1, "game.bin", 718310208);

  track_handle = (cdrom_t*)cdreader->open_track("game.cue", RC_HASH_CDTRACK_LARGEST);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "game.bin");
  ASSERT_NUM64_EQUALS(track_handle->file_track_offset, 7914480); /* track 2: 0x78C3F0 (00:44:65) */
  ASSERT_NUM_EQUALS(track_handle->track_pregap_sectors, 225);
  ASSERT_NUM_EQUALS(track_handle->sector_size, 2352);
  ASSERT_NUM_EQUALS(track_handle->sector_header_size, 16);

  cdreader->close_track(track_handle);
}

static void test_open_cue_track_largest_data_index2()
{
  cdrom_t* track_handle;
  const char cue[] =
	  "FILE \"game.bin\" BINARY\n"
	  "  TRACK 01 AUDIO\n"
	  "    INDEX 01 00:00:00\n"
	  "  TRACK 02 MODE1/2352\n"
	  "    INDEX 00 00:00:00\n"
	  "    INDEX 01 00:02:00\n"
	  "    INDEX 02 00:08:64\n";

  mock_file_text(0, "game.cue", cue);
  mock_empty_file(1, "game.bin", 718310208);

  track_handle = (cdrom_t*)cdreader->open_track("game.cue", RC_HASH_CDTRACK_LARGEST);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "game.bin");
  ASSERT_NUM64_EQUALS(track_handle->file_track_offset, 0);
  ASSERT_NUM_EQUALS(track_handle->track_pregap_sectors, 150); /* 00:02:00 = 150 frames in */
  ASSERT_NUM_EQUALS(track_handle->sector_size, 2352);
  ASSERT_NUM_EQUALS(track_handle->sector_header_size, 16);

  cdreader->close_track(track_handle);
}

static void test_open_cue_track_largest_data_multiple_bins()
{
  cdrom_t* track_handle;
	
  mock_file_text(0, "game.cue", cue_multiple_bin_multiple_data);
  mock_empty_file(1, "track1.bin", 4132464);
  mock_empty_file(2, "track2.bin", 30080102);
  mock_empty_file(3, "track3.bin", 40343152);
  mock_empty_file(4, "track4.bin", 47277552);

  track_handle = (cdrom_t*)cdreader->open_track("game.cue", 0);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "track3.bin");
  ASSERT_NUM64_EQUALS(track_handle->file_track_offset, 0);
  ASSERT_NUM_EQUALS(track_handle->track_pregap_sectors, 150); /* 00:02:00 = 150 frames in */
  ASSERT_NUM_EQUALS(track_handle->sector_size, 2352);
  ASSERT_NUM_EQUALS(track_handle->sector_header_size, 16);

  cdreader->close_track(track_handle);
}

static void test_open_cue_track_largest_data_only_audio()
{
  const char cue[] =
    "FILE \"track1.bin\" BINARY\n"
    "  TRACK 01 AUDIO\n"
    "    INDEX 01 00:00:00\n"
    "FILE \"track2.bin\" BINARY\n"
    "  TRACK 02 AUDIO\n"
    "    INDEX 00 00:00:00\n"
    "    INDEX 01 00:03:00\n"
    "FILE \"track3.bin\" BINARY\n"
    "  TRACK 03 AUDIO\n"
    "    INDEX 00 00:00:00\n"
    "    INDEX 01 00:02:00\n"
    "FILE \"track4.bin\" BINARY\n"
    "  TRACK 04 AUDIO\n"
    "    INDEX 00 00:00:00\n";
  cdrom_t* track_handle;

  mock_file_text(0, "game.cue", cue);
  mock_empty_file(1, "track1.bin", 4132464);
  mock_empty_file(2, "track2.bin", 30080102);
  mock_empty_file(3, "track3.bin", 40343152);
  mock_empty_file(4, "track4.bin", 47277552);

  track_handle = (cdrom_t*)cdreader->open_track("game.cue", 0);
  ASSERT_PTR_NULL(track_handle);
}

static void test_open_cue_track_first_data()
{
  cdrom_t* track_handle;

  mock_file_text(0, "game.cue", cue_single_bin_multiple_data);
  mock_empty_file(1, "game.bin", 718310208);

  track_handle = (cdrom_t*)cdreader->open_track("game.cue", RC_HASH_CDTRACK_FIRST_DATA);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "game.bin");
  ASSERT_NUM64_EQUALS(track_handle->file_track_offset, 9807840); /* track 2: 0x0095a7e0 (00:55:45) */
  ASSERT_NUM_EQUALS(track_handle->track_pregap_sectors, 0);
  ASSERT_NUM_EQUALS(track_handle->sector_size, 2352);
  ASSERT_NUM_EQUALS(track_handle->sector_header_size, 16);

  cdreader->close_track(track_handle);
}

static void test_determine_sector_size_sync(int sector_size)
{
  cdrom_t* track_handle;
  const size_t image_size = sector_size * 32;
  unsigned char* image = (unsigned char*)malloc(image_size);
  mock_file_text(0, "game.cue", cue_single_track);
  mock_file(1, "game.bin", image, image_size);

  memset(image, 0, image_size);
  memcpy(&image[sector_size * 16], sync_pattern, sizeof(sync_pattern));

  track_handle = (cdrom_t*)cdreader->open_track("game.cue", 1);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "game.bin");
  ASSERT_NUM64_EQUALS(track_handle->file_track_offset, 0);
  ASSERT_NUM_EQUALS(track_handle->sector_size, sector_size);
  ASSERT_NUM_EQUALS(track_handle->sector_header_size, 16);

  cdreader->close_track(track_handle);
  free(image);
}

static void test_determine_sector_size_sync_primary_volume_descriptor(int sector_size)
{
  cdrom_t* track_handle;
  const size_t image_size = sector_size * 32;
  unsigned char* image = (unsigned char*)malloc(image_size);
  mock_file_text(0, "game.cue", cue_single_track);
  mock_file(1, "game.bin", image, image_size);

  memset(image, 0, image_size);
  memcpy(&image[sector_size * 16], sync_pattern, sizeof(sync_pattern));
  memcpy(&image[sector_size * 16 + 25], "CD001", 5);

  track_handle = (cdrom_t*)cdreader->open_track("game.cue", 1);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "game.bin");
  ASSERT_NUM64_EQUALS(track_handle->file_track_offset, 0);
  ASSERT_NUM_EQUALS(track_handle->sector_size, sector_size);
  ASSERT_NUM_EQUALS(track_handle->sector_header_size, 24);

  cdreader->close_track(track_handle);
  free(image);
}

static void test_determine_sector_size_sync_primary_volume_descriptor_index0(int sector_size)
{
  char cue[] =
    "FILE \"game.bin\" BINARY\n"
    "  TRACK 01 MODE2/2352\n"
    "    INDEX 00 00:00:00\n"
    "    INDEX 01 00:02:00\n";

  cdrom_t* track_handle;
  const size_t image_size = sector_size * 200;
  unsigned char* image = (unsigned char*)malloc(image_size);
  mock_file_text(0, "game.cue", cue);
  mock_file(1, "game.bin", image, image_size);

  char sector_size_str[16];
  sprintf(sector_size_str, "%d", sector_size);
  memcpy(&cue[40], sector_size_str, 4);

  memset(image, 0, image_size);
  memcpy(&image[sector_size * (150 + 16)], sync_pattern, sizeof(sync_pattern));
  memcpy(&image[sector_size * (150 + 16) + 25], "CD001", 5);

  track_handle = (cdrom_t*)cdreader->open_track("game.cue", 1);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "game.bin");
  ASSERT_NUM64_EQUALS(track_handle->file_track_offset, 0);
  ASSERT_NUM_EQUALS(track_handle->track_pregap_sectors, 150);
  ASSERT_NUM_EQUALS(track_handle->sector_size, sector_size);
  ASSERT_NUM_EQUALS(track_handle->sector_header_size, 24);

  cdreader->close_track(track_handle);
  free(image);
}

static void test_determine_sector_size_sync_2048()
{
  cdrom_t* track_handle;
  const int sector_size = 2048;
  const size_t image_size = sector_size * 32;
  unsigned char* image = (unsigned char*)malloc(image_size);
  mock_file_text(0, "game.cue", cue_single_track);
  mock_file(1, "game.bin", image, image_size);

  memset(image, 0, image_size);

  /* 2048 byte sectors don't have a sync pattern - will use mode specified in header */
  track_handle = (cdrom_t*)cdreader->open_track("game.cue", 1);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "game.bin");
  ASSERT_NUM64_EQUALS(track_handle->file_track_offset, 0);
  ASSERT_NUM_EQUALS(track_handle->sector_size, 2352);
  ASSERT_NUM_EQUALS(track_handle->sector_header_size, 24);

  cdreader->close_track(track_handle);
  free(image);
}

static void test_determine_sector_size_sync_primary_volume_descriptor_2048()
{
  cdrom_t* track_handle;
  const int sector_size = 2048;
  const size_t image_size = sector_size * 32;
  unsigned char* image = (unsigned char*)malloc(image_size);
  mock_file_text(0, "game.cue", cue_single_track);
  mock_file(1, "game.bin", image, image_size);

  memset(image, 0, image_size);
  memcpy(&image[sector_size * 16 + 1], "CD001", 5);

  track_handle = (cdrom_t*)cdreader->open_track("game.cue", 1);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "game.bin");
  ASSERT_NUM64_EQUALS(track_handle->file_track_offset, 0);
  ASSERT_NUM_EQUALS(track_handle->sector_size, sector_size);
  ASSERT_NUM_EQUALS(track_handle->sector_header_size, 0);

  cdreader->close_track(track_handle);
  free(image);
}

static void test_determine_sector_size_sync_primary_volume_descriptor_index0_2048()
{
  char cue[] =
    "FILE \"game.bin\" BINARY\n"
    "  TRACK 01 MODE1/2048\n"
    "    INDEX 00 00:00:00\n"
    "    INDEX 01 00:02:00\n";

  cdrom_t* track_handle;
  const int sector_size = 2048;
  const size_t image_size = sector_size * 200;
  unsigned char* image = (unsigned char*)malloc(image_size);
  mock_file_text(0, "game.cue", cue);
  mock_file(1, "game.bin", image, image_size);

  char sector_size_str[16];
  sprintf(sector_size_str, "%d", sector_size);
  memcpy(&cue[40], sector_size_str, 4);

  memset(image, 0, image_size);
  memcpy(&image[sector_size * (150 + 16) + 1], "CD001", 5);

  track_handle = (cdrom_t*)cdreader->open_track("game.cue", 1);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "game.bin");
  ASSERT_NUM64_EQUALS(track_handle->file_track_offset, 0);
  ASSERT_NUM_EQUALS(track_handle->track_pregap_sectors, 150);
  ASSERT_NUM_EQUALS(track_handle->sector_size, sector_size);
  ASSERT_NUM_EQUALS(track_handle->sector_header_size, 0);

  cdreader->close_track(track_handle);
  free(image);
}

static void test_absolute_sector_to_track_sector_cue_pregap()
{
  const char cue[] =
    "FILE \"game1.bin\" BINARY\n"/* file contains 500 sectors of data [1176000 bytes] */
    "  TRACK 01 MODE2/2352\n"
    "    INDEX 00 00:00:00\n"    /* 150 pre-gap sectors */
    "    INDEX 01 00:02:00\n"    /* 350 sectors of data */
    "FILE \"game2.bin\" BINARY\n"
	"  TRACK 02 MODE2/2352\n"
	"    INDEX 00 00:00:00\n"    /* 150 pre-gap sectors */
	"    INDEX 01 00:02:00\n";

  cdrom_t* track_handle;
  const size_t image_size = 60 * 200;
  unsigned char* image = (unsigned char*)malloc(image_size);

  mock_file_text(0, "game.cue", cue);
  mock_file(1, "game1.bin", NULL, 500 * 2352);
  mock_file(2, "game2.bin", image, image_size);

  track_handle = (cdrom_t*)cdreader->open_track("game.cue", 2);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "game2.bin");

  /* pregap of second track starts at sector 500 */
  ASSERT_NUM_EQUALS(track_handle->track_first_sector, 500);
  ASSERT_NUM_EQUALS(track_handle->track_pregap_sectors, 150);

  /* data for second track starts at sector 650 */
  ASSERT_NUM_EQUALS(cdreader->first_track_sector(track_handle), 650);

  cdreader->close_track(track_handle);
  free(image);
}

static void test_absolute_sector_to_track_sector_gdi()
{
  cdrom_t* track_handle;
  mock_file_text(0, "game.gdi", gdi_many_tracks);
  mock_file(1, "track26.bin", NULL, 1234567);

  track_handle = (cdrom_t*)cdreader->open_track("game.gdi", 26);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "track26.bin");
  ASSERT_NUM_EQUALS(track_handle->track_first_sector, 548106);

  ASSERT_NUM_EQUALS(cdreader->first_track_sector(track_handle), 548106);

  cdreader->close_track(track_handle);
}

static void test_read_sector()
{
  char buffer[4096];
  cdrom_t* track_handle;
  const size_t image_size = 2352 * 32;
  unsigned char* image = (unsigned char*)malloc(image_size);
  int offset, i;

  mock_file_text(0, "game.cue", cue_single_track);
  mock_file(1, "game.bin", image, image_size);

  memset(image, 0, image_size);
  memcpy(&image[2352 * 16], sync_pattern, sizeof(sync_pattern));
  image[2352 * 16 + 12] = 0;
  image[2352 * 16 + 13] = 2;
  image[2352 * 16 + 14] = 0x16;
  image[2352 * 16 + 15] = 2;

  offset = 2352 * 1 + 16;
  for (i = 0; i < 26; i++)
  {
	memset(&image[offset], i + 'A', 256);
	offset += 256;

	if ((i % 8) == 7)
      offset += (2352 - 2048);
  }

  track_handle = (cdrom_t*)cdreader->open_track("game.cue", 1);
  ASSERT_PTR_NOT_NULL(track_handle);

  ASSERT_PTR_NOT_NULL(track_handle->file_handle);
  ASSERT_STR_EQUALS(get_mock_filename(track_handle->file_handle), "game.bin");
  ASSERT_NUM64_EQUALS(track_handle->track_pregap_sectors, 0);
  ASSERT_NUM_EQUALS(track_handle->sector_size, 2352);
  ASSERT_NUM_EQUALS(track_handle->sector_header_size, 16);

  /* read across multiple sectors */
  ASSERT_NUM_EQUALS(cdreader->read_sector(track_handle, 1, buffer, sizeof(buffer)), 4096);

  ASSERT_NUM_EQUALS(buffer[0], 'A');
  ASSERT_NUM_EQUALS(buffer[255], 'A');
  ASSERT_NUM_EQUALS(buffer[256], 'B');
  ASSERT_NUM_EQUALS(buffer[2047], 'H');
  ASSERT_NUM_EQUALS(buffer[2048], 'I');
  ASSERT_NUM_EQUALS(buffer[4095], 'P');

  /* read of partial sector */
  ASSERT_NUM_EQUALS(cdreader->read_sector(track_handle, 2, buffer, 10), 10);
  ASSERT_NUM_EQUALS(buffer[0], 'I');
  ASSERT_NUM_EQUALS(buffer[9], 'I');
  ASSERT_NUM_EQUALS(buffer[10], 'A');

  cdreader->close_track(track_handle);
  free(image);
}

/* ========================================================================= */

void test_cdreader(void) {
  TEST_SUITE_BEGIN();

  init_mock_filereader();
  rc_hash_init_default_cdreader();
  
  TEST(test_open_cue_track_2);
  TEST(test_open_cue_track_12);
  TEST(test_open_cue_track_14);
  TEST(test_open_cue_track_missing_bin);

  TEST(test_open_gdi_track_3);
  TEST(test_open_gdi_track_3_quoted);
  TEST(test_open_gdi_track_3_extra_whitespace);
  TEST(test_open_gdi_track_last);

  TEST(test_open_cue_track_largest_data);
  TEST(test_open_cue_track_largest_data_multiple_bin);
  TEST(test_open_cue_track_largest_data_backwards_compatibility);
  TEST(test_open_cue_track_largest_data_last_track);
  TEST(test_open_cue_track_largest_data_index0s);
  TEST(test_open_cue_track_largest_data_index2);
  TEST(test_open_cue_track_largest_data_multiple_bins);
  TEST(test_open_cue_track_largest_data_only_audio);

  TEST(test_open_cue_track_first_data);

  TEST_PARAMS1(test_determine_sector_size_sync, 2352);
  TEST_PARAMS1(test_determine_sector_size_sync_primary_volume_descriptor, 2352);
  TEST_PARAMS1(test_determine_sector_size_sync_primary_volume_descriptor_index0, 2352);

  TEST_PARAMS1(test_determine_sector_size_sync, 2336);
  TEST_PARAMS1(test_determine_sector_size_sync_primary_volume_descriptor, 2336);
  TEST_PARAMS1(test_determine_sector_size_sync_primary_volume_descriptor_index0, 2336);

  TEST(test_determine_sector_size_sync_2048);
  TEST(test_determine_sector_size_sync_primary_volume_descriptor_2048);
  TEST(test_determine_sector_size_sync_primary_volume_descriptor_index0_2048);

  TEST(test_absolute_sector_to_track_sector_cue_pregap);
  TEST(test_absolute_sector_to_track_sector_gdi);

  TEST(test_read_sector);

  TEST_SUITE_END();
}
