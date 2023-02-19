#include "rc_hash.h"

#include "../test_framework.h"
#include "data.h"
#include "mock_filereader.h"

#include <stdlib.h>

static int hash_mock_file(const char* filename, char hash[33], int console_id, const uint8_t* buffer, size_t buffer_size)
{
  mock_file(0, filename, buffer, buffer_size);

  return rc_hash_generate_from_file(hash, console_id, filename);
}

static void iterate_mock_file(struct rc_hash_iterator *iterator, const char* filename, const uint8_t* buffer, size_t buffer_size)
{
  mock_file(0, filename, buffer, buffer_size);

  rc_hash_initialize_iterator(iterator, filename, NULL, 0);
}

/* ========================================================================= */

static void test_hash_full_file(int console_id, const char* filename, size_t size, const char* expected_md5)
{
  uint8_t* image = generate_generic_file(size);
  char hash_buffer[33], hash_file[33], hash_iterator[33];

  /* test full buffer hash */
  int result_buffer = rc_hash_generate_from_buffer(hash_buffer, console_id, image, size);

  /* test full file hash */
  int result_file = hash_mock_file(filename, hash_file, console_id, image, size);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  iterate_mock_file(&iterator, filename, image, size);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_buffer, 1);
  ASSERT_STR_EQUALS(hash_buffer, expected_md5);

  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_unknown_format(int console_id, const char* path)
{
  char hash_file[33] = "", hash_iterator[33] = "";

  /* test file hash (won't match) */
  int result_file = rc_hash_generate_from_file(hash_file, console_id, path);

  /* test file identification from iterator (won't match) */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, path, NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 0);
  ASSERT_STR_EQUALS(hash_file, "");

  ASSERT_NUM_EQUALS(result_iterator, 0);
  ASSERT_STR_EQUALS(hash_iterator, "");
}

static void test_hash_m3u(int console_id, const char* filename, size_t size, const char* expected_md5)
{
  uint8_t* image = generate_generic_file(size);
  char hash_file[33], hash_iterator[33];
  const char* m3u_filename = "test.m3u";

  mock_file(0, filename, image, size);
  mock_file(1, m3u_filename, (uint8_t*)filename, strlen(filename));

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, console_id, m3u_filename);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, m3u_filename, NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_filename(int console_id, const char* path, const char* expected_md5)
{
  char hash_file[33], hash_iterator[33];

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, console_id, path);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, path, NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

/* ========================================================================= */

