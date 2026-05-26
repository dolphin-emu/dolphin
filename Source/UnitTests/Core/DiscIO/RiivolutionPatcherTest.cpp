// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <string>

#include "Common/FileUtil.h"
#include "DiscIO/RiivolutionPatcher.h"

namespace
{
constexpr char kSubdir[] = "test";
constexpr char kRelativePath[] = "/test";
}

class RiivolutionPatcherTest : public testing::Test
{
protected:
  RiivolutionPatcherTest()
      : m_sd_root(File::CreateTempDir()), m_subdir(m_sd_root + "/" + kSubdir),
        m_xml_path(m_subdir + "/patch.xml")
  {
  }

  ~RiivolutionPatcherTest() override
  {
    if (!m_sd_root.empty())
      File::DeleteDirRecursively(m_sd_root);
  }

  void SetUp() override
  {
    ASSERT_FALSE(m_sd_root.empty());
    ASSERT_TRUE(File::CreateDir(m_subdir));
    ASSERT_TRUE(File::CreateEmptyFile(m_xml_path));
  }

  const std::string m_sd_root;
  const std::string m_subdir;
  const std::string m_xml_path;
};

TEST_F(RiivolutionPatcherTest, ResolvesAbsolutePathWhenSdRootHasTrailingSlash)
{
  // D_RIIVOLUTION_IDX always ends with '/'
  DiscIO::Riivolution::FileDataLoaderHostFS loader(m_sd_root + "/", m_xml_path, {});
  EXPECT_EQ(loader.GetFolderContents(kRelativePath).size(), 1u);
}
