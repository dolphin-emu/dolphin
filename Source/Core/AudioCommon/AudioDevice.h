#pragma once

#include <string>
#include <vector>

struct AudioDevice
{

public:
	AudioDevice() {};
	AudioDevice(u32 _index, std::string _name, std::string _path, std::string _id) : index(_index), name(_name), path(_path), id(_id) {};

	inline bool operator==(const AudioDevice& other) { return id == other.id; }
	inline bool operator!=(const AudioDevice& other) { return !operator==(other); }

	static std::vector<AudioDevice>& GetDevices();
	static void ReloadDevices();
	static AudioDevice& GetDeviceById(const std::string& id);
	static AudioDevice& GetSelectedDevice();
	static AudioDevice& GetDefaultDevice();
	
	u32 index;
	std::string name;
	std::string path;
	std::string id;

private:
	static void LoadDevices();

	static std::vector<AudioDevice> devices;
	static bool loaded;
	static AudioDevice DEFAULT;

};
