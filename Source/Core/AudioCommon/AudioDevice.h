#pragma once

#include <string>
#include <vector>
#include "Common/MsgHandler.h"
#include "Common/Logging/LogManager.h"

class AudioDevice {

private:
	static std::vector<AudioDevice> devices;
	static bool loaded;
	static void LoadDevices();

public:
	static AudioDevice DEFAULT;
	static std::vector<AudioDevice> GetDevices();
	static void ReloadDevices();
	static AudioDevice GetDeviceById(std::string id);
	static AudioDevice GetSelectedDevice();
	u32 index;
	std::string name;
	std::string path;
	std::string id;
	AudioDevice() {};
	AudioDevice(u32 _index, std::string _name, std::string _path, std::string _id) : index(_index), name(_name), path(_path), id(_id) {};
	inline bool operator==(const AudioDevice& other) { return id == other.id; }
	inline bool operator!=(const AudioDevice& other) { return !operator==(other); }

};


