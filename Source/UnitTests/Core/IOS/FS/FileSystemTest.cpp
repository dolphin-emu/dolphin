// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/IOS.h"
#include "UICommon/UICommon.h"

using namespace IOS::HLE::FS;

constexpr Modes modes{Mode::ReadWrite, Mode::None, Mode::None};

class FileSystemTest : public testing::Test
{
protected:
  FileSystemTest() : m_profile_path{File::CreateTempDir()}
  {
    if (UserDirectoryCreationFailed())
    {
      return;
    }
    UICommon::SetUserDirectory(m_profile_path);
    m_fs = IOS::HLE::Kernel{}.GetFS();
  }

  ~FileSystemTest() override
  {
    if (UserDirectoryCreationFailed())
    {
      return;
    }
    m_fs.reset();
    File::DeleteDirRecursively(m_profile_path);
  }
  void SetUp() override
  {
    if (UserDirectoryCreationFailed())
    {
      FAIL();
    }
  }
  bool UserDirectoryCreationFailed() const { return m_profile_path.empty(); }

  std::shared_ptr<FileSystem> m_fs;

private:
  std::string m_profile_path;
};

TEST(FileSystem, BasicPathValidity)
{
  EXPECT_TRUE(IsValidPath("/"));
  EXPECT_FALSE(IsValidNonRootPath("/"));

  EXPECT_TRUE(IsValidNonRootPath("/shared2/sys/SYSCONF"));
  EXPECT_TRUE(IsValidNonRootPath("/shared2/sys"));
  EXPECT_TRUE(IsValidNonRootPath("/shared2"));

  // Paths must start with /.
  EXPECT_FALSE(IsValidNonRootPath("\\test"));
  // Paths must not end with /.
  EXPECT_FALSE(IsValidNonRootPath("/shared2/sys/"));
  // Paths must not be longer than 64 characters.
  EXPECT_FALSE(IsValidPath(
      "/abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"));
}

TEST(FileSystem, PathSplitting)
{
  SplitPathResult result;

  result = {"/shared1", "00000042.app"};
  EXPECT_EQ(SplitPathAndBasename("/shared1/00000042.app"), result);

  result = {"/shared2/sys", "SYSCONF"};
  EXPECT_EQ(SplitPathAndBasename("/shared2/sys/SYSCONF"), result);

  result = {"/shared2", "sys"};
  EXPECT_EQ(SplitPathAndBasename("/shared2/sys"), result);

  result = {"/", "shared2"};
  EXPECT_EQ(SplitPathAndBasename("/shared2"), result);
}

TEST_F(FileSystemTest, EssentialDirectories)
{
  for (const std::string path :
       {"/sys", "/ticket", "/title", "/shared1", "/shared2", "/tmp", "/import", "/meta"})
  {
    EXPECT_TRUE(m_fs->ReadDirectory(Uid{0}, Gid{0}, path).has_value()) << path;
  }
}

TEST_F(FileSystemTest, CreateFile)
{
  const std::string PATH = "/tmp/f";

  constexpr u8 ArbitraryAttribute = 0xE1;

  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, PATH, ArbitraryAttribute, modes), ResultCode::Success);

  const Result<Metadata> stats = m_fs->GetMetadata(Uid{0}, Gid{0}, PATH);
  ASSERT_TRUE(stats.has_value());
  EXPECT_TRUE(stats->is_file);
  EXPECT_EQ(stats->size, 0u);
  EXPECT_EQ(stats->uid, 0u);
  EXPECT_EQ(stats->gid, 0);
  EXPECT_EQ(stats->modes, modes);
  EXPECT_EQ(stats->attribute, ArbitraryAttribute);

  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, PATH, 0, modes), ResultCode::AlreadyExists);

  const Result<std::vector<std::string>> tmp_files = m_fs->ReadDirectory(Uid{0}, Gid{0}, "/tmp");
  ASSERT_TRUE(tmp_files.has_value());
  EXPECT_EQ(std::ranges::count(*tmp_files, "f"), 1u);

  // Test invalid paths
  // Unprintable characters
  EXPECT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/tmp/tes\1t", 0, modes), ResultCode::Invalid);
  EXPECT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/tmp/te\x7fst", 0, modes), ResultCode::Invalid);
  // Paths with too many components are not rejected for files.
  EXPECT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/1/2/3/4/5/6/7/8/9", 0, modes), ResultCode::NotFound);
}

