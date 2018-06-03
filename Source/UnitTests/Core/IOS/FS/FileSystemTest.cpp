// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <memory>
#include <string>

#include <gtest/gtest.h>

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
    UICommon::SetUserDirectory(m_profile_path);
    m_fs = IOS::HLE::Kernel{}.GetFS();
  }

  virtual ~FileSystemTest()
  {
    m_fs.reset();
    File::DeleteDirRecursively(m_profile_path);
  }

  std::shared_ptr<FileSystem> m_fs;

private:
  std::string m_profile_path;
};

TEST_F(FileSystemTest, EssentialDirectories)
{
  for (const std::string& path :
       {"/sys", "/ticket", "/title", "/shared1", "/shared2", "/tmp", "/import", "/meta"})
  {
    EXPECT_TRUE(m_fs->ReadDirectory(Uid{0}, Gid{0}, path).Succeeded()) << path;
  }
}

TEST_F(FileSystemTest, CreateFile)
{
  const std::string PATH = "/tmp/f";

  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, PATH, 0, modes), ResultCode::Success);

  const Result<Metadata> stats = m_fs->GetMetadata(Uid{0}, Gid{0}, PATH);
  ASSERT_TRUE(stats.Succeeded());
  EXPECT_TRUE(stats->is_file);
  EXPECT_EQ(stats->size, 0u);
  // TODO: After we start saving metadata correctly, check the UID, GID, permissions
  // as well (issue 10234).

  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, PATH, 0, modes), ResultCode::AlreadyExists);

  const Result<std::vector<std::string>> tmp_files = m_fs->ReadDirectory(Uid{0}, Gid{0}, "/tmp");
  ASSERT_TRUE(tmp_files.Succeeded());
  EXPECT_EQ(std::count(tmp_files->begin(), tmp_files->end(), "f"), 1u);
}

TEST_F(FileSystemTest, CreateDirectory)
{
  const std::string PATH = "/tmp/d";

  ASSERT_EQ(m_fs->CreateDirectory(Uid{0}, Gid{0}, PATH, 0, modes), ResultCode::Success);

  const Result<Metadata> stats = m_fs->GetMetadata(Uid{0}, Gid{0}, PATH);
  ASSERT_TRUE(stats.Succeeded());
  EXPECT_FALSE(stats->is_file);
  // TODO: After we start saving metadata correctly, check the UID, GID, permissions
  // as well (issue 10234).

  const Result<std::vector<std::string>> children = m_fs->ReadDirectory(Uid{0}, Gid{0}, PATH);
  ASSERT_TRUE(children.Succeeded());
  EXPECT_TRUE(children->empty());

  // TODO: uncomment this after the FS code is fixed to return AlreadyExists.
  // EXPECT_EQ(m_fs->CreateDirectory(Uid{0}, Gid{0}, PATH, 0, Mode::Read, Mode::None, Mode::None),
  //           ResultCode::AlreadyExists);
}

TEST_F(FileSystemTest, Delete)
{
  EXPECT_TRUE(m_fs->ReadDirectory(Uid{0}, Gid{0}, "/tmp").Succeeded());
  EXPECT_EQ(m_fs->Delete(Uid{0}, Gid{0}, "/tmp"), ResultCode::Success);
  EXPECT_EQ(m_fs->ReadDirectory(Uid{0}, Gid{0}, "/tmp").Error(), ResultCode::NotFound);
}

TEST_F(FileSystemTest, Rename)
{
  EXPECT_TRUE(m_fs->ReadDirectory(Uid{0}, Gid{0}, "/tmp").Succeeded());

  EXPECT_EQ(m_fs->Rename(Uid{0}, Gid{0}, "/tmp", "/test"), ResultCode::Success);

  EXPECT_EQ(m_fs->ReadDirectory(Uid{0}, Gid{0}, "/tmp").Error(), ResultCode::NotFound);
  EXPECT_TRUE(m_fs->ReadDirectory(Uid{0}, Gid{0}, "/test").Succeeded());
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

  EXPECT_EQ(m_fs->ReadDirectory(Uid{0}, Gid{0}, "/tmp/d").Error(), ResultCode::NotFound);
  const Result<std::vector<std::string>> children = m_fs->ReadDirectory(Uid{0}, Gid{0}, "/tmp/d2");
  ASSERT_TRUE(children.Succeeded());
  EXPECT_TRUE(children->empty());
}

