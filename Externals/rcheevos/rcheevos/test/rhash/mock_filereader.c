#include "rc_hash.h"

#include <stdlib.h>
#include <string.h>

typedef struct mock_file_data
{
  const char* path;
  const uint8_t* data;
  int64_t size;
  int64_t pos;
  int first_sector;
} mock_file_data;

static mock_file_data mock_file_instance[16];
static int mock_cd_tracks;

static void* _mock_file_open(const char* path)
{
  int i;
  for (i = 0; i < sizeof(mock_file_instance) / sizeof(mock_file_instance[0]); ++i)
  {
    if (strcmp(path, mock_file_instance[i].path) == 0)
    {
      mock_file_instance[i].pos = 0;
      return &mock_file_instance[i];
    }
  }

  return NULL;
}

static void _mock_file_seek(void* file_handle, int64_t offset, int origin)
{
  mock_file_data* file = (mock_file_data*)file_handle;
  switch (origin)
  {
  case SEEK_SET:
    file->pos = offset;
    break;
  case SEEK_CUR:
    file->pos += offset;
    break;
  case SEEK_END:
    file->pos = file->size - offset;
    break;
  }

  if (file->pos > file->size)
    file->pos = file->size;
}

static int64_t _mock_file_tell(void* file_handle)
{
  mock_file_data* file = (mock_file_data*)file_handle;
  return file->pos;
}

static size_t _mock_file_read(void* file_handle, void* buffer, size_t count)
{
  mock_file_data* file = (mock_file_data*)file_handle;
  const size_t remaining = (size_t)(file->size - file->pos);
  if (count > remaining)
    count = remaining;

  if (count > 0)
  {
    if (file->data)
      memcpy(buffer, &file->data[file->pos], count);
    else
      memset(buffer, 0, count);

    file->pos += count;
  }

  return count;
}

static void _mock_file_close(void* file_handle)
{
    (void)file_handle;
}

static void reset_mock_files()
{
  int i;

  memset(&mock_file_instance, 0, sizeof(mock_file_instance));
  for (i = 0; i < sizeof(mock_file_instance) / sizeof(mock_file_instance[0]); ++i)
    mock_file_instance[i].path = "";

  mock_cd_tracks = 0;
}

void init_mock_filereader()
{
  struct rc_hash_filereader reader;
  reader.open = _mock_file_open;
  reader.seek = _mock_file_seek;
  reader.tell = _mock_file_tell;
  reader.read = _mock_file_read;
  reader.close = _mock_file_close;

  rc_hash_init_custom_filereader(&reader);

  reset_mock_files();
}

void mock_file(int index, const char* filename, const uint8_t* buffer, size_t buffer_size)
{
  if (index == 0)
    reset_mock_files();

  mock_file_instance[index].path = filename;
  mock_file_instance[index].data = buffer;
  mock_file_instance[index].size = buffer_size;
  mock_file_instance[index].pos = 0;
  mock_file_instance[index].first_sector = 0;
}

void mock_file_text(int index, const char* filename, const char* contents)
{
    mock_file(index, filename, (const uint8_t*)contents, strlen(contents));
}

void mock_file_first_sector(int index, int first_sector)
{
  mock_file_instance[index].first_sector = first_sector;
}

void mock_file_size(int index, size_t mock_size)
{
  mock_file_instance[index].size = mock_size;
}

void mock_empty_file(int index, const char* filename, size_t mock_size)
{
  mock_file(index, filename, NULL, mock_size);
}

static void* _mock_cd_open_track(const char* path, uint32_t track)
{
  if (track == RC_HASH_CDTRACK_LAST)
    track = mock_cd_tracks;

  if (track == 1 || track == RC_HASH_CDTRACK_FIRST_DATA || track == RC_HASH_CDTRACK_LARGEST)
  {
    if (strstr(path, ".cue")) 
    {
      mock_file_data* file = (mock_file_data*)_mock_file_open(path);
      if (!file)
        return file;

      return _mock_file_open((const char*)file->data);
    }

    return _mock_file_open(path);
  }
  else if (strstr(path, ".cue"))
  {
    mock_file_data* file = (mock_file_data*)_mock_file_open(path);
    if (file)
    {
      char buffer[256];

      const size_t file_len = strlen((const char*)file->data);
      memcpy(buffer, file->data, file_len - 4);
      sprintf(&buffer[file_len - 4], "%d%s", track, &file->data[file_len - 4]);

      return _mock_file_open(buffer);
    }
  }
  else if (strstr(path, ".gdi"))
  {
    mock_file_data* file = (mock_file_data*)_mock_file_open(path);
    if (file)
    {
      char buffer[32];
      sprintf(buffer, "track%02d.bin", track);
      return _mock_file_open(buffer);
    }
  }

  return NULL;
}

static size_t _mock_cd_read_sector(void* track_handle, uint32_t sector, void* buffer, size_t requested_bytes)
{
  mock_file_data* file = (mock_file_data*)track_handle;
  sector -= file->first_sector;

  _mock_file_seek(track_handle, sector * 2048, SEEK_SET);
  return _mock_file_read(track_handle, buffer, requested_bytes);
}

static uint32_t _mock_cd_first_track_sector(void* track_handle)
{
  mock_file_data* file = (mock_file_data*)track_handle;
  return file->first_sector;
}

void mock_cd_num_tracks(int num_tracks)
{
  mock_cd_tracks = num_tracks;
}

void init_mock_cdreader()
{
  struct rc_hash_cdreader cdreader;
  memset(&cdreader, 0, sizeof(cdreader));
  cdreader.open_track = _mock_cd_open_track;
  cdreader.close_track = _mock_file_close;
  cdreader.read_sector = _mock_cd_read_sector;
  cdreader.first_track_sector = _mock_cd_first_track_sector;

  rc_hash_init_custom_cdreader(&cdreader);

  mock_cd_tracks = 0;
}

const char* get_mock_filename(void* file_handle)
{
  mock_file_data* file = (mock_file_data*)file_handle;
  return file->path;
}