static void test_hash_3do_bin()
{
  size_t image_size;
  uint8_t* image = generate_3do_bin(1, 123456, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "9b2266b8f5abed9c12cce780750e88d6";

  mock_file(0, "game.bin", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_3DO, "game.bin");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  mock_file_size(0, 45678901); /* must be > 32MB for iterator to consider CD formats for bin */
  rc_hash_initialize_iterator(&iterator, "game.bin", NULL, 0);
  mock_file_size(0, image_size); /* change it back before doing the hashing */

  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_3do_cue()
{
  size_t image_size;
  uint8_t* image = generate_3do_bin(1, 9347, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "257d1d19365a864266b236214dbea29c";

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_3DO, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_3do_iso()
{
  size_t image_size;
  uint8_t* image = generate_3do_bin(1, 9347, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "257d1d19365a864266b236214dbea29c";

  mock_file(0, "game.iso", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_3DO, "game.iso");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.iso", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_3do_invalid_header()
{
  /* this is meant to simulate attempting to open a non-3DO CD. TODO: generate PSX CD */
  size_t image_size;
  uint8_t* image = generate_3do_bin(1, 12, &image_size);
  char hash_file[33];

  /* make the header not match */
  image[3] = 0x34;

  mock_file(0, "game.bin", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_3DO, "game.bin");

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 0);
}

static void test_hash_3do_launchme_case_insensitive()
{
  /* main executable for "Captain Quazar" is "launchme" */
  /* main executable for "Rise of the Robots" is "launchMe" */
  /* main executable for "Road Rash" is "LaunchMe" */
  /* main executable for "Sewer Shark" is "Launchme" */
  size_t image_size;
  uint8_t* image = generate_3do_bin(1, 6543, &image_size);
  char hash_file[33];
  const char* expected_md5 = "59622882e3261237e8a1e396825ae4f5";

  memcpy(&image[2048 + 0x14 + 0x48 + 0x20], "launchme", 8);
  mock_file(0, "game.bin", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_3DO, "game.bin");

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);
}

static void test_hash_3do_no_launchme()
{
  /* this case should not happen */
  size_t image_size;
  uint8_t* image = generate_3do_bin(1, 6543, &image_size);
  char hash_file[33];

  memcpy(&image[2048 + 0x14 + 0x48 + 0x20], "filename", 8);
  mock_file(0, "game.bin", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_3DO, "game.bin");

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 0);
}

static void test_hash_3do_long_directory()
{
  /* root directory for "Dragon's Lair" uses more than one sector */
  size_t image_size;
  uint8_t* image = generate_3do_bin(3, 6543, &image_size);
  char hash_file[33];
  const char* expected_md5 = "8979e876ae502e0f79218f7ff7bd8c2a";

  mock_file(0, "game.bin", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_3DO, "game.bin");

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);
}

/* ========================================================================= */

static void test_hash_arduboy()
{
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "67b64633285a7f965064ba29dab45148";

  const char* hex_input =
      ":100000000C94690D0C94910D0C94910D0C94910D20\n"
      ":100010000C94910D0C94910D0C94910D0C94910DE8\n"
      ":100020000C94910D0C94910D0C94C32A0C94352BC7\n"
      ":00000001FF\n";
  mock_file(0, "game.hex", (const uint8_t*)hex_input, strlen(hex_input));

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_ARDUBOY, "game.hex");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.hex", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_arduboy_crlf()
{
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "67b64633285a7f965064ba29dab45148";

  const char* hex_input =
      ":100000000C94690D0C94910D0C94910D0C94910D20\r\n"
      ":100010000C94910D0C94910D0C94910D0C94910DE8\r\n"
      ":100020000C94910D0C94910D0C94C32A0C94352BC7\r\n"
      ":00000001FF\r\n";
  mock_file(0, "game.hex", (const uint8_t*)hex_input, strlen(hex_input));

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_ARDUBOY, "game.hex");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.hex", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_arduboy_no_final_lf()
{
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "67b64633285a7f965064ba29dab45148";

  const char* hex_input =
      ":100000000C94690D0C94910D0C94910D0C94910D20\n"
      ":100010000C94910D0C94910D0C94910D0C94910DE8\n"
      ":100020000C94910D0C94910D0C94C32A0C94352BC7\n"
      ":00000001FF";
  mock_file(0, "game.hex", (const uint8_t*)hex_input, strlen(hex_input));

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_ARDUBOY, "game.hex");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.hex", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

/* ========================================================================= */

static void test_hash_atari_7800()
{
  size_t image_size;
  uint8_t* image = generate_atari_7800_file(16, 0, &image_size);
  char hash[33];
  int result = rc_hash_generate_from_buffer(hash, RC_CONSOLE_ATARI_7800, image, image_size);
  free(image);

  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "455f07d8500f3fabc54906737866167f");
  ASSERT_NUM_EQUALS(image_size, 16384);
}

static void test_hash_atari_7800_with_header()
{
  size_t image_size;
  uint8_t* image = generate_atari_7800_file(16, 1, &image_size);
  char hash[33];
  int result = rc_hash_generate_from_buffer(hash, RC_CONSOLE_ATARI_7800, image, image_size);
  free(image);

  /* NOTE: expectation is that this hash matches the hash in test_hash_atari_7800 */
  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "455f07d8500f3fabc54906737866167f");
  ASSERT_NUM_EQUALS(image_size, 16384 + 128);
}

/* ========================================================================= */

static void test_hash_dreamcast_single_bin()
{
  size_t image_size;
  uint8_t* image = generate_dreamcast_bin(45000, 1458208, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "2a550500caee9f06e5d061fe10a46f6e";

  mock_file(0, "track03.bin", image, image_size);
  mock_file_first_sector(0, 45000);
  mock_file(1, "game.gdi", (uint8_t*)"game.bin", 8);
  mock_cd_num_tracks(3);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_DREAMCAST, "game.gdi");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.gdi", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_dreamcast_split_bin()
{
  size_t image_size;
  uint8_t* image = generate_dreamcast_bin(548106, 1830912, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "771e56aff169230ede4505013a4bcf9f";

  mock_file(0, "game.gdi", (uint8_t*)"game.bin", 8);
  mock_file(1, "track03.bin", image, image_size);
  mock_file_first_sector(1, 45000);
  mock_file(2, "track26.bin", image, image_size);
  mock_file_first_sector(2, 548106);
  mock_cd_num_tracks(26);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_DREAMCAST, "game.gdi");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.gdi", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_dreamcast_cue()
{
  const char* cue_file =
      "FILE \"track01.bin\" BINARY\n"
      "  TRACK 01 MODE1/2352\n"
      "    INDEX 01 00:00:00\n"
      "FILE \"track02.bin\" BINARY\n"
      "  TRACK 02 AUDIO\n"
      "    INDEX 00 00:00:00\n"
      "    INDEX 01 00:02:00\n"
      "FILE \"track03.bin\" BINARY\n"
      "  TRACK 03 MODE1/2352\n"
      "    INDEX 01 00:00:00\n"
      "FILE \"track04.bin\" BINARY\n"
      "  TRACK 04 AUDIO\n"
      "    INDEX 00 00:00:00\n"
      "    INDEX 01 00:02:00\n"
      "FILE \"track05.bin\" BINARY\n"
      "  TRACK 05 MODE1/2352\n"
      "    INDEX 00 00:00:00\n"
      "    INDEX 01 00:03:00\n";
  size_t image_size;
  uint8_t* image = convert_to_2352(generate_dreamcast_bin(45000, 1697028, &image_size), &image_size, 45000);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "c952864c3364591d2a8793ce2cfbf3a0";

  mock_file(0, "game.cue", (uint8_t*)cue_file, strlen(cue_file));
  mock_file(1, "track01.bin", image, 1425312);    /* 606 sectors */
  mock_file(2, "track02.bin", image, 1589952);    /* 676 sectors */
  mock_file(3, "track03.bin", image, image_size); /* 737 sectors */
  mock_file(4, "track04.bin", image, 1237152);    /* 526 sectors */
  mock_file(5, "track05.bin", image, image_size);

  rc_hash_init_default_cdreader(); /* want to test actual first_track_sector calculation */

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_DREAMCAST, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);
  init_mock_cdreader();

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

/* ========================================================================= */

static void test_hash_nes_32k()
{
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 0, &image_size);
  char hash[33];
  int result = rc_hash_generate_from_buffer(hash, RC_CONSOLE_NINTENDO, image, image_size);
  free(image);

  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "6a2305a2b6675a97ff792709be1ca857");
  ASSERT_NUM_EQUALS(image_size, 32768);
}