TEST_F(FileSystemTest, RenameWithExistingTargetFile)
{
  // Create the test source file and write some data (so that we can check its size later on).
  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/tmp/f1", 0, modes), ResultCode::Success);
  const std::vector<u8> TEST_DATA{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}};
  std::vector<u8> read_buffer(TEST_DATA.size());
  {
    const Result<FileHandle> file = m_fs->OpenFile(Uid{0}, Gid{0}, "/tmp/f1", Mode::ReadWrite);
    ASSERT_TRUE(file.Succeeded());
    ASSERT_TRUE(file->Write(TEST_DATA.data(), TEST_DATA.size()).Succeeded());
  }

  // Create the test target file and leave it empty.
  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/tmp/f2", 0, modes), ResultCode::Success);

  // Rename f1 to f2 and check that f1 replaced f2.
  EXPECT_EQ(m_fs->Rename(Uid{0}, Gid{0}, "/tmp/f1", "/tmp/f2"), ResultCode::Success);

  ASSERT_FALSE(m_fs->GetMetadata(Uid{0}, Gid{0}, "/tmp/f1").Succeeded());
  EXPECT_EQ(m_fs->GetMetadata(Uid{0}, Gid{0}, "/tmp/f1").Error(), ResultCode::NotFound);

  const Result<Metadata> metadata = m_fs->GetMetadata(Uid{0}, Gid{0}, "/tmp/f2");
  ASSERT_TRUE(metadata.Succeeded());
  EXPECT_TRUE(metadata->is_file);
  EXPECT_EQ(metadata->size, TEST_DATA.size());
}

TEST_F(FileSystemTest, GetDirectoryStats)
{
  auto check_stats = [this](u32 clusters, u32 inodes) {
    const Result<DirectoryStats> stats = m_fs->GetDirectoryStats("/tmp");
    ASSERT_TRUE(stats.Succeeded());
    EXPECT_EQ(stats->used_clusters, clusters);
    EXPECT_EQ(stats->used_inodes, inodes);
  };

  check_stats(0u, 1u);

  EXPECT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/tmp/file", 0, modes), ResultCode::Success);
  // Still no clusters (because the file is empty), but 2 inodes now.
  check_stats(0u, 2u);

  {
    const Result<FileHandle> file = m_fs->OpenFile(Uid{0}, Gid{0}, "/tmp/file", Mode::Write);
    file->Write(std::vector<u8>(20).data(), 20);
  }
  // The file should now take up one cluster.
  // TODO: uncomment after the FS code is fixed.
  // check_stats(1u, 2u);
}

// Files need to be explicitly created using CreateFile or CreateDirectory.
// Automatically creating them on first use would be a bug.
TEST_F(FileSystemTest, NonExistingFiles)
{
  const Result<Metadata> metadata = m_fs->GetMetadata(Uid{0}, Gid{0}, "/tmp/foo");
  ASSERT_FALSE(metadata.Succeeded());
  EXPECT_EQ(metadata.Error(), ResultCode::NotFound);

  const Result<FileHandle> file = m_fs->OpenFile(Uid{0}, Gid{0}, "/tmp/foo", Mode::Read);
  ASSERT_FALSE(file.Succeeded());
  EXPECT_EQ(file.Error(), ResultCode::NotFound);

  const Result<std::vector<std::string>> children = m_fs->ReadDirectory(Uid{0}, Gid{0}, "/foo");
  ASSERT_FALSE(children.Succeeded());
  EXPECT_EQ(children.Error(), ResultCode::NotFound);
}

TEST_F(FileSystemTest, Seek)
{
  const std::vector<u8> TEST_DATA(10);

  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/tmp/f", 0, modes), ResultCode::Success);

  const Result<FileHandle> file = m_fs->OpenFile(Uid{0}, Gid{0}, "/tmp/f", Mode::ReadWrite);
  ASSERT_TRUE(file.Succeeded());

  // An empty file should have a size of exactly 0 bytes.
  EXPECT_EQ(file->GetStatus()->size, 0u);
  // The file position should be set to the start right after an open.
  EXPECT_EQ(file->GetStatus()->offset, 0u);

  // Write some dummy data.
  ASSERT_TRUE(file->Write(TEST_DATA.data(), TEST_DATA.size()).Succeeded());
  EXPECT_EQ(file->GetStatus()->size, TEST_DATA.size());
  EXPECT_EQ(file->GetStatus()->offset, TEST_DATA.size());

  auto seek_and_check = [&file](u32 offset, SeekMode mode, u32 expected_position) {
    const Result<u32> new_offset = file->Seek(offset, mode);
    ASSERT_TRUE(new_offset.Succeeded());
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
  ASSERT_FALSE(new_position.Succeeded());
  EXPECT_EQ(new_position.Error(), ResultCode::Invalid);
}

TEST_F(FileSystemTest, WriteAndSimpleReadback)
{
  const std::vector<u8> TEST_DATA{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}};
  std::vector<u8> read_buffer(TEST_DATA.size());

  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/tmp/f", 0, modes), ResultCode::Success);

  const Result<FileHandle> file = m_fs->OpenFile(Uid{0}, Gid{0}, "/tmp/f", Mode::ReadWrite);
  ASSERT_TRUE(file.Succeeded());

  // Write some test data.
  ASSERT_TRUE(file->Write(TEST_DATA.data(), TEST_DATA.size()).Succeeded());

  // Now read it back and make sure it is identical.
  ASSERT_TRUE(file->Seek(0, SeekMode::Set).Succeeded());
  ASSERT_TRUE(file->Read(read_buffer.data(), read_buffer.size()).Succeeded());
  EXPECT_EQ(TEST_DATA, read_buffer);
}

