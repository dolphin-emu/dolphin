// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "UpdaterCommon/UpdaterCommon.h"

#include <array>
#include <optional>

#include <OptionParser.h>
#include <ed25519/ed25519.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>
#include <zlib.h>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/HttpRequest.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"
#include "UpdaterCommon/UI.h"

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace
{
// Where to log updater output.
FILE* log_fp = stderr;

// Public key used to verify update manifests.
const std::array<u8, 32> UPDATE_PUB_KEY = {
    0x2a, 0xb3, 0xd1, 0xdc, 0x6e, 0xf5, 0x07, 0xf6, 0xa0, 0x6c, 0x7c, 0x54, 0xdf, 0x54, 0xf4, 0x42,
    0x80, 0xa6, 0x28, 0x8b, 0x6d, 0x70, 0x14, 0xb5, 0x4c, 0x34, 0x95, 0x20, 0x4d, 0xd4, 0xd3, 0x5d};

const char UPDATE_TEMP_DIR[] = "TempUpdate";

struct Manifest
{
  using Filename = std::string;
  using Hash = std::array<u8, 16>;
  std::map<Filename, Hash> entries;
};

// Represent the operations to be performed by the updater.
struct TodoList
{
  struct DownloadOp
  {
    Manifest::Filename filename;
    Manifest::Hash hash;
  };
  std::vector<DownloadOp> to_download;

  struct UpdateOp
  {
    Manifest::Filename filename;
    std::optional<Manifest::Hash> old_hash;
    Manifest::Hash new_hash;
  };
  std::vector<UpdateOp> to_update;

  struct DeleteOp
  {
    Manifest::Filename filename;
    Manifest::Hash old_hash;
  };
  std::vector<DeleteOp> to_delete;

  void Log() const;
};

bool ProgressCallback(double total, double now, double, double)
{
  UI::SetCurrentProgress(static_cast<int>(now), static_cast<int>(total));
  return true;
}

std::string HexEncode(const u8* buffer, size_t size)
{
  std::string out(size * 2, '\0');

  for (size_t i = 0; i < size; ++i)
  {
    out[2 * i] = "0123456789abcdef"[buffer[i] >> 4];
    out[2 * i + 1] = "0123456789abcdef"[buffer[i] & 0xF];
  }

  return out;
}

bool HexDecode(const std::string& hex, u8* buffer, size_t size)
{
  if (hex.size() != size * 2)
    return false;

  auto DecodeNibble = [](char c) -> std::optional<u8> {
    if (c >= '0' && c <= '9')
      return static_cast<u8>(c - '0');
    else if (c >= 'a' && c <= 'f')
      return static_cast<u8>(c - 'a' + 10);
    else if (c >= 'A' && c <= 'F')
      return static_cast<u8>(c - 'A' + 10);
    else
      return {};
  };
  for (size_t i = 0; i < size; ++i)
  {
    std::optional<u8> high = DecodeNibble(hex[2 * i]);
    std::optional<u8> low = DecodeNibble(hex[2 * i + 1]);

    if (!high || !low)
      return false;

    buffer[i] = (*high << 4) | *low;
  }

  return true;
}

std::optional<std::string> GzipInflate(const std::string& data)
{
  z_stream zstrm;
  zstrm.zalloc = nullptr;
  zstrm.zfree = nullptr;
  zstrm.opaque = nullptr;
  zstrm.avail_in = static_cast<u32>(data.size());
  zstrm.next_in = reinterpret_cast<u8*>(const_cast<char*>(data.data()));

  // 16 + MAX_WBITS means gzip. Don't ask me.
  inflateInit2(&zstrm, 16 + MAX_WBITS);

  std::string out;
  char buffer[4096];
  int ret;

  do
  {
    zstrm.avail_out = sizeof(buffer);
    zstrm.next_out = reinterpret_cast<u8*>(buffer);

    ret = inflate(&zstrm, 0);
    out.append(buffer, sizeof(buffer) - zstrm.avail_out);
  } while (ret == Z_OK);

  inflateEnd(&zstrm);

  if (ret != Z_STREAM_END)
  {
    fprintf(log_fp, "Could not read the data as gzip: error %d.\n", ret);
    return {};
  }

  return out;
}

Manifest::Hash ComputeHash(const std::string& contents)
{
  std::array<u8, 32> full;
  mbedtls_sha256_ret(reinterpret_cast<const u8*>(contents.data()), contents.size(), full.data(),
                     false);

  Manifest::Hash out;
  std::copy(full.begin(), full.begin() + 16, out.begin());
  return out;
}

bool VerifySignature(const std::string& data, const std::string& b64_signature)
{
  u8 signature[64];  // ed25519 sig size.
  size_t sig_size;

  if (mbedtls_base64_decode(signature, sizeof(signature), &sig_size,
                            reinterpret_cast<const u8*>(b64_signature.data()),
                            b64_signature.size()) ||
      sig_size != sizeof(signature))
  {
    fprintf(log_fp, "Invalid base64: %s\n", b64_signature.c_str());
    return false;
  }

  return ed25519_verify(signature, reinterpret_cast<const u8*>(data.data()), data.size(),
                        UPDATE_PUB_KEY.data());
}

void FlushLog()
{
  fflush(log_fp);
  fclose(log_fp);
}

void TodoList::Log() const
{
  if (to_update.size())
  {
    fprintf(log_fp, "Updating:\n");
    for (const auto& op : to_update)
    {
      std::string old_desc =
          op.old_hash ? HexEncode(op.old_hash->data(), op.old_hash->size()) : "(new)";
      fprintf(log_fp, "  - %s: %s -> %s\n", op.filename.c_str(), old_desc.c_str(),
              HexEncode(op.new_hash.data(), op.new_hash.size()).c_str());
    }
  }
  if (to_delete.size())
  {
    fprintf(log_fp, "Deleting:\n");
    for (const auto& op : to_delete)
    {
      fprintf(log_fp, "  - %s (%s)\n", op.filename.c_str(),
              HexEncode(op.old_hash.data(), op.old_hash.size()).c_str());
    }
  }
}

bool DownloadContent(const std::vector<TodoList::DownloadOp>& to_download,
                     const std::string& content_base_url, const std::string& temp_path)
{
  Common::HttpRequest req(std::chrono::seconds(30), ProgressCallback);

  UI::SetTotalMarquee(false);

  for (size_t i = 0; i < to_download.size(); i++)
  {
    UI::SetTotalProgress(static_cast<int>(i + 1), static_cast<int>(to_download.size()));

    auto& download = to_download[i];

    std::string hash_filename = HexEncode(download.hash.data(), download.hash.size());

    // File already exists, skipping
    if (File::Exists(temp_path + DIR_SEP + hash_filename))
      continue;

    UI::SetDescription("Downloading " + download.filename + "... (File " + std::to_string(i + 1) +
                       " of " + std::to_string(to_download.size()) + ")");
    UI::SetCurrentMarquee(false);

    // Add slashes where needed.
    std::string content_store_path = hash_filename;
    content_store_path.insert(4, "/");
    content_store_path.insert(2, "/");

    std::string url = content_base_url + content_store_path;
    fprintf(log_fp, "Downloading %s ...\n", url.c_str());

    auto resp = req.Get(url);
    if (!resp)
      return false;

    UI::SetCurrentMarquee(true);
    UI::SetDescription("Verifying " + download.filename + "...");

    std::string contents(reinterpret_cast<char*>(resp->data()), resp->size());
    std::optional<std::string> maybe_decompressed = GzipInflate(contents);
    if (!maybe_decompressed)
      return false;
    const std::string decompressed = std::move(*maybe_decompressed);

    // Check that the downloaded contents have the right hash.
    Manifest::Hash contents_hash = ComputeHash(decompressed);
    if (contents_hash != download.hash)
    {
      fprintf(log_fp, "Wrong hash on downloaded content %s.\n", url.c_str());
      return false;
    }

    const std::string out = temp_path + DIR_SEP + hash_filename;
    if (!File::WriteStringToFile(out, decompressed))
    {
      fprintf(log_fp, "Could not write cache file %s.\n", out.c_str());
      return false;
    }
  }
  return true;
}

TodoList ComputeActionsToDo(Manifest this_manifest, Manifest next_manifest)
{
  TodoList todo;

  // Delete if present in this manifest but not in next manifest.
  for (const auto& entry : this_manifest.entries)
  {
    if (next_manifest.entries.find(entry.first) == next_manifest.entries.end())
    {
      TodoList::DeleteOp del;
      del.filename = entry.first;
      del.old_hash = entry.second;
      todo.to_delete.push_back(std::move(del));
    }
  }

  // Download and update if present in next manifest with different hash from this manifest.
  for (const auto& entry : next_manifest.entries)
  {
    std::optional<Manifest::Hash> old_hash;

    const auto& old_entry = this_manifest.entries.find(entry.first);
    if (old_entry != this_manifest.entries.end())
      old_hash = old_entry->second;

    if (!old_hash || *old_hash != entry.second)
    {
      TodoList::DownloadOp download;
      download.filename = entry.first;
      download.hash = entry.second;

      todo.to_download.push_back(std::move(download));

      TodoList::UpdateOp update;
      update.filename = entry.first;
      update.old_hash = old_hash;
      update.new_hash = entry.second;
      todo.to_update.push_back(std::move(update));
    }
  }

  return todo;
}

std::optional<std::string> FindOrCreateTempDir(const std::string& base_path)
{
  std::string temp_path = base_path + DIR_SEP + UPDATE_TEMP_DIR;
  int counter = 0;

  File::DeleteDirRecursively(temp_path);

  do
  {
    if (File::CreateDir(temp_path))
    {
      return temp_path;
    }
    else
    {
      fprintf(log_fp, "Couldn't create temp directory.\n");

      // Try again with a counter appended to the path.
      std::string suffix = UPDATE_TEMP_DIR + std::to_string(counter);
      temp_path = base_path + DIR_SEP + suffix;
    }
  } while (counter++ < 10);

  fprintf(log_fp, "Could not find an appropriate temp directory name. Giving up.\n");
  return {};
}

void CleanUpTempDir(const std::string& temp_dir, const TodoList& todo)
{
  // This is best-effort cleanup, we ignore most errors.
  for (const auto& download : todo.to_download)
    File::Delete(temp_dir + DIR_SEP + HexEncode(download.hash.data(), download.hash.size()));
  File::DeleteDir(temp_dir);
}

bool BackupFile(const std::string& path)
{
  std::string backup_path = path + ".bak";
  fprintf(log_fp, "Backing up unknown pre-existing %s to .bak.\n", path.c_str());
  if (!File::Rename(path, backup_path))
  {
    fprintf(log_fp, "Cound not rename %s to %s for backup.\n", path.c_str(), backup_path.c_str());
    return false;
  }
  return true;
}

bool DeleteObsoleteFiles(const std::vector<TodoList::DeleteOp>& to_delete,
                         const std::string& install_base_path)
{
  for (const auto& op : to_delete)
  {
    std::string path = install_base_path + DIR_SEP + op.filename;

    if (!File::Exists(path))
    {
      fprintf(log_fp, "File %s is already missing.\n", op.filename.c_str());
      continue;
    }
    else
    {
      std::string contents;
      if (!File::ReadFileToString(path, contents))
      {
        fprintf(log_fp, "Could not read file planned for deletion: %s.\n", op.filename.c_str());
        return false;
      }
      Manifest::Hash contents_hash = ComputeHash(contents);
      if (contents_hash != op.old_hash)
      {
        if (!BackupFile(path))
          return false;
      }

      File::Delete(path);
    }
  }
  return true;
}

bool UpdateFiles(const std::vector<TodoList::UpdateOp>& to_update,
                 const std::string& install_base_path, const std::string& temp_path)
{
  for (const auto& op : to_update)
  {
    std::string path = install_base_path + DIR_SEP + op.filename;
    if (!File::CreateFullPath(path))
    {
      fprintf(log_fp, "Could not create directory structure for %s.\n", op.filename.c_str());
      return false;
    }

#ifndef _WIN32
    // TODO: A new updater protocol version is required to properly mark executable files. For
    // now, copy executable bits from existing files. This will break for newly added executables.
    std::optional<mode_t> permission;
#endif

    if (File::Exists(path))
    {
#ifndef _WIN32
      struct stat file_stats;

      if (lstat(path.c_str(), &file_stats) != 0)
        continue;

      if (S_ISLNK(file_stats.st_mode))
      {
        fprintf(log_fp, "%s is symlink, skipping\n", path.c_str());
        continue;
      }

      permission = file_stats.st_mode;
#endif
      std::string contents;
      if (!File::ReadFileToString(path, contents))
      {
        fprintf(log_fp, "Could not read existing file %s.\n", op.filename.c_str());
        return false;
      }
      Manifest::Hash contents_hash = ComputeHash(contents);
      if (contents_hash == op.new_hash)
      {
        fprintf(log_fp, "File %s was already up to date. Partial update?\n", op.filename.c_str());
        continue;
      }
      else if (!op.old_hash || contents_hash != *op.old_hash)
      {
        if (!BackupFile(path))
          return false;
      }
    }

    // Now we can safely move the new contents to the location.
    std::string content_filename = HexEncode(op.new_hash.data(), op.new_hash.size());
    fprintf(log_fp, "Updating file %s from content %s...\n", op.filename.c_str(),
            content_filename.c_str());
    if (!File::Copy(temp_path + DIR_SEP + content_filename, path))
    {
      fprintf(log_fp, "Could not update file %s.\n", op.filename.c_str());
      return false;
    }

#ifndef _WIN32
    if (permission.has_value() && chmod(path.c_str(), permission.value()) != 0)
      return false;
#endif
  }
  return true;
}

bool PerformUpdate(const TodoList& todo, const std::string& install_base_path,
                   const std::string& content_base_url, const std::string& temp_path)
{
  fprintf(log_fp, "Starting download step...\n");
  if (!DownloadContent(todo.to_download, content_base_url, temp_path))
    return false;
  fprintf(log_fp, "Download step completed.\n");

  fprintf(log_fp, "Starting update step...\n");
  if (!UpdateFiles(todo.to_update, install_base_path, temp_path))
    return false;
  fprintf(log_fp, "Update step completed.\n");

  fprintf(log_fp, "Starting deletion step...\n");
  if (!DeleteObsoleteFiles(todo.to_delete, install_base_path))
    return false;
  fprintf(log_fp, "Deletion step completed.\n");

  return true;
}

void FatalError(const std::string& message)
{
  fprintf(log_fp, "%s\n", message.c_str());

  UI::SetVisible(true);
  UI::Error(message);
}

std::optional<Manifest> ParseManifest(const std::string& manifest)
{
  Manifest parsed;
  size_t pos = 0;

  while (pos < manifest.size())
  {
    size_t filename_end_pos = manifest.find('\t', pos);
    if (filename_end_pos == std::string::npos)
    {
      fprintf(log_fp, "Manifest entry %zu: could not find filename end.\n", parsed.entries.size());
      return {};
    }
    size_t hash_end_pos = manifest.find('\n', filename_end_pos);
    if (hash_end_pos == std::string::npos)
    {
      fprintf(log_fp, "Manifest entry %zu: could not find hash end.\n", parsed.entries.size());
      return {};
    }

    std::string filename = manifest.substr(pos, filename_end_pos - pos);
    std::string hash = manifest.substr(filename_end_pos + 1, hash_end_pos - filename_end_pos - 1);
    if (hash.size() != 32)
    {
      fprintf(log_fp, "Manifest entry %zu: invalid hash: \"%s\".\n", parsed.entries.size(),
              hash.c_str());
      return {};
    }

    Manifest::Hash decoded_hash;
    if (!HexDecode(hash, decoded_hash.data(), decoded_hash.size()))
    {
      fprintf(log_fp, "Manifest entry %zu: invalid hash: \"%s\".\n", parsed.entries.size(),
              hash.c_str());
      return {};
    }

    parsed.entries[filename] = decoded_hash;
    pos = hash_end_pos + 1;
  }

  return parsed;
}

// Not showing a progress bar here because this part is just too quick
std::optional<Manifest> FetchAndParseManifest(const std::string& url)
{
  Common::HttpRequest http;

  Common::HttpRequest::Response resp = http.Get(url);
  if (!resp)
  {
    fprintf(log_fp, "Manifest download failed.\n");
    return {};
  }

  std::string contents(reinterpret_cast<char*>(resp->data()), resp->size());
  std::optional<std::string> maybe_decompressed = GzipInflate(contents);
  if (!maybe_decompressed)
    return {};
  std::string decompressed = std::move(*maybe_decompressed);

  // Split into manifest and signature.
  size_t boundary = decompressed.rfind("\n\n");
  if (boundary == std::string::npos)
  {
    fprintf(log_fp, "No signature was found in manifest.\n");
    return {};
  }

  std::string signature_block = decompressed.substr(boundary + 2);  // 2 for "\n\n".
  decompressed.resize(boundary + 1);                                // 1 to keep the final "\n".

  std::vector<std::string> signatures = SplitString(signature_block, '\n');
  bool found_valid_signature = false;
  for (const auto& signature : signatures)
  {
    if (VerifySignature(decompressed, signature))
    {
      found_valid_signature = true;
      break;
    }
  }
  if (!found_valid_signature)
  {
    fprintf(log_fp, "Could not verify signature of the manifest.\n");
    return {};
  }

  return ParseManifest(decompressed);
}

struct Options
{
  std::string this_manifest_url;
  std::string next_manifest_url;
  std::string content_store_url;
  std::string install_base_path;
  std::optional<std::string> binary_to_restart;
  std::optional<u32> parent_pid;
  std::optional<std::string> log_file;
};

std::optional<Options> ParseCommandLine(std::vector<std::string>& args)
{
  using optparse::OptionParser;

  OptionParser parser =
      OptionParser().prog("Dolphin Updater").description("Dolphin Updater binary");

  parser.add_option("--this-manifest-url")
      .dest("this-manifest-url")
      .help("URL to the update manifest for the currently installed version.")
      .metavar("URL");
  parser.add_option("--next-manifest-url")
      .dest("next-manifest-url")
      .help("URL to the update manifest for the to-be-installed version.")
      .metavar("URL");
  parser.add_option("--content-store-url")
      .dest("content-store-url")
      .help("Base URL of the content store where files to download are stored.")
      .metavar("URL");
  parser.add_option("--install-base-path")
      .dest("install-base-path")
      .help("Base path of the Dolphin install to be updated.")
      .metavar("PATH");
  parser.add_option("--binary-to-restart")
      .dest("binary-to-restart")
      .help("Binary to restart after the update is over.")
      .metavar("PATH");
  parser.add_option("--log-file")
      .dest("log-file")
      .help("File where to log updater debug output.")
      .metavar("PATH");
  parser.add_option("--parent-pid")
      .dest("parent-pid")
      .type("int")
      .help("(optional) PID of the parent process. The updater will wait for this process to "
            "complete before proceeding.")
      .metavar("PID");

  optparse::Values options = parser.parse_args(args);

  Options opts;

  // Required arguments.
  std::vector<std::string> required{"this-manifest-url", "next-manifest-url", "content-store-url",
                                    "install-base-path"};
  for (const auto& req : required)
  {
    if (!options.is_set(req))
    {
      parser.print_help();
      return {};
    }
  }
  opts.this_manifest_url = options["this-manifest-url"];
  opts.next_manifest_url = options["next-manifest-url"];
  opts.content_store_url = options["content-store-url"];
  opts.install_base_path = options["install-base-path"];

  // Optional arguments.
  if (options.is_set("binary-to-restart"))
    opts.binary_to_restart = options["binary-to-restart"];
  if (options.is_set("parent-pid"))
    opts.parent_pid = static_cast<u32>(options.get("parent-pid"));
  if (options.is_set("log-file"))
    opts.log_file = options["log-file"];

  return opts;
}
};  // namespace

