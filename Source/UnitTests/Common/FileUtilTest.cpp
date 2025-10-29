// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <latch>
#include <thread>

#include <gtest/gtest.h>

#include "Common/BitUtils.h"
#include "Common/DirectIOFile.h"
#include "Common/FileUtil.h"

class FileUtilTest : public testing::Test
{
protected:
  FileUtilTest()
      : m_parent_directory(File::CreateTempDir()), m_file_path(m_parent_directory + "/file.txt"),
        m_directory_path(m_parent_directory + "/dir"),
        m_invalid_path(m_parent_directory + "/invalid.txt")
  {
  }

  ~FileUtilTest() override
  {
    if (!m_parent_directory.empty())
    {
      File::DeleteDirRecursively(m_parent_directory);
    }
  }

  void SetUp() override
  {
    if (m_parent_directory.empty())
    {
      FAIL();
    }
  }

  constexpr static std::array<File::IfAbsentBehavior, 2> s_warning_behaviors = {
      File::IfAbsentBehavior::ConsoleWarning,
      File::IfAbsentBehavior::NoConsoleWarning,
  };

  const std::string m_parent_directory;
  const std::string m_file_path;
  const std::string m_directory_path;
  const std::string m_invalid_path;
};

static void DeleteShouldNotRemoveDirectory(const std::string& path, File::IfAbsentBehavior behavior)
{
  File::CreateDir(path);
  EXPECT_FALSE(File::Delete(path, behavior));
  File::DeleteDir(path, behavior);
}

static void DeleteShouldRemoveFile(const std::string& path, File::IfAbsentBehavior behavior)
{
  File::CreateEmptyFile(path);
  EXPECT_TRUE(File::Delete(path, behavior));
}

static void DeleteShouldReturnTrueForInvalidPath(const std::string& path,
                                                 File::IfAbsentBehavior behavior)
{
  EXPECT_TRUE(File::Delete(path, behavior));
}

static void DeleteDirShouldRemoveDirectory(const std::string& path, File::IfAbsentBehavior behavior)
{
  File::CreateDir(path);
  EXPECT_TRUE(File::DeleteDir(path, behavior));
}

static void DeleteDirShouldNotRemoveFile(const std::string& path, File::IfAbsentBehavior behavior)
{
  File::CreateEmptyFile(path);
  EXPECT_FALSE(File::DeleteDir(path, behavior));
  File::Delete(path, behavior);
}

static void DeleteDirShouldReturnTrueForInvalidPath(const std::string& path,
                                                    File::IfAbsentBehavior behavior)
{
  EXPECT_TRUE(File::DeleteDir(path, behavior));
}

TEST_F(FileUtilTest, Delete)
{
  for (const auto behavior : s_warning_behaviors)
  {
    DeleteShouldNotRemoveDirectory(m_directory_path, behavior);
    DeleteShouldRemoveFile(m_file_path, behavior);
    DeleteShouldReturnTrueForInvalidPath(m_invalid_path, behavior);
  }
}

TEST_F(FileUtilTest, DeleteDir)
{
  for (const auto behavior : s_warning_behaviors)
  {
    DeleteDirShouldRemoveDirectory(m_directory_path, behavior);
    DeleteDirShouldNotRemoveFile(m_file_path, behavior);
    DeleteDirShouldReturnTrueForInvalidPath(m_invalid_path, behavior);
  }
}