TEST_F(FileSystemTest, CreateDirectory)
{
  const std::string PATH = "/tmp/d";

  constexpr u8 ArbitraryAttribute = 0x20;

  ASSERT_EQ(m_fs->CreateDirectory(Uid{0}, Gid{0}, PATH, ArbitraryAttribute, modes),
            ResultCode::Success);

  const Result<Metadata> stats = m_fs->GetMetadata(Uid{0}, Gid{0}, PATH);
  ASSERT_TRUE(stats.has_value());
  EXPECT_FALSE(stats->is_file);
  EXPECT_EQ(stats->uid, 0u);
  EXPECT_EQ(stats->gid, 0);
  EXPECT_EQ(stats->modes, modes);
  EXPECT_EQ(stats->attribute, ArbitraryAttribute);

  const Result<std::vector<std::string>> children = m_fs->ReadDirectory(Uid{0}, Gid{0}, PATH);
  ASSERT_TRUE(children.has_value());
  EXPECT_TRUE(children->empty());

  EXPECT_EQ(m_fs->CreateDirectory(Uid{0}, Gid{0}, PATH, 0, modes), ResultCode::AlreadyExists);

  // Paths with too many components should be rejected.
  EXPECT_EQ(m_fs->CreateDirectory(Uid{0}, Gid{0}, "/1/2/3/4/5/6/7/8/9", 0, modes),
            ResultCode::TooManyPathComponents);
}

TEST_F(FileSystemTest, Delete)
{
  EXPECT_TRUE(m_fs->ReadDirectory(Uid{0}, Gid{0}, "/tmp").has_value());
  EXPECT_EQ(m_fs->Delete(Uid{0}, Gid{0}, "/tmp"), ResultCode::Success);
  EXPECT_EQ(m_fs->ReadDirectory(Uid{0}, Gid{0}, "/tmp").error(), ResultCode::NotFound);

  // Test recursive directory deletion.
  ASSERT_EQ(m_fs->CreateDirectory(Uid{0}, Gid{0}, "/sys/1", 0, modes), ResultCode::Success);
  ASSERT_EQ(m_fs->CreateDirectory(Uid{0}, Gid{0}, "/sys/1/2", 0, modes), ResultCode::Success);
  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/sys/1/2/3", 0, modes), ResultCode::Success);
  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/sys/1/2/4", 0, modes), ResultCode::Success);

  // Leave a file open. Deletion should fail while the file is in use.
  auto handle = std::make_optional(m_fs->OpenFile(Uid{0}, Gid{0}, "/sys/1/2/3", Mode::Read));
  ASSERT_TRUE(handle->has_value());
  EXPECT_EQ(m_fs->Delete(Uid{0}, Gid{0}, "/sys/1/2/3"), ResultCode::InUse);
  // A directory that contains a file that is in use is considered to be in use,
  // so this should fail too.
  EXPECT_EQ(m_fs->Delete(Uid{0}, Gid{0}, "/sys/1"), ResultCode::InUse);

  // With the handle closed, both of these should work:
  handle.reset();
  EXPECT_EQ(m_fs->Delete(Uid{0}, Gid{0}, "/sys/1/2/3"), ResultCode::Success);
  EXPECT_EQ(m_fs->Delete(Uid{0}, Gid{0}, "/sys/1"), ResultCode::Success);
}

TEST_F(FileSystemTest, Rename)
{
  EXPECT_TRUE(m_fs->ReadDirectory(Uid{0}, Gid{0}, "/tmp").has_value());

  EXPECT_EQ(m_fs->Rename(Uid{0}, Gid{0}, "/tmp", "/test"), ResultCode::Success);

  EXPECT_EQ(m_fs->ReadDirectory(Uid{0}, Gid{0}, "/tmp").error(), ResultCode::NotFound);
  EXPECT_TRUE(m_fs->ReadDirectory(Uid{0}, Gid{0}, "/test").has_value());

  // Rename /test back to /tmp.
  EXPECT_EQ(m_fs->Rename(Uid{0}, Gid{0}, "/test", "/tmp"), ResultCode::Success);

  // Create a file called /tmp/f1, and rename it to /tmp/f2.
  // This should not work; file name changes are not allowed for files.
  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/tmp/f1", 0, modes), ResultCode::Success);
  EXPECT_EQ(m_fs->Rename(Uid{0}, Gid{0}, "/tmp/f1", "/tmp/f2"), ResultCode::Invalid);
}

