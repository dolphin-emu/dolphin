// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <gtest/gtest.h>

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

  void SetUp()
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
