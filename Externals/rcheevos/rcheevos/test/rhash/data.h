#ifndef RHASH_TEST_DATA_H
#define RHASH_TEST_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

uint8_t* generate_generic_file(size_t size);

uint8_t* convert_to_2352(uint8_t* input, size_t* input_size, uint32_t first_sector);

uint8_t* generate_3do_bin(unsigned root_directory_sectors, unsigned binary_size, size_t* image_size);
uint8_t* generate_dreamcast_bin(unsigned track_first_sector, unsigned binary_size, size_t* image_size);
uint8_t* generate_pce_cd_bin(unsigned binary_sectors, size_t* image_size);
uint8_t* generate_pcfx_bin(unsigned binary_sectors, size_t* image_size);
uint8_t* generate_psx_bin(const char* binary_name, unsigned binary_size, size_t* image_size);
uint8_t* generate_ps2_bin(const char* binary_name, unsigned binary_size, size_t* image_size);

uint8_t* generate_atari_7800_file(size_t kb, int with_header, size_t* image_size);
uint8_t* generate_nes_file(size_t kb, int with_header, size_t* image_size);
uint8_t* generate_fds_file(size_t sides, int with_header, size_t* image_size);
uint8_t* generate_nds_file(size_t mb, unsigned arm9_size, unsigned arm7_size, size_t* image_size);

uint8_t* generate_iso9660_bin(unsigned binary_sectors, const char* volume_label, size_t* image_size);
uint8_t* generate_iso9660_file(uint8_t* image, const char* filename, const uint8_t* contents, size_t contents_size);

extern uint8_t test_rom_z64[64];
extern uint8_t test_rom_n64[64];
extern uint8_t test_rom_v64[64];

#ifdef __cplusplus
}
#endif

#endif /* RHASH_TEST_DATA_H */