static void test_hash_nes_32k_with_header()
{
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 1, &image_size);
  char hash[33];
  int result = rc_hash_generate_from_buffer(hash, RC_CONSOLE_NINTENDO, image, image_size);
  free(image);

  /* NOTE: expectation is that this hash matches the hash in test_hash_nes_32k */
  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "6a2305a2b6675a97ff792709be1ca857");
  ASSERT_NUM_EQUALS(image_size, 32768 + 16);
}

static void test_hash_nes_256k()
{
  size_t image_size;
  uint8_t* image = generate_nes_file(256, 0, &image_size);
  char hash[33];
  int result = rc_hash_generate_from_buffer(hash, RC_CONSOLE_NINTENDO, image, image_size);
  free(image);

  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "545d527301b8ae148153988d6c4fcb84");
  ASSERT_NUM_EQUALS(image_size, 262144);
}

static void test_hash_fds_two_sides()
{
  size_t image_size;
  uint8_t* image = generate_fds_file(2, 0, &image_size);
  char hash[33];
  int result = rc_hash_generate_from_buffer(hash, RC_CONSOLE_NINTENDO, image, image_size);
  free(image);

  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "fd770d4d34c00760fabda6ad294a8f0b");
  ASSERT_NUM_EQUALS(image_size, 65500 * 2);
}

static void test_hash_fds_two_sides_with_header()
{
  size_t image_size;
  uint8_t* image = generate_fds_file(2, 1, &image_size);
  char hash[33];
  int result = rc_hash_generate_from_buffer(hash, RC_CONSOLE_NINTENDO, image, image_size);
  free(image);

  /* NOTE: expectation is that this hash matches the hash in test_hash_fds_two_sides */
  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "fd770d4d34c00760fabda6ad294a8f0b");
  ASSERT_NUM_EQUALS(image_size, 65500 * 2 + 16);
}

static void test_hash_nes_file_32k()
{
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 0, &image_size);
  char hash[33];
  int result = hash_mock_file("test.nes", hash, RC_CONSOLE_NINTENDO, image, image_size);
  free(image);

  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "6a2305a2b6675a97ff792709be1ca857");
  ASSERT_NUM_EQUALS(image_size, 32768);
}

static void test_hash_nes_iterator_32k()
{
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 0, &image_size);
  char hash1[33], hash2[33];
  int result1, result2;
  struct rc_hash_iterator iterator;
  iterate_mock_file(&iterator, "test.nes", image, image_size);
  result1 = rc_hash_iterate(hash1, &iterator);
  result2 = rc_hash_iterate(hash2, &iterator);
  rc_hash_destroy_iterator(&iterator);
  free(image);

  ASSERT_NUM_EQUALS(result1, 1);
  ASSERT_STR_EQUALS(hash1, "6a2305a2b6675a97ff792709be1ca857");

  ASSERT_NUM_EQUALS(result2, 0);
  ASSERT_STR_EQUALS(hash2, "");
}

static void test_hash_nes_file_iterator_32k()
{
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 0, &image_size);
  char hash1[33], hash2[33];
  int result1, result2;
  struct rc_hash_iterator iterator;
  rc_hash_initialize_iterator(&iterator, "test.nes", image, image_size);
  result1 = rc_hash_iterate(hash1, &iterator);
  result2 = rc_hash_iterate(hash2, &iterator);
  rc_hash_destroy_iterator(&iterator);
  free(image);

  ASSERT_NUM_EQUALS(result1, 1);
  ASSERT_STR_EQUALS(hash1, "6a2305a2b6675a97ff792709be1ca857");

  ASSERT_NUM_EQUALS(result2, 0);
  ASSERT_STR_EQUALS(hash2, "");
}

/* ========================================================================= */

static void test_hash_n64(uint8_t* buffer, size_t buffer_size, const char* expected_hash)
{
  char hash[33];
  int result;

  rc_hash_reset_filereader(); /* explicitly unset the filereader */
  result = rc_hash_generate_from_buffer(hash, RC_CONSOLE_NINTENDO_64, buffer, buffer_size);
  init_mock_filereader(); /* restore the mock filereader */

  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, expected_hash);
}