TEST_F(FileSystemTest, WriteAndRead)
{
  const std::vector<u8> TEST_DATA{{0xf, 1, 2, 3, 4, 5, 6, 7, 8, 9}};
  std::vector<u8> buffer(TEST_DATA.size());

  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/tmp/f", 0, modes), ResultCode::Success);

  Result<FileHandle> tmp_handle = m_fs->OpenFile(Uid{0}, Gid{0}, "/tmp/f", Mode::ReadWrite);
  ASSERT_TRUE(tmp_handle.Succeeded());
  const Fd fd = tmp_handle->Release();

  // Try to read from an empty file. This should do nothing.
  // See https://github.com/dolphin-emu/dolphin/pull/4942
  Result<u32> read_result = m_fs->ReadBytesFromFile(fd, buffer.data(), buffer.size());
  EXPECT_TRUE(read_result.Succeeded());
  EXPECT_EQ(*read_result, 0u);
  EXPECT_EQ(m_fs->GetFileStatus(fd)->offset, 0u);

  ASSERT_TRUE(m_fs->WriteBytesToFile(fd, TEST_DATA.data(), TEST_DATA.size()).Succeeded());
  EXPECT_EQ(m_fs->GetFileStatus(fd)->offset, TEST_DATA.size());

  // Try to read past EOF while we are at the end of the file. This should do nothing too.
  read_result = m_fs->ReadBytesFromFile(fd, buffer.data(), buffer.size());
  EXPECT_TRUE(read_result.Succeeded());
  EXPECT_EQ(*read_result, 0u);
  EXPECT_EQ(m_fs->GetFileStatus(fd)->offset, TEST_DATA.size());

  // Go back to the start and try to read past EOF. This should read the entire file until EOF.
  ASSERT_TRUE(m_fs->SeekFile(fd, 0, SeekMode::Set).Succeeded());
  std::vector<u8> larger_buffer(TEST_DATA.size() + 10);
  read_result = m_fs->ReadBytesFromFile(fd, larger_buffer.data(), larger_buffer.size());
  EXPECT_TRUE(read_result.Succeeded());
  EXPECT_EQ(*read_result, TEST_DATA.size());
  EXPECT_EQ(m_fs->GetFileStatus(fd)->offset, TEST_DATA.size());
}

TEST_F(FileSystemTest, MultipleHandles)
{
  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/tmp/f", 0, modes), ResultCode::Success);

  {
    const Result<FileHandle> file = m_fs->OpenFile(Uid{0}, Gid{0}, "/tmp/f", Mode::ReadWrite);
    ASSERT_TRUE(file.Succeeded());
    // Fill it with 10 zeroes.
    ASSERT_TRUE(file->Write(std::vector<u8>(10).data(), 10).Succeeded());
  }

  const Result<FileHandle> file1 = m_fs->OpenFile(Uid{0}, Gid{0}, "/tmp/f", Mode::ReadWrite);
  const Result<FileHandle> file2 = m_fs->OpenFile(Uid{0}, Gid{0}, "/tmp/f", Mode::ReadWrite);
  ASSERT_TRUE(file1.Succeeded());
  ASSERT_TRUE(file2.Succeeded());

  // Write some test data using one handle and make sure the data is seen by the other handle
  // (see issue 2917, 5232 and 8702 and https://github.com/dolphin-emu/dolphin/pull/2649).
  // Also make sure the file offsets are independent for each handle.

  const std::vector<u8> TEST_DATA{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}};
  EXPECT_EQ(file1->GetStatus()->offset, 0u);
  ASSERT_TRUE(file1->Write(TEST_DATA.data(), TEST_DATA.size()).Succeeded());
  EXPECT_EQ(file1->GetStatus()->offset, 10u);

  std::vector<u8> read_buffer(TEST_DATA.size());
  EXPECT_EQ(file2->GetStatus()->offset, 0u);
  ASSERT_TRUE(file2->Read(read_buffer.data(), read_buffer.size()).Succeeded());
  EXPECT_EQ(file2->GetStatus()->offset, 10u);
  EXPECT_EQ(TEST_DATA, read_buffer);
}

// ReadDirectory is used by official titles to determine whether a path is a file.
// If it is not a file, ResultCode::Invalid must be returned.
TEST_F(FileSystemTest, ReadDirectoryOnFile)
{
  ASSERT_EQ(m_fs->CreateFile(Uid{0}, Gid{0}, "/tmp/f", 0, modes), ResultCode::Success);

  const Result<std::vector<std::string>> result = m_fs->ReadDirectory(Uid{0}, Gid{0}, "/tmp/f");
  ASSERT_FALSE(result.Succeeded());
  EXPECT_EQ(result.Error(), ResultCode::Invalid);
}