bool RunUpdater(std::vector<std::string> args)
{
  std::optional<Options> maybe_opts = ParseCommandLine(args);

  if (!maybe_opts)
  {
    return false;
  }

  UI::Init();
  UI::SetVisible(false);

  Common::ScopeGuard ui_guard{[] { UI::Stop(); }};
  Options opts = std::move(*maybe_opts);

  if (opts.log_file)
  {
    log_fp = fopen(opts.log_file.value().c_str(), "w");
    if (!log_fp)
      log_fp = stderr;
    else
      atexit(FlushLog);
  }

  fprintf(log_fp, "Updating from: %s\n", opts.this_manifest_url.c_str());
  fprintf(log_fp, "Updating to:   %s\n", opts.next_manifest_url.c_str());
  fprintf(log_fp, "Install path:  %s\n", opts.install_base_path.c_str());

  if (!File::IsDirectory(opts.install_base_path))
  {
    FatalError("Cannot find install base path, or not a directory.");
    return false;
  }

  if (opts.parent_pid)
  {
    fprintf(log_fp, "Waiting for parent PID %d to complete...\n", *opts.parent_pid);

    auto pid = opts.parent_pid.value();

    UI::WaitForPID(static_cast<u32>(pid));

    fprintf(log_fp, "Completed! Proceeding with update.\n");
  }

  UI::SetVisible(true);

  UI::SetDescription("Fetching and parsing manifests...");

  Manifest this_manifest, next_manifest;
  {
    std::optional<Manifest> maybe_manifest = FetchAndParseManifest(opts.this_manifest_url);
    if (!maybe_manifest)
    {
      FatalError("Could not fetch current manifest. Aborting.");
      return false;
    }
    this_manifest = std::move(*maybe_manifest);

    maybe_manifest = FetchAndParseManifest(opts.next_manifest_url);
    if (!maybe_manifest)
    {
      FatalError("Could not fetch next manifest. Aborting.");
      return false;
    }
    next_manifest = std::move(*maybe_manifest);
  }

  UI::SetDescription("Computing what to do...");

  TodoList todo = ComputeActionsToDo(this_manifest, next_manifest);
  todo.Log();

  std::optional<std::string> maybe_temp_dir = FindOrCreateTempDir(opts.install_base_path);
  if (!maybe_temp_dir)
    return false;
  std::string temp_dir = std::move(*maybe_temp_dir);

  UI::SetDescription("Performing Update...");

  bool ok = PerformUpdate(todo, opts.install_base_path, opts.content_store_url, temp_dir);
  if (!ok)
  {
    FatalError("Failed to apply the update.");
    CleanUpTempDir(temp_dir, todo);
    return false;
  }

  UI::ResetCurrentProgress();
  UI::ResetTotalProgress();
  UI::SetCurrentMarquee(false);
  UI::SetTotalMarquee(false);
  UI::SetCurrentProgress(1, 1);
  UI::SetTotalProgress(1, 1);
  UI::SetDescription("Done!");

  // Let the user process that we are done.
  UI::Sleep(1);

  if (opts.binary_to_restart)
  {
    UI::LaunchApplication(opts.binary_to_restart.value());
  }

  return true;
}