TEST_F(FileUtilTest, CreateFullPath)
{
  ASSERT_TRUE(!m_directory_path.ends_with('/'));
  File::CreateDir(m_directory_path);

  // should try to create the directory at m_directory_path, which already exists
  EXPECT_TRUE(File::IsDirectory(m_directory_path));
  EXPECT_TRUE(File::CreateFullPath(m_directory_path + "/"));

  // should create the directory (one level)
  std::string p1 = m_directory_path + "/p1";
  EXPECT_TRUE(!File::Exists(p1));
  EXPECT_TRUE(File::CreateFullPath(p1 + "/"));
  EXPECT_TRUE(File::IsDirectory(p1));

  // should create the directories (multiple levels)
  std::string p2 = m_directory_path + "/p2";
  std::string p2a = m_directory_path + "/p2/a";
  std::string p2b = m_directory_path + "/p2/a/b";
  std::string p2c = m_directory_path + "/p2/a/b/c";
  EXPECT_TRUE(!File::Exists(p2));
  EXPECT_TRUE(!File::Exists(p2a));
  EXPECT_TRUE(!File::Exists(p2b));
  EXPECT_TRUE(!File::Exists(p2c));
  EXPECT_TRUE(File::CreateFullPath(p2c + "/"));
  EXPECT_TRUE(File::IsDirectory(p2));
  EXPECT_TRUE(File::IsDirectory(p2a));
  EXPECT_TRUE(File::IsDirectory(p2b));
  EXPECT_TRUE(File::IsDirectory(p2c));

  // if the given path ends in a file, the file should be ignored
  std::string p3 = m_directory_path + "/p3";
  std::string p3file = m_directory_path + "/p3/test.bin";
  EXPECT_TRUE(!File::Exists(p3));
  EXPECT_TRUE(!File::Exists(p3file));
  EXPECT_TRUE(File::CreateFullPath(p3file));
  EXPECT_TRUE(File::IsDirectory(p3));
  EXPECT_TRUE(!File::Exists(p3file));

  // if we try to create a directory where a file already exists, we expect it to fail and leave the
  // file alone
  EXPECT_TRUE(File::CreateEmptyFile(p3file));
  EXPECT_TRUE(File::IsFile(p3file));
  EXPECT_FALSE(File::CreateFullPath(p3file + "/"));
  EXPECT_TRUE(File::IsFile(p3file));
}

