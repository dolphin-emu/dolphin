# /dev/dolphin

This is an IOS device provided by Dolphin Emulator for the purpose of allowing homebrew and game mods to check if they're running on Dolphin, as well as detect how the emulator is running in order.

If the open is successful, it means that homebrew/mod must be running on emulator, and can skip certain things that won't work like patching IOS or enable emulator only features.

## IOCTL Listing

| Request Number | Name | Inputs | Outputs | Notes |
|---|---|---|---|---|
| 0x01 | GetElapsedTime | none | 4 bytes (u32) | Returns the elapsed time in milliseconds since emulation started. |
| 0x02 | GetVersion | none | null-terminated string | Returns Dolphin's current version, based on the SCM version string. |
| 0x03 | GetSpeedLimit | none | 4 bytes (u32) | Returns the maximum speed emulation can go, in percentage (i.e. 100%). |
| 0x04 | SetSpeedLimit | 4 bytes (u32) | none | Changes the emulation speed limit. The calculation is float(val) / float(100). |
| 0x05 | GetCpuSpeed | none | 4 bytes (u32) | Returns the emulated CPU's clock speed. |
| 0x06 | GetRealProductCode | none | null-terminated string | Returns the CODE field from the setting.txt file of the Wii's NAND. |
| 0x07 | SetDiscordClient | char* | none | Changes the Discord Rich Presence Client ID. |
| 0x08 | SetDiscordPresence | 10 vectors: [details (char*), state (char*), large_image_key (char*), large_image_text (char*), small_image_key (char*), small_image_text (char*), start_timestamp (u64), end_timestamp (u64), party_size (u32), party_max (u32)] | none | Updates the current Discord Rich Presence activity. See [Discord developer documentation](https://discord.com/developers/docs/rich-presence/using-with-the-game-sdk) for a full description of the API. |
| 0x09 | ResetDiscord | none | none | Resets the Discord Rich Presence back to Dolphin defaults. |
| 0x0A | GetSystemTime | none | 8 bytes (u64) | Returns the system time in milliseconds since UNIX epoch. |

This information was originally compiled on WiiBrew by AndrewPiroli and ProfElements before being ported to this document.

# References

- [Dolphin Source Code](/Source/Core/Core/IOS/DolphinDevice.cpp)
