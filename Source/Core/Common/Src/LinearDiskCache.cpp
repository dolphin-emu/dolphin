// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "LinearDiskCache.h"

static const char ID[4] = {'D', 'C', 'A', 'C'};
const int version = 4888;  // TODO: Get from SVN_REV

LinearDiskCache::LinearDiskCache() 
	: file_(NULL), num_entries_(0) {
}

void LinearDiskCache::WriteHeader() {
	fwrite(ID, 4, 1, file_);
	fwrite(&version, 4, 1, file_);
}

bool LinearDiskCache::ValidateHeader() {
	char header_id[4];
	int header_version;
	fread(&header_id, 4, 1, file_);
	fread(&header_version, 4, 1, file_);
	if (memcmp(header_id, ID, 4) != 0)
		return false;
	if (header_version != version)
		return false;
	return true;
}

int LinearDiskCache::OpenAndRead(const char *filename, LinearDiskCacheReader *reader) {
	int items_read_count = 0;
	file_ = fopen(filename, "rb");
	int file_size = 0;
	if (file_) {
		fseek(file_, 0, SEEK_END);
		file_size = (int)ftell(file_);
	}

	bool file_corrupt = false;
	if (file_size == 0) {
		if (file_)
			fclose(file_);
		// Reopen for writing.
		file_ = fopen(filename, "wb");
		// Cache empty, let's initialize a header.
		WriteHeader();
		num_entries_ = 0;
	} else {
		// file_ must be != 0 here.
		// Back to the start we go.
		fseek(file_, 0, SEEK_SET);
		// Check that the file is valid
		if (!ValidateHeader()) {
			// Not valid - delete the file and start over.
			fclose(file_);
			unlink(filename);

			PanicAlert("LinearDiskCache file header broken.");

			file_ = fopen(filename, "wb");
			WriteHeader();
			num_entries_ = 0;
		} else {
			// Valid - blow through it.
			// We're past the header already thanks to ValidateHeader.
			while (!feof(file_)) {
				int key_size, value_size;
				int key_size_size = fread(&key_size, 1, sizeof(key_size), file_);
				int value_size_size = fread(&value_size, 1, sizeof(value_size), file_);
				if (key_size_size == 0 && value_size_size == 0) {
					// I guess feof isn't doing it's job - we're at the end.
					break;
				}
				if (key_size <= 0 || value_size < 0 || key_size_size != 4 || value_size_size != 4) {
					PanicAlert("Disk cache file %s corrupted/truncated! ks: %i vs %i kss %i vss %i", filename,
						key_size, value_size, key_size_size, value_size_size);
					file_corrupt = true;
					break;
				}
				u8 *key = new u8[key_size];
				u8 *value = new u8[value_size];
				int actual_key_size = (int)fread(key, 1, key_size, file_);
				int actual_value_size = (int)fread(value, 1, value_size, file_);
				if (actual_key_size != key_size || actual_value_size != value_size) {
					PanicAlert("Disk cache file %s corrupted/truncated! ks: %i  actual ks: %i   vs: %i  actual vs: %i", filename,
						key_size, actual_key_size, value_size, actual_value_size);
					file_corrupt = true;
				} else {
					reader->Read(key, key_size, value, value_size);
					items_read_count++;
				}
				delete [] key;
				delete [] value;
			}
			fclose(file_);
			// Done reading.

			// Reopen file for append.
			// At this point, ftell() will be at the end of the file,
			// which happens to be exactly what we want.
			file_ = fopen(filename, "ab");
			fseek(file_, 0, SEEK_END);
		}
	}

	if (file_corrupt) {
		// Restore sanity, start over.
		fclose(file_);
		unlink(filename);

		file_ = fopen(filename, "wb+");
		WriteHeader();
	}

	return items_read_count;
}

void LinearDiskCache::Append(
	const u8 *key, int key_size, const u8 *value, int value_size) {
	// Should do a check that we don't already have "key"?
	fwrite(&key_size, 1, sizeof(key_size), file_);
	fwrite(&value_size, 1, sizeof(value_size), file_);
	fwrite(key, 1, key_size, file_);
	fwrite(value, 1, value_size, file_);
}

void LinearDiskCache::Sync() {
	fflush(file_);
}

void LinearDiskCache::Close() {
	fclose(file_);
	file_ = 0;
	num_entries_ = 0;
}