TEST_F(FileUtilTest, DirectIOFile)
{
  static constexpr std::array<u8, 3> u8_test_data = {42, 7, 99};
  static constexpr std::array<u16, 3> u16_test_data = {0xdead, 0xbeef, 0xf00d};

  static constexpr int u16_data_offset = 73;

  File::DirectIOFile file;
  EXPECT_FALSE(file.IsOpen());

  // Read mode fails with a non-existing file.
  EXPECT_FALSE(file.Open(m_file_path, File::AccessMode::Read));

  std::array<u8, u8_test_data.size()> u8_buffer = {};

  // Everything fails when a file isn't open.
  EXPECT_FALSE(file.Write(u8_buffer));
  EXPECT_FALSE(file.Read(u8_buffer));
  EXPECT_FALSE(file.Flush());
  EXPECT_FALSE(file.Seek(12, File::SeekOrigin::Begin));
  EXPECT_EQ(file.Tell(), 0);
  EXPECT_EQ(file.GetSize(), 0);

  // Fail to open non-existing file.
  EXPECT_FALSE(file.Open(m_file_path, File::AccessMode::ReadAndWrite));
  EXPECT_FALSE(file.Open(m_file_path, File::AccessMode::Read, File::OpenMode::Existing));
  EXPECT_FALSE(file.Open(m_file_path, File::AccessMode::Write, File::OpenMode::Existing));

  // Read mode can create files
  EXPECT_TRUE(file.Open(m_file_path, File::AccessMode::Read, File::OpenMode::Create));
  EXPECT_TRUE(file.Close());

  // Open a file for writing.
  EXPECT_TRUE(file.Open(m_file_path, File::AccessMode::Write, File::OpenMode::Always));
  EXPECT_TRUE(file.Flush());
  EXPECT_TRUE(file.IsOpen());

  // Note: Double Open() currently ASSERTs. It's not obvious if that should succeed or fail.

  EXPECT_TRUE(file.Close());
  EXPECT_FALSE(file.Open(m_file_path, File::AccessMode::Write, File::OpenMode::Create));

  EXPECT_TRUE(file.Open(m_file_path, File::AccessMode::Write, File::OpenMode::Existing));
  EXPECT_TRUE(file.Close());

  // Rename, Resize, and Delete fail with a closed handle.
  EXPECT_FALSE(Rename(file, m_file_path, "fail"));
  EXPECT_FALSE(Resize(file, 72));
  EXPECT_FALSE(Delete(file, m_file_path));
  EXPECT_TRUE(File::Exists(m_file_path));

  EXPECT_TRUE(file.Open(m_file_path, File::AccessMode::Write));

  EXPECT_TRUE(file.Write(u8_buffer.data(), 0));  // write of 0 succeeds.
  EXPECT_FALSE(file.Read(u8_buffer.data(), 0));  // read of 0 (in write mode) fails.

  // Resize through handle works.
  EXPECT_TRUE(Resize(file, 1));
  EXPECT_EQ(file.GetSize(), 1);
  file.Close();

  // Write truncates by default.
  EXPECT_TRUE(file.Open(m_file_path, File::AccessMode::Write));
  EXPECT_EQ(file.GetSize(), 0);

  EXPECT_TRUE(file.Write(u8_test_data));
  EXPECT_EQ(file.GetSize(), u8_test_data.size());  // size changed
  EXPECT_EQ(file.Tell(), u8_test_data.size());     // file position changed

  // Relative seek works.
  EXPECT_TRUE(file.Seek(0, File::SeekOrigin::End));
  EXPECT_EQ(file.Tell(), file.GetSize());
  EXPECT_TRUE(file.Seek(-int(u8_test_data.size()), File::SeekOrigin::Current));
  EXPECT_EQ(file.Tell(), 0);

  // Read while in "write mode" fails
  EXPECT_FALSE(file.Read(u8_buffer));
  EXPECT_EQ(file.Tell(), 0);

  // Seeking past the end works.
  EXPECT_TRUE(file.Seek(u16_data_offset, File::SeekOrigin::Begin));
  EXPECT_EQ(file.Tell(), u16_data_offset);
  EXPECT_EQ(file.GetSize(), u8_test_data.size());  // no change in size

  static constexpr u64 final_file_size = u16_data_offset + sizeof(u16_test_data);

  // Size changes after write.
  EXPECT_TRUE(file.Write(Common::AsU8Span(u16_test_data)));
  EXPECT_EQ(file.GetSize(), final_file_size);

  // Seek before begin fails.
  EXPECT_FALSE(file.Seek(-1, File::SeekOrigin::Begin));
  EXPECT_EQ(file.Tell(), file.GetSize());  // unchanged position

  EXPECT_TRUE(file.Close());
  EXPECT_FALSE(file.IsOpen());

  EXPECT_EQ(file.Tell(), 0);
  EXPECT_EQ(file.GetSize(), 0);

  EXPECT_FALSE(file.Close());  // Double close fails.

  // Open file for reading.
  EXPECT_TRUE(file.Open(m_file_path, File::AccessMode::Read, File::OpenMode::Always));
  EXPECT_TRUE(file.Close());
  EXPECT_TRUE(file.Open(m_file_path, File::AccessMode::Read));

  static constexpr int big_offset = 999;

  // We can seek beyond the end.
  EXPECT_TRUE(file.Seek(big_offset, File::SeekOrigin::End));
  EXPECT_EQ(file.Tell(), file.GetSize() + big_offset);

  // Resize through handle fails when in read mode.
  EXPECT_FALSE(Resize(file, big_offset));
  EXPECT_EQ(file.GetSize(), final_file_size);

  constexpr size_t thread_count = 4;

  File::DirectIOFile closed_file;

  std::latch do_reads{1};

  EXPECT_TRUE(file.Seek(u16_data_offset, File::SeekOrigin::Begin));

  // Concurrent access with copied handles works.
  std::vector<std::thread> threads(thread_count);
  for (auto& t : threads)
  {
    t = std::thread{[&do_reads, file, closed_file]() mutable {
      EXPECT_FALSE(closed_file.IsOpen());  // a copied closed file is still closed

      EXPECT_EQ(file.Tell(), u16_data_offset);  // current position is copied

      std::array<u8, 1> one_byte{};

      constexpr u64 small_offset = 3;

      do_reads.wait();

      EXPECT_TRUE(file.Seek(small_offset, File::SeekOrigin::Begin));
      EXPECT_EQ(file.Tell(), small_offset);

      // writes fail in read mode.
      EXPECT_FALSE(file.Write(u8_test_data));
      EXPECT_FALSE(file.OffsetWrite(0, u8_test_data.data(), 0));
      EXPECT_EQ(file.Tell(), small_offset);

      EXPECT_TRUE(file.Read(one_byte.data(), 0));  // read of zero succeeds.
      EXPECT_EQ(file.Tell(), small_offset);        // unchanged position

      // Reading at an explicit offset doesn't change the position
      EXPECT_TRUE(file.OffsetRead(u8_test_data.size() - 1, one_byte));
      EXPECT_EQ(file.Tell(), small_offset);
      EXPECT_EQ(one_byte[0], u8_test_data.back());

      EXPECT_EQ(file.GetSize(), final_file_size);
      EXPECT_TRUE(file.Seek(-int(sizeof(u16_test_data)), File::SeekOrigin::End));

      // We can't read beyond the end of the file.
      EXPECT_FALSE(file.OffsetRead(big_offset, one_byte));
      // A read of zero beyond the end works.
      EXPECT_TRUE(file.OffsetRead(big_offset, one_byte.data(), 0));

      // Reading the previously written data works.
      std::array<u16, u16_test_data.size()> u16_buffer = {};
      EXPECT_TRUE(file.Read(Common::AsWritableU8Span(u16_buffer)));
      EXPECT_TRUE(std::ranges::equal(u16_buffer, u16_test_data));

      // We can't read at the end of the file.
      EXPECT_FALSE(file.Read(one_byte));
      EXPECT_EQ(file.Tell(), file.GetSize());  // The position is unchanged.
    }};
  }

  // We may close the file on our end before the other threads use it.
  file.Close();

  // ReadAndWrite does not truncate existing files.
  EXPECT_TRUE(file.Open(m_file_path, File::AccessMode::ReadAndWrite));
  EXPECT_EQ(file.GetSize(), final_file_size);
  file.Close();

  // Write doesn't truncate when the mode is changed.
  EXPECT_TRUE(file.Open(m_file_path, File::AccessMode::Write, File::OpenMode::Existing));
  EXPECT_EQ(file.GetSize(), final_file_size);

  // Open a new handle to the same file.
  File::DirectIOFile second_handle(m_file_path, File::AccessMode::Write, File::OpenMode::Always);
  EXPECT_TRUE(second_handle.IsOpen());

  const std::string destination_path_1 = m_file_path + ".dest_1";

  // Rename through handle works when opened elsewhere.
  EXPECT_TRUE(Rename(file, m_file_path, destination_path_1));
  EXPECT_FALSE(File::Exists(m_file_path));
  EXPECT_TRUE(File::Exists(destination_path_1));

  const std::string destination_path_2 = m_file_path + ".dest_2";

  // Note: Windows fails the next `Rename` if this file is kept open.
  // I don't know if there is a nice way to make that work.
  {
    File::DirectIOFile another_file{destination_path_2, File::AccessMode::Write};
    EXPECT_TRUE(another_file.IsOpen());
  }

  // Rename overwrites existing files.
  EXPECT_TRUE(Rename(file, destination_path_1, destination_path_2));
  EXPECT_FALSE(File::Exists(destination_path_1));
  EXPECT_EQ(File::GetSize(destination_path_2), final_file_size);

  const std::string destination_path_3 = m_file_path + ".dest_3";

  // Truncate fails in Read mode.
  File::Copy(destination_path_2, destination_path_3);
  EXPECT_EQ(File::GetSize(destination_path_3), final_file_size);
  {
    File::DirectIOFile tmp{destination_path_3, File::AccessMode::Read, File::OpenMode::Truncate};
    EXPECT_FALSE(tmp.IsOpen());
    EXPECT_EQ(File::GetSize(destination_path_3), final_file_size);
  }

  // Truncate works with ReadAndWrite.
  EXPECT_EQ(File::GetSize(destination_path_3), final_file_size);
  {
    File::DirectIOFile tmp{destination_path_3, File::AccessMode::ReadAndWrite,
                           File::OpenMode::Truncate};
    EXPECT_EQ(tmp.GetSize(), 0);
    Delete(tmp, destination_path_3);
  }

  // Truncate works with Write.
  File::Copy(destination_path_2, destination_path_3);
  EXPECT_EQ(File::GetSize(destination_path_3), final_file_size);
  {
    File::DirectIOFile tmp{destination_path_3, File::AccessMode::Write, File::OpenMode::Truncate};
    EXPECT_EQ(tmp.GetSize(), 0);
    Delete(tmp, destination_path_3);
  }

  // Delete through handle works.
  EXPECT_TRUE(Delete(file, destination_path_2));
  EXPECT_FALSE(File::Exists(destination_path_2));

  // The threads can read even after deleting everything.
  do_reads.count_down();
  std::ranges::for_each(threads, &std::thread::join);

  file.Close();
  EXPECT_TRUE(file.Open(destination_path_1, File::AccessMode::Read, File::OpenMode::Always));

  // Required on Windows for the below to succeed.
  second_handle.Close();

  file.Close();
  EXPECT_TRUE(file.Open(destination_path_2, File::AccessMode::Write, File::OpenMode::Always));
}