TEST_F(FileSystemTest, RenameWithExistingTargetDirectory)
{
  // Test directory -> existing, non-empty directory.
  // IOS's FS sysmodule is not POSIX compliant and will remove the existing directory
  // if it exists, even when there are files in it.
  ASSERT_EQ(m_fs->CreateDirectory(Uid{0}, Gid{0}, "/tmp/d", 0, modes), ResultCode::Success);
  ASSERT_EQ(m_fs->CreateDirectory(Uid{0}, Gid{0}, "/tmp/d2", 0, modes), ResultCode::Success);
  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/tmp/d2/file", 0, modes), ResultCode::Success);
  EXPECT_EQ(m_fs->Rename(Uid{0}, Gid{0}, "/tmp/d", "/tmp/d2"), ResultCode::Success);

  EXPECT_EQ(m_fs->ReadDirectory(Uid{0}, Gid{0}, "/tmp/d").error(), ResultCode::NotFound);
  const Result<std::vector<std::string>> children = m_fs->ReadDirectory(Uid{0}, Gid{0}, "/tmp/d2");
  ASSERT_TRUE(children.has_value());
  EXPECT_TRUE(children->empty());
}

TEST_F(FileSystemTest, RenameWithExistingTargetFile)
{
  const std::string source_path = "/sys/f2";
  const std::string dest_path = "/tmp/f2";

  // Create the test source file and write some data (so that we can check its size later on).
  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, source_path, 0, modes), ResultCode::Success);
  const std::vector<u8> TEST_DATA{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}};
  std::vector<u8> read_buffer(TEST_DATA.size());
  {
    const Result<FileHandle> file = m_fs->OpenFile(Uid{0}, Gid{0}, source_path, Mode::ReadWrite);
    ASSERT_TRUE(file.has_value());
    ASSERT_TRUE(file->Write(TEST_DATA.data(), TEST_DATA.size()).has_value());
  }

  // Create the test target file and leave it empty.
  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, dest_path, 0, modes), ResultCode::Success);

  // Rename /sys/f2 to /tmp/f2 and check that f1 replaced f2.
  EXPECT_EQ(m_fs->Rename(Uid{0}, Gid{0}, source_path, dest_path), ResultCode::Success);

  ASSERT_FALSE(m_fs->GetMetadata(Uid{0}, Gid{0}, source_path).has_value());
  EXPECT_EQ(m_fs->GetMetadata(Uid{0}, Gid{0}, source_path).error(), ResultCode::NotFound);

  const Result<Metadata> metadata = m_fs->GetMetadata(Uid{0}, Gid{0}, dest_path);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_TRUE(metadata->is_file);
  EXPECT_EQ(metadata->size, TEST_DATA.size());
}

TEST_F(FileSystemTest, GetDirectoryStats)
{
  auto check_stats = [this](const u32 clusters, const u32 inodes) {
    const Result<DirectoryStats> stats = m_fs->GetDirectoryStats("/tmp");
    ASSERT_TRUE(stats.has_value());
    EXPECT_EQ(stats->used_clusters, clusters);
    EXPECT_EQ(stats->used_inodes, inodes);
  };

  check_stats(0u, 1u);

  EXPECT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/tmp/file", 0, modes), ResultCode::Success);
  // Still no clusters (because the file is empty), but 2 inodes now.
  check_stats(0u, 2u);

  {
    const Result<FileHandle> file = m_fs->OpenFile(Uid{0}, Gid{0}, "/tmp/file", Mode::Write);
    (void)file->Write(std::vector<u8>(20).data(), 20);
  }
  // The file should now take up one cluster.
  check_stats(1u, 2u);
}