static void test_hash_n64_file(const char* filename, uint8_t* buffer, size_t buffer_size, const char* expected_hash)
{
  char hash_file[33], hash_iterator[33];
  mock_file(0, filename, buffer, buffer_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_NINTENDO_64, filename);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, filename, NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_hash);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_hash);
}

/* ========================================================================= */

static void test_hash_nds()
{
  size_t image_size;
  uint8_t* image = generate_nds_file(2, 1234567, 654321, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_hash = "56b30c276cba4affa886bd38e8e34d7e";

  mock_file(0, "game.nds", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_NINTENDO_DS, "game.nds");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.nds", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_hash);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_hash);
}

static void test_hash_nds_supercard()
{
  size_t image_size;
  uint8_t* image = generate_nds_file(2, 1234567, 654321, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_hash = "56b30c276cba4affa886bd38e8e34d7e";

  /* inject the SuperCard header (512 bytes) */
  uint8_t* image2 = malloc(image_size + 512);
  memcpy(&image2[512], &image[0], image_size);
  memset(&image2[0], 0, 512);
  image2[0] = 0x2E;
  image2[1] = 0x00;
  image2[2] = 0x00;
  image2[3] = 0xEA;
  image2[0xB0] = 0x44;
  image2[0xB1] = 0x46;
  image2[0xB2] = 0x96;
  image2[0xB3] = 0x00;

  mock_file(0, "game.nds", image2, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_NINTENDO_DS, "game.nds");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.nds", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);
  free(image2);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_hash);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_hash);
}

static void test_hash_nds_buffered()
{
  size_t image_size;
  uint8_t* image = generate_nds_file(2, 1234567, 654321, &image_size);
  char hash_buffer[33], hash_iterator[33];
  const char* expected_hash = "56b30c276cba4affa886bd38e8e34d7e";

  /* test file hash */
  int result_buffer = rc_hash_generate_from_buffer(hash_buffer, RC_CONSOLE_NINTENDO_DS, image, image_size);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.nds", image, image_size);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_buffer, 1);
  ASSERT_STR_EQUALS(hash_buffer, expected_hash);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_hash);
}

/* ========================================================================= */