// Files need to be explicitly created using CreateFile or CreateDirectory.
// Automatically creating them on first use would be a bug.
TEST_F(FileSystemTest, NonExistingFiles)
{
  const Result<Metadata> metadata = m_fs->GetMetadata(Uid{0}, Gid{0}, "/tmp/foo");
  ASSERT_FALSE(metadata.has_value());
  EXPECT_EQ(metadata.error(), ResultCode::NotFound);

  const Result<FileHandle> file = m_fs->OpenFile(Uid{0}, Gid{0}, "/tmp/foo", Mode::Read);
  ASSERT_FALSE(file.has_value());
  EXPECT_EQ(file.error(), ResultCode::NotFound);

  const Result<std::vector<std::string>> children = m_fs->ReadDirectory(Uid{0}, Gid{0}, "/foo");
  ASSERT_FALSE(children.has_value());
  EXPECT_EQ(children.error(), ResultCode::NotFound);
}

TEST_F(FileSystemTest, Seek)
{
  const std::vector<u8> TEST_DATA(10);

  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/tmp/f", 0, modes), ResultCode::Success);

  const Result<FileHandle> file = m_fs->OpenFile(Uid{0}, Gid{0}, "/tmp/f", Mode::ReadWrite);
  ASSERT_TRUE(file.has_value());

  // An empty file should have a size of exactly 0 bytes.
  EXPECT_EQ(file->GetStatus()->size, 0u);
  // The file position should be set to the start right after an open.
  EXPECT_EQ(file->GetStatus()->offset, 0u);

  // Write some dummy data.
  ASSERT_TRUE(file->Write(TEST_DATA.data(), TEST_DATA.size()).has_value());
  EXPECT_EQ(file->GetStatus()->size, TEST_DATA.size());
  EXPECT_EQ(file->GetStatus()->offset, TEST_DATA.size());

  auto seek_and_check = [&file](const u32 offset, const SeekMode mode,
                                const u32 expected_position) {
    const Result<u32> new_offset = file->Seek(offset, mode);
    ASSERT_TRUE(new_offset.has_value());
    EXPECT_EQ(*new_offset, expected_position);
    EXPECT_EQ(file->GetStatus()->offset, expected_position);
  };

  seek_and_check(0, SeekMode::Set, 0);
  seek_and_check(5, SeekMode::Set, 5);
  seek_and_check(0, SeekMode::Current, 5);
  seek_and_check(2, SeekMode::Current, 7);
  seek_and_check(0, SeekMode::End, 10);

  // Test past-EOF seeks.
  const Result<u32> new_position = file->Seek(11, SeekMode::Set);
  ASSERT_FALSE(new_position.has_value());
  EXPECT_EQ(new_position.error(), ResultCode::Invalid);
}

TEST_F(FileSystemTest, WriteAndSimpleReadback)
{
  const std::vector<u8> TEST_DATA{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}};
  std::vector<u8> read_buffer(TEST_DATA.size());

  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/tmp/f", 0, modes), ResultCode::Success);

  const Result<FileHandle> file = m_fs->OpenFile(Uid{0}, Gid{0}, "/tmp/f", Mode::ReadWrite);
  ASSERT_TRUE(file.has_value());

  // Write some test data.
  ASSERT_TRUE(file->Write(TEST_DATA.data(), TEST_DATA.size()).has_value());

  // Now read it back and make sure it is identical.
  ASSERT_TRUE(file->Seek(0, SeekMode::Set).has_value());
  ASSERT_TRUE(file->Read(read_buffer.data(), read_buffer.size()).has_value());
  EXPECT_EQ(TEST_DATA, read_buffer);
}

TEST_F(FileSystemTest, WriteAndRead)
{
  const std::vector<u8> TEST_DATA{{0xf, 1, 2, 3, 4, 5, 6, 7, 8, 9}};
  const u32 TEST_DATA_SIZE = static_cast<u32>(TEST_DATA.size());
  std::vector<u8> buffer(TEST_DATA_SIZE);

  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/tmp/f", 0, modes), ResultCode::Success);

  Result<FileHandle> tmp_handle = m_fs->OpenFile(Uid{0}, Gid{0}, "/tmp/f", Mode::ReadWrite);
  ASSERT_TRUE(tmp_handle.has_value());
  const Fd fd = tmp_handle->Release();

  // Try to read from an empty file. This should do nothing.
  // See https://github.com/dolphin-emu/dolphin/pull/4942
  Result<u32> read_result = m_fs->ReadBytesFromFile(fd, buffer.data(), TEST_DATA_SIZE);
  EXPECT_TRUE(read_result.has_value());
  EXPECT_EQ(*read_result, 0u);
  EXPECT_EQ(m_fs->GetFileStatus(fd)->offset, 0u);

  ASSERT_TRUE(m_fs->WriteBytesToFile(fd, TEST_DATA.data(), TEST_DATA_SIZE).has_value());
  EXPECT_EQ(m_fs->GetFileStatus(fd)->offset, TEST_DATA.size());

  // Try to read past EOF while we are at the end of the file. This should do nothing too.
  read_result = m_fs->ReadBytesFromFile(fd, buffer.data(), TEST_DATA_SIZE);
  EXPECT_TRUE(read_result.has_value());
  EXPECT_EQ(*read_result, 0u);
  EXPECT_EQ(m_fs->GetFileStatus(fd)->offset, TEST_DATA.size());

  // Go back to the start and try to read past EOF. This should read the entire file until EOF.
  ASSERT_TRUE(m_fs->SeekFile(fd, 0, SeekMode::Set).has_value());
  const u32 LARGER_TEST_DATA_SIZE = TEST_DATA_SIZE + 10;
  std::vector<u8> larger_buffer(LARGER_TEST_DATA_SIZE);
  read_result = m_fs->ReadBytesFromFile(fd, larger_buffer.data(), LARGER_TEST_DATA_SIZE);
  EXPECT_TRUE(read_result.has_value());
  EXPECT_EQ(*read_result, TEST_DATA.size());
  EXPECT_EQ(m_fs->GetFileStatus(fd)->offset, TEST_DATA.size());
}

TEST_F(FileSystemTest, MultipleHandles)
{
  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/tmp/f", 0, modes), ResultCode::Success);

  {
    const Result<FileHandle> file = m_fs->OpenFile(Uid{0}, Gid{0}, "/tmp/f", Mode::ReadWrite);
    ASSERT_TRUE(file.has_value());
    // Fill it with 10 zeroes.
    ASSERT_TRUE(file->Write(std::vector<u8>(10).data(), 10).has_value());
  }

  const Result<FileHandle> file1 = m_fs->OpenFile(Uid{0}, Gid{0}, "/tmp/f", Mode::ReadWrite);
  const Result<FileHandle> file2 = m_fs->OpenFile(Uid{0}, Gid{0}, "/tmp/f", Mode::ReadWrite);
  ASSERT_TRUE(file1.has_value());
  ASSERT_TRUE(file2.has_value());

  // Write some test data using one handle and make sure the data is seen by the other handle
  // (see issue 2917, 5232 and 8702 and https://github.com/dolphin-emu/dolphin/pull/2649).
  // Also make sure the file offsets are independent for each handle.

  const std::vector<u8> TEST_DATA{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}};
  EXPECT_EQ(file1->GetStatus()->offset, 0u);
  ASSERT_TRUE(file1->Write(TEST_DATA.data(), TEST_DATA.size()).has_value());
  EXPECT_EQ(file1->GetStatus()->offset, 10u);

  std::vector<u8> read_buffer(TEST_DATA.size());
  EXPECT_EQ(file2->GetStatus()->offset, 0u);
  ASSERT_TRUE(file2->Read(read_buffer.data(), read_buffer.size()).has_value());
  EXPECT_EQ(file2->GetStatus()->offset, 10u);
  EXPECT_EQ(TEST_DATA, read_buffer);
}

// ReadDirectory is used by official titles to determine whether a path is a file.
// If it is not a file, ResultCode::Invalid must be returned.
TEST_F(FileSystemTest, ReadDirectoryOnFile)
{
  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/tmp/f", 0, modes), ResultCode::Success);

  const Result<std::vector<std::string>> result = m_fs->ReadDirectory(Uid{0}, Gid{0}, "/tmp/f");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ResultCode::Invalid);
}