static void test_hash_pce_cd()
{
  size_t image_size;
  uint8_t* image = generate_pce_cd_bin(72, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "6565819195a49323e080e7539b54f251";

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PC_ENGINE, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_pce_cd_invalid_header()
{
  size_t image_size;
  uint8_t* image = generate_pce_cd_bin(72, &image_size);

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* make the header not match */
  image[2048 + 0x24] = 0x34;

  test_hash_unknown_format(RC_CONSOLE_PC_ENGINE, "game.cue");

  free(image);
}

/* ========================================================================= */

static void test_hash_pcfx()
{
  size_t image_size;
  uint8_t* image = generate_pcfx_bin(72, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "0a03af66559b8529c50c4e7788379598";

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PCFX, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_pcfx_invalid_header()
{
  size_t image_size;
  uint8_t* image = generate_pcfx_bin(72, &image_size);

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* make the header not match */
  image[12] = 0x34;

  test_hash_unknown_format(RC_CONSOLE_PC_ENGINE, "game.cue");

  free(image);
}

static void test_hash_pcfx_pce_cd()
{
  /* Battle Heat is formatted as a PC-Engine CD */
  size_t image_size;
  uint8_t* image = generate_pce_cd_bin(72, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "6565819195a49323e080e7539b54f251";

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);
  mock_file(2, "game2.bin", image, image_size); /* PC-Engine CD check only applies to track 2 */

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PCFX, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_psx_cd()
{
  size_t image_size;
  uint8_t* image = generate_psx_bin("SLUS_007.45", 0x07D800, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "db433fb038cde4fb15c144e8c7dea6e3";

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PLAYSTATION, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_psx_cd_no_system_cnf()
{
  size_t image_size;
  uint8_t* image;
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "e494c79a7315be0dc3e8571c45df162c";
  unsigned binary_size = 0x12000;
  const unsigned sectors_needed = (((binary_size + 2047) / 2048) + 20);
  uint8_t* exe;

  image = generate_iso9660_bin(sectors_needed, "HOMEBREW", &image_size);
  exe = generate_iso9660_file(image, "PSX.EXE", NULL, binary_size);
  memcpy(exe, "PS-X EXE", 8);
    binary_size -= 2048;
  exe[28] = binary_size & 0xFF;
  exe[29] = (binary_size >> 8) & 0xFF;
  exe[30] = (binary_size >> 16) & 0xFF;
  exe[31] = (binary_size >> 24) & 0xFF;

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PLAYSTATION, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_psx_cd_exe_in_subfolder()
{
  size_t image_size;
  uint8_t* image = generate_psx_bin("bin\\SCES_012.37", 0x07D800, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "674018e23a4052113665dfb264e9c2fc";

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PLAYSTATION, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_ps2_iso()
{
  size_t image_size;
  uint8_t* image = generate_ps2_bin("SLUS_200.64", 0x07D800, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "01a517e4ad72c6c2654d1b839be7579d";

  mock_file(0, "game.iso", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PLAYSTATION_2, "game.iso");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.iso", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_ps2_psx()
{
  size_t image_size;
  uint8_t* image = generate_psx_bin("SLUS_007.45", 0x07D800, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "db433fb038cde4fb15c144e8c7dea6e3";

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PLAYSTATION_2, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5); /* PSX hash */
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation (should not generate PS2 hash for PSX file) */
  ASSERT_NUM_EQUALS(result_file, 0);
  ASSERT_NUM_EQUALS(result_iterator, 0);
}

static void test_hash_psp()
{
  const size_t param_sfo_size = 690;
  uint8_t* param_sfo = generate_generic_file(param_sfo_size);
  const size_t eboot_bin_size = 273470;
  uint8_t* eboot_bin = generate_generic_file(eboot_bin_size);
  size_t image_size;
  uint8_t* image = generate_iso9660_bin(160, "TEST", &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "80c0b42b2d89d036086869433a176a03";

  generate_iso9660_file(image, "PSP_GAME\\PARAM.SFO", param_sfo, param_sfo_size);
  generate_iso9660_file(image, "PSP_GAME\\SYSDIR\\EBOOT.BIN", eboot_bin, eboot_bin_size);

  mock_file(0, "game.iso", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PSP, "game.iso");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.iso", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);
  free(eboot_bin);
  free(param_sfo);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_psp_video()
{
  const size_t param_sfo_size = 690;
  uint8_t* param_sfo = generate_generic_file(param_sfo_size);
  const size_t eboot_bin_size = 273470;
  uint8_t* eboot_bin = generate_generic_file(eboot_bin_size);
  size_t image_size;
  uint8_t* image = generate_iso9660_bin(160, "TEST", &image_size);
  char hash_file[33], hash_iterator[33];

  /* UMD video disc may have an UPDATE folder, but nothing in the PSP_GAME or SYSDIR folders. */
  generate_iso9660_file(image, "PSP_GAME\\SYSDIR\\UPDATE\\EBOOT.BIN", eboot_bin, eboot_bin_size);
  /* the PARAM.SFO file is in the UMD_VIDEO folder. */
  generate_iso9660_file(image, "UMD_VIDEO\\PARAM.SFO", param_sfo, param_sfo_size);

  mock_file(0, "game.iso", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PSP, "game.iso");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.iso", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);
  free(eboot_bin);
  free(param_sfo);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 0);
  ASSERT_NUM_EQUALS(result_iterator, 0);
}

static void test_hash_sega_cd()
{
  /* the first 512 bytes of sector 0 are a volume header and ROM header. 
   * generate a generic block and add the Sega CD marker */
  size_t image_size = 512;
  uint8_t* image = generate_generic_file(image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "574498e1453cb8934df60c4ab906e783";
  memcpy(image, "SEGADISCSYSTEM  ", 16);

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_SEGA_CD, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_sega_cd_invalid_header()
{
  size_t image_size = 512;
  uint8_t* image = generate_generic_file(image_size);

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  test_hash_unknown_format(RC_CONSOLE_SEGA_CD, "game.cue");

  free(image);
}

static void test_hash_saturn()
{
  /* the first 512 bytes of sector 0 are a volume header and ROM header. 
   * generate a generic block and add the Sega CD marker */
  size_t image_size = 512;
  uint8_t* image = generate_generic_file(image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "4cd9c8e41cd8d137be15bbe6a93ae1d8";
  memcpy(image, "SEGA SEGASATURN ", 16);

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_SATURN, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_saturn_invalid_header()
{
  size_t image_size = 512;
  uint8_t* image = generate_generic_file(image_size);

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  test_hash_unknown_format(RC_CONSOLE_SATURN, "game.cue");

  free(image);
}

/* ========================================================================= */

static void assert_valid_m3u(const char* disc_filename, const char* m3u_filename, const char* m3u_contents)
{
  const size_t size = 131072;
  uint8_t* image = generate_generic_file(size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "a0f425b23200568132ba76b2405e3933";

  mock_file(0, disc_filename, image, size);
  mock_file(1, m3u_filename, (uint8_t*)m3u_contents, strlen(m3u_contents));

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PC8800, m3u_filename);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, m3u_filename, NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_m3u_buffered()
{
  const size_t size = 131072;
  uint8_t* image = generate_generic_file(size);
  char hash_iterator[33];
  const char* m3u_filename = "test.m3u";
  const char* filename = "test.d88";
  const char* expected_md5 = "a0f425b23200568132ba76b2405e3933";
  uint8_t* m3u_contents = (uint8_t*)filename;
  const size_t m3u_size = strlen(filename);

  mock_file(0, filename, image, size);
  mock_file(1, m3u_filename, m3u_contents, m3u_size);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, m3u_filename, m3u_contents, m3u_size);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_m3u_with_comments()
{
  assert_valid_m3u("test.d88", "test.m3u", 
      "#EXTM3U\r\n\r\n#EXTBYT:131072\r\ntest.d88\r\n");
}

static void test_hash_m3u_empty()
{
  char hash_file[33], hash_iterator[33];
  const char* m3u_filename = "test.m3u";
  const char* m3u_contents = "#EXTM3U\r\n\r\n#EXTBYT:131072\r\n";

  mock_file(0, m3u_filename, (uint8_t*)m3u_contents, strlen(m3u_contents));

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PC8800, m3u_filename);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, m3u_filename, NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 0);
  ASSERT_NUM_EQUALS(result_iterator, 0);
}

static void test_hash_m3u_trailing_whitespace()
{
  assert_valid_m3u("test.d88", "test.m3u", 
      "#EXTM3U  \r\n  \r\n#EXTBYT:131072  \r\ntest.d88  \t  \r\n");
}

static void test_hash_m3u_line_ending()
{
  assert_valid_m3u("test.d88", "test.m3u", 
      "#EXTM3U\n\n#EXTBYT:131072\ntest.d88\n");
}

static void test_hash_m3u_extension_case()
{
  assert_valid_m3u("test.D88", "test.M3U", 
      "#EXTM3U\r\n\r\n#EXTBYT:131072\r\ntest.D88\r\n");
}

static void test_hash_m3u_relative_path()
{
  assert_valid_m3u("folder1/folder2/test.d88", "folder1/test.m3u", 
      "#EXTM3U\r\n\r\n#EXTBYT:131072\r\nfolder2/test.d88");
}

static void test_hash_m3u_absolute_path(const char* absolute_path)
{
  char m3u_contents[128] = "#EXTM3U\r\n\r\n#EXTBYT:131072\r\n";
  strcat(m3u_contents, absolute_path);

  assert_valid_m3u(absolute_path, "relative/test.m3u", m3u_contents);
}

static void test_hash_file_without_ext()
{
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 1, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* filename = "test";

  mock_file(0, filename, image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_NINTENDO, filename);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, filename, NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */

  /* specifying a console will use the appropriate hasher */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, "6a2305a2b6675a97ff792709be1ca857");

  /* no extension will use the default full file iterator, so hash should include header */
  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, "64b131c5c7fec32985d9c99700babb7e");
}

/* ========================================================================= */

void test_hash(void) {
  TEST_SUITE_BEGIN();

  init_mock_filereader();
  init_mock_cdreader();

  /* 3DO */
  TEST(test_hash_3do_bin);
  TEST(test_hash_3do_cue);
  TEST(test_hash_3do_iso);
  TEST(test_hash_3do_invalid_header);
  TEST(test_hash_3do_launchme_case_insensitive);
  TEST(test_hash_3do_no_launchme);
  TEST(test_hash_3do_long_directory);

  /* Amstrad CPC */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_AMSTRAD_PC, "test.dsk", 194816, "9d616e4ad3f16966f61422c57e22aadd");
  TEST_PARAMS4(test_hash_m3u, RC_CONSOLE_AMSTRAD_PC, "test.dsk", 194816, "9d616e4ad3f16966f61422c57e22aadd");

  /* Apple II */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_APPLE_II, "test.nib", 232960, "96e8d33bdc385fd494327d6e6791cbe4");
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_APPLE_II, "test.dsk", 143360, "88be638f4d78b4072109e55f13e8a0ac");
  TEST_PARAMS4(test_hash_m3u, RC_CONSOLE_APPLE_II, "test.dsk", 143360, "88be638f4d78b4072109e55f13e8a0ac");

  /* Arduboy */
  TEST(test_hash_arduboy);
  TEST(test_hash_arduboy_crlf);
  TEST(test_hash_arduboy_no_final_lf);

  /* Arcade */
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "game.7z", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "\\game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "roms\\game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "C:\\roms\\game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/roms/game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/games/game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/roms/game.7z", "c8d46d341bea4fd5bff866a65ff8aea9");

  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/nes_game.zip", "9b7aad36b365712fc93728088de4c209");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/nes/game.zip", "9b7aad36b365712fc93728088de4c209");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "C:\\roms\\nes\\game.zip", "9b7aad36b365712fc93728088de4c209");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "nes\\game.zip", "9b7aad36b365712fc93728088de4c209");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/snes/game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/nes2/game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");

  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/coleco/game.zip", "c546f63ae7de98add4b9f221a4749260");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/msx/game.zip", "59ab85f6b56324fd81b4e324b804c29f");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/pce/game.zip", "c414a783f3983bbe2e9e01d9d5320c7e");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/sgx/game.zip", "db545ab29694bfda1010317d4bac83b8");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/tg16/game.zip", "8b6c5c2e54915be2cdba63973862e143");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/fds/game.zip", "c0c135a97e8c577cfdf9204823ff211f");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/gamegear/game.zip", "f6f471e952b8103032b723f57bdbe767");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/sms/game.zip", "43f35f575dead94dd2f42f9caf69fe5a");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/megadriv/game.zip", "f99d0aaf12ba3eb6ced9878c76692c63");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/sg1000/game.zip", "e8f6c711c4371f09537b4f2a7a304d6c");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/spectrum/game.zip", "a5f62157b2617bd728c4b1bc885c29e9");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/ngp/game.zip", "d4133b74c4e57274ca514e27a370dcb6");

  /* Arcadia 2001 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_ARCADIA_2001, "test.bin", 4096, "572686c3a073162e4ec6eff86e6f6e3a");

  /* Atari 2600 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_ATARI_2600, "test.bin", 2048, "02c3f2fa186388ba8eede9147fb431c4");

  /* Atari 7800 */
  TEST(test_hash_atari_7800);
  TEST(test_hash_atari_7800_with_header);

  /* Atari Jaguar */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_ATARI_JAGUAR, "test.jag", 0x400000, "a247ec8a8c42e18fcb80702dfadac14b");

  /* Colecovision */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_COLECOVISION, "test.col", 16384, "455f07d8500f3fabc54906737866167f");

  /* Commodore 64 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_COMMODORE_64, "test.nib", 327936, "e7767d32b23e3fa62c5a250a08caeba3");
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_COMMODORE_64, "test.d64", 174848, "ecd5a8ef4e77f2e9469d9b6e891394f0");
  TEST_PARAMS4(test_hash_m3u, RC_CONSOLE_COMMODORE_64, "test.d64", 174848, "ecd5a8ef4e77f2e9469d9b6e891394f0");

  /* Dreamcast */
  TEST(test_hash_dreamcast_single_bin);
  TEST(test_hash_dreamcast_split_bin);
  TEST(test_hash_dreamcast_cue);

  /* Elektor TV Games Computer */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER, "test.pgm", 4096, "572686c3a073162e4ec6eff86e6f6e3a");
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER, "test.tvc", 1861, "37097124a29aff663432d049654a17dc");

  /* Fairchild Channel F */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_FAIRCHILD_CHANNEL_F, "test.bin", 2048, "02c3f2fa186388ba8eede9147fb431c4");
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_FAIRCHILD_CHANNEL_F, "test.chf", 2048, "02c3f2fa186388ba8eede9147fb431c4");

  /* Gameboy */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_GAMEBOY, "test.gb", 131072, "a0f425b23200568132ba76b2405e3933");

  /* Gameboy Color */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_GAMEBOY_COLOR, "test.gbc", 2097152, "cf86acf519625a25a17b1246975e90ae");

  /* Gameboy Advance */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_GAMEBOY_COLOR, "test.gba", 4194304, "a247ec8a8c42e18fcb80702dfadac14b");

  /* Game Gear */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_GAME_GEAR, "test.gg", 524288, "68f0f13b598e0b66461bc578375c3888");

  /* Intellivision */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_INTELLIVISION, "test.bin", 8192, "ce1127f881b40ce6a67ecefba50e2835");

  /* Interton VC 4000 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_INTERTON_VC_4000, "test.bin", 2048, "02c3f2fa186388ba8eede9147fb431c4");

  /* Magnavox Odyssey 2 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_MAGNAVOX_ODYSSEY2, "test.bin", 4096, "572686c3a073162e4ec6eff86e6f6e3a");

  /* Master System */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_MASTER_SYSTEM, "test.sms", 131072, "a0f425b23200568132ba76b2405e3933");

  /* Mega Drive */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_MEGA_DRIVE, "test.md", 1048576, "da9461b3b0f74becc3ccf6c2a094c516");
  TEST_PARAMS4(test_hash_m3u, RC_CONSOLE_MEGA_DRIVE, "test.md", 1048576, "da9461b3b0f74becc3ccf6c2a094c516");

  /* Mega Duck */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_MEGADUCK, "test.bin", 65536, "8e6576cd5c21e44e0bbfc4480577b040");

  /* MSX */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_MSX, "test.dsk", 737280, "0e73fe94e5f2e2d8216926eae512b7a6");
  TEST_PARAMS4(test_hash_m3u, RC_CONSOLE_MSX, "test.dsk", 737280, "0e73fe94e5f2e2d8216926eae512b7a6");

  /* Neo Geo Pocket */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_NEOGEO_POCKET, "test.ngc", 2097152, "cf86acf519625a25a17b1246975e90ae");

  /* NES */
  TEST(test_hash_nes_32k);
  TEST(test_hash_nes_32k_with_header);
  TEST(test_hash_nes_256k);
  TEST(test_hash_fds_two_sides);
  TEST(test_hash_fds_two_sides_with_header);

  TEST(test_hash_nes_file_32k);
  TEST(test_hash_nes_file_iterator_32k);
  TEST(test_hash_nes_iterator_32k);

  /* Nintendo 64 */
  TEST_PARAMS3(test_hash_n64, test_rom_z64, sizeof(test_rom_z64), "06096d7ce21cb6bcde38391534c4eb91");
  TEST_PARAMS3(test_hash_n64, test_rom_v64, sizeof(test_rom_v64), "06096d7ce21cb6bcde38391534c4eb91");
  TEST_PARAMS3(test_hash_n64, test_rom_n64, sizeof(test_rom_n64), "06096d7ce21cb6bcde38391534c4eb91");
  TEST_PARAMS4(test_hash_n64_file, "game.z64", test_rom_z64, sizeof(test_rom_z64), "06096d7ce21cb6bcde38391534c4eb91");
  TEST_PARAMS4(test_hash_n64_file, "game.v64", test_rom_v64, sizeof(test_rom_v64), "06096d7ce21cb6bcde38391534c4eb91");
  TEST_PARAMS4(test_hash_n64_file, "game.n64", test_rom_n64, sizeof(test_rom_n64), "06096d7ce21cb6bcde38391534c4eb91");
  TEST_PARAMS4(test_hash_n64_file, "game.n64", test_rom_z64, sizeof(test_rom_z64), "06096d7ce21cb6bcde38391534c4eb91"); /* misnamed */
  TEST_PARAMS4(test_hash_n64_file, "game.z64", test_rom_n64, sizeof(test_rom_n64), "06096d7ce21cb6bcde38391534c4eb91"); /* misnamed */

  /* Nintendo DS */
  TEST(test_hash_nds);
  TEST(test_hash_nds_supercard);
  TEST(test_hash_nds_buffered);

  /* Oric (no fixed file size) */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_ORIC, "test.tap", 18119, "953a2baa3232c63286aeae36b2172cef");

  /* PC-8800 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_PC8800, "test.d88", 348288, "8cca4121bf87200f45e91b905a9f5afd");
  TEST_PARAMS4(test_hash_m3u, RC_CONSOLE_PC8800, "test.d88", 348288, "8cca4121bf87200f45e91b905a9f5afd");

  /* PC Engine */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_PC_ENGINE, "test.pce", 524288, "68f0f13b598e0b66461bc578375c3888");
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_PC_ENGINE, "test.pce", 524288 + 512, "258c93ebaca1c3f488ab48218e5e8d38");

  /* PC Engine CD */
  TEST(test_hash_pce_cd);
  TEST(test_hash_pce_cd_invalid_header);

  /* PC-FX */
  TEST(test_hash_pcfx);
  TEST(test_hash_pcfx_invalid_header);
  TEST(test_hash_pcfx_pce_cd);

  /* Playstation */
  TEST(test_hash_psx_cd);
  TEST(test_hash_psx_cd_no_system_cnf);
  TEST(test_hash_psx_cd_exe_in_subfolder);

  /* Playstation 2 */
  TEST(test_hash_ps2_iso);
  TEST(test_hash_ps2_psx);

  /* Playstation Portable */
  TEST(test_hash_psp);
  TEST(test_hash_psp_video);

  /* Pokemon Mini */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_POKEMON_MINI, "test.min", 524288, "68f0f13b598e0b66461bc578375c3888");

  /* Sega CD */
  TEST(test_hash_sega_cd);
  TEST(test_hash_sega_cd_invalid_header);

  /* Sega 32X */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_SEGA_32X, "test.bin", 3145728, "07d733f252896ec41b4fd521fe610e2c");

  /* Sega Saturn */
  TEST(test_hash_saturn);
  TEST(test_hash_saturn_invalid_header);

  /* SG-1000 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_SG1000, "test.sg", 32768, "6a2305a2b6675a97ff792709be1ca857");

  /* SNES */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_SUPER_NINTENDO, "test.smc", 524288, "68f0f13b598e0b66461bc578375c3888");
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_SUPER_NINTENDO, "test.smc", 524288 + 512, "258c93ebaca1c3f488ab48218e5e8d38");

  /* TIC-80 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_TIC80, "test.tic", 67682, "79b96f4ffcedb3ce8210a83b22cd2c69");

  /* Vectrex */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_SG1000, "test.vec", 4096, "572686c3a073162e4ec6eff86e6f6e3a");

  /* VirtualBoy */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_SG1000, "test.vb", 524288, "68f0f13b598e0b66461bc578375c3888");

  /* Watara Supervision */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_SUPERVISION, "test.sv", 32768, "6a2305a2b6675a97ff792709be1ca857");

  /* WASM-4 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_WASM4, "test.wasm", 33454, "bce38bb5f05622fc7e0e56757059d180");

  /* WonderSwan */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_WONDERSWAN, "test.ws", 524288, "68f0f13b598e0b66461bc578375c3888");
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_WONDERSWAN, "test.wsc", 4194304, "a247ec8a8c42e18fcb80702dfadac14b");

  /* m3u support */
  TEST(test_hash_m3u_buffered);
  TEST(test_hash_m3u_with_comments);
  TEST(test_hash_m3u_empty);
  TEST(test_hash_m3u_trailing_whitespace);
  TEST(test_hash_m3u_line_ending);
  TEST(test_hash_m3u_extension_case);
  TEST(test_hash_m3u_relative_path);
  TEST_PARAMS1(test_hash_m3u_absolute_path, "/absolute/test.d88");
  TEST_PARAMS1(test_hash_m3u_absolute_path, "\\absolute\\test.d88");
  TEST_PARAMS1(test_hash_m3u_absolute_path, "C:\\absolute\\test.d88");
  TEST_PARAMS1(test_hash_m3u_absolute_path, "\\\\server\\absolute\\test.d88");
  TEST_PARAMS1(test_hash_m3u_absolute_path, "samba:/absolute/test.d88");

  /* other */
  TEST(test_hash_file_without_ext);

  TEST_SUITE_END();
}