TEST_F(FileSystemTest, ReadDirectoryOrdering)
{
  ASSERT_EQ(m_fs->CreateDirectory(Uid{0}, Gid{0}, "/tmp/o", 0, modes), ResultCode::Success);

  // Randomly generated file names in no particular order.
  const std::array<std::string, 5> file_names{{
      "Rkj62lGwHp",
      "XGDQTDJMea",
      "1z5M43WeFw",
      "YAY39VuMRd",
      "hxJ86nkoBX",
  }};
  // Create the files.
  for (const auto& name : file_names)
    ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/tmp/o/" + name, 0, modes), ResultCode::Success);

  // Verify that ReadDirectory returns a file list that is ordered by descending creation date
  // (issue 10234).
  const Result<std::vector<std::string>> result = m_fs->ReadDirectory(Uid{0}, Gid{0}, "/tmp/o");
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), file_names.size());
  EXPECT_TRUE(std::equal(result->begin(), result->end(), file_names.rbegin()));
}

TEST_F(FileSystemTest, CreateFullPath)
{
  ASSERT_EQ(m_fs->CreateFullPath(Uid{0}, Gid{0}, "/tmp/a/b/c/d", 0, modes), ResultCode::Success);

  // Parent directories should be created by CreateFullPath.
  for (const std::string path : {"/tmp", "/tmp/a", "/tmp/a/b", "/tmp/a/b/c"})
    EXPECT_TRUE(m_fs->ReadDirectory(Uid{0}, Gid{0}, path).has_value());

  // If parent directories already exist, the call should still succeed.
  EXPECT_EQ(m_fs->CreateFullPath(Uid{0}, Gid{0}, "/tmp/a/b/c/d", 0, modes), ResultCode::Success);

  // If parent directories already exist and are owned by a different user,
  // CreateFullPath should still succeed.
  // See https://github.com/dolphin-emu/dolphin/pull/8593
  EXPECT_EQ(m_fs->CreateFullPath(Uid{0x1000}, Gid{1}, "/shared2/wc24/mbox/Readme.txt", 0, modes),
            ResultCode::Success);
}

TEST_F(FileSystemTest, DoState)
{
  const std::string TEST_DATA_1 = "123";
  const std::string TEST_DATA_2 = "4567";

  std::array<u8, 4> read_buffer;

  ASSERT_EQ(m_fs->CreateDirectory(Uid{1}, Gid{2}, "/tmp/a", 0, modes), ResultCode::Success);
  ASSERT_EQ(m_fs->CreateDirectory(Uid{1}, Gid{2}, "/tmp/a/b", 0, modes), ResultCode::Success);
  ASSERT_EQ(m_fs->CreateDirectory(Uid{0}, Gid{0}, "/tmp/a/c", 0, modes), ResultCode::Success);

  ASSERT_EQ(m_fs->CreateFile(Uid{1}, Gid{2}, "/tmp/a/d", 0, modes), ResultCode::Success);
  ASSERT_EQ(m_fs->CreateFile(Uid{3}, Gid{4}, "/tmp/e", 0, modes), ResultCode::Success);

  {
    Result<FileHandle> file1 = m_fs->OpenFile(Uid{1}, Gid{2}, "/tmp/a/d", Mode::ReadWrite);
    ASSERT_TRUE(file1.has_value());
    ASSERT_TRUE(file1->Write(TEST_DATA_1.data(), TEST_DATA_1.size()).has_value());
  }

  std::array<u8, 1024> state_buffer;
  size_t state_size;

  {
    Result<FileHandle> file2 = m_fs->OpenFile(Uid{3}, Gid{4}, "/tmp/e", Mode::ReadWrite);
    ASSERT_TRUE(file2.has_value());
    ASSERT_TRUE(file2->Write(TEST_DATA_2.data(), TEST_DATA_2.size()).has_value());

    u8* state_pointer = state_buffer.data();
    PointerWrap p(&state_pointer, state_buffer.size(), PointerWrap::Mode::Write);
    m_fs->DoState(p);
    ASSERT_TRUE(p.IsWriteMode());

    ASSERT_TRUE(file2->Seek(2, SeekMode::Set).has_value());
    ASSERT_TRUE(file2->Write("_", 1).has_value());

    Fd fd = file2->Release();
    p.Do(fd);
    ASSERT_TRUE(p.IsWriteMode());

    state_size = state_pointer - state_buffer.data();
  }

  ASSERT_EQ(m_fs->Delete(Uid{0}, Gid{0}, "/tmp/a"), ResultCode::Success);
  ASSERT_EQ(m_fs->GetMetadata(Uid{0}, Gid{0}, "/tmp/a").error(), ResultCode::NotFound);
  ASSERT_EQ(m_fs->GetMetadata(Uid{0}, Gid{0}, "/tmp/a/b").error(), ResultCode::NotFound);
  ASSERT_EQ(m_fs->GetMetadata(Uid{0}, Gid{0}, "/tmp/a/c").error(), ResultCode::NotFound);
  ASSERT_EQ(m_fs->GetMetadata(Uid{0}, Gid{0}, "/tmp/a/d").error(), ResultCode::NotFound);

  ASSERT_EQ(m_fs->CreateFile(Uid{5}, Gid{6}, "/tmp/f", 0, modes), ResultCode::Success);

  ASSERT_EQ(m_fs->CreateDirectory(Uid{7}, Gid{8}, "/tmp/g", 0, modes), ResultCode::Success);

  u8* state_pointer = state_buffer.data();
  PointerWrap p(&state_pointer, state_size, PointerWrap::Mode::Read);
  m_fs->DoState(p);
  ASSERT_TRUE(p.IsReadMode());

  constexpr auto check_directory_metadata = [](const Result<Metadata>& metadata, Uid uid, Gid gid) {
    ASSERT_TRUE(metadata.has_value());
    ASSERT_EQ(metadata->uid, uid);
    ASSERT_EQ(metadata->gid, gid);
    ASSERT_FALSE(metadata->is_file);
  };

  constexpr auto check_file_metadata = [](const Result<Metadata>& metadata, Uid uid, Gid gid,
                                          u32 size) {
    ASSERT_TRUE(metadata.has_value());
    ASSERT_EQ(metadata->uid, uid);
    ASSERT_EQ(metadata->gid, gid);
    ASSERT_TRUE(metadata->is_file);
    ASSERT_EQ(metadata->size, size);
  };

  check_directory_metadata(m_fs->GetMetadata(Uid{0}, Gid{0}, "/tmp/a"), Uid{1}, Gid{2});
  check_directory_metadata(m_fs->GetMetadata(Uid{0}, Gid{0}, "/tmp/a/b"), Uid{1}, Gid{2});
  check_directory_metadata(m_fs->GetMetadata(Uid{0}, Gid{0}, "/tmp/a/c"), Uid{0}, Gid{0});

  check_file_metadata(m_fs->GetMetadata(Uid{0}, Gid{0}, "/tmp/a/d"), Uid{1}, Gid{2}, 3);
  check_file_metadata(m_fs->GetMetadata(Uid{0}, Gid{0}, "/tmp/e"), Uid{3}, Gid{4}, 4);

  ASSERT_EQ(m_fs->GetMetadata(Uid{0}, Gid{0}, "/tmp/f").error(), ResultCode::NotFound);
  ASSERT_EQ(m_fs->GetMetadata(Uid{0}, Gid{0}, "/tmp/g").error(), ResultCode::NotFound);

  Fd fd{};
  p.Do(fd);
  ASSERT_TRUE(p.IsReadMode());

  {
    FileHandle file2(m_fs.get(), fd);
    ASSERT_EQ(file2.GetStatus()->offset, 4u);
    ASSERT_TRUE(file2.Seek(0, SeekMode::Set).has_value());
    ASSERT_TRUE(file2.Read(read_buffer.data(), TEST_DATA_2.size()).has_value());
    for (size_t i = 0; i < TEST_DATA_2.size(); ++i)
      ASSERT_EQ(read_buffer[i], TEST_DATA_2[i]);
  }

  {
    Result<FileHandle> file1 = m_fs->OpenFile(Uid{5}, Gid{6}, "/tmp/a/d", Mode::Read);
    ASSERT_TRUE(file1->Read(read_buffer.data(), TEST_DATA_1.size()).has_value());
    for (size_t i = 0; i < TEST_DATA_1.size(); ++i)
      ASSERT_EQ(read_buffer[i], TEST_DATA_1[i]);
  }
}
