
#if defined _WIN32
#include <Mmdeviceapi.h> // Must be included before devpkey:
#include <Functiondiscoverykeys_devpkey.h>
#elif defined HAVE_ALSA && HAVE_ALSA
#include <alsa/asoundlib.h>
#endif

#include "AudioDevice.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"

std::vector<AudioDevice> AudioDevice::devices;
// index 0 fits XAudio2_7 default, id "default" fits ALSA default
AudioDevice AudioDevice::DEFAULT = AudioDevice(0, "Default audio device", "", "default");
bool AudioDevice::loaded = false;

void AudioDevice::LoadDevices()
{
	devices.clear();

#if defined _WIN32
#define THROW_ON_ERROR(hr) if(FAILED(hr)) { throw hr; }

	HRESULT hr;

	IMMDeviceEnumerator* device_enumerator = NULL;
	IMMDeviceCollection* device_collection = NULL;
	u32 count = 0;

	// Some awful COM code to get the number of audio endpoints.
	try
	{
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&device_enumerator);
		THROW_ON_ERROR(hr);

		hr = device_enumerator->EnumAudioEndpoints(EDataFlow::eRender, DEVICE_STATE_ACTIVE, &device_collection);
		THROW_ON_ERROR(hr);

		hr = device_collection->GetCount(&count);
		THROW_ON_ERROR(hr);
	}
	catch (HRESULT hresult)
	{
		ERROR_LOG(AUDIO, "Error while loading audio device list: %#X", hresult);
	}

	// Retrieve details for each audio endpoint.
	for (u32 i = 0; i < count; i++)
	{
		AudioDevice device;
		device.index = i;

		LPWSTR id = NULL;
		IPropertyStore* propStore = NULL;
		IMMDevice* device_endpoint = NULL;

		PROPVARIANT devPath;
		PROPVARIANT devName;

		// some more awful COM code to retrieve the necessary device information.
		try
		{
			hr = device_collection->Item(i, &device_endpoint);
			THROW_ON_ERROR(hr);

			hr = device_endpoint->GetId(&id);
			THROW_ON_ERROR(hr);
			device.id = TStrToUTF8(id);
			
			hr = device_endpoint->OpenPropertyStore(STGM_READ, &propStore);
			THROW_ON_ERROR(hr);
			
			PropVariantInit(&devPath);
			// wtf? the necessary propkey for device path isn't defined?
			// see https://msdn.microsoft.com/en-us/library/windows/desktop/hh405048(v=vs.85).aspx comment 
			hr = propStore->GetValue(PROPERTYKEY{ { 0x9c119480, 0xddc2, 0x4954,{ 0xa1, 0x50, 0x5b, 0xd2, 0x40, 0xd4, 0x54, 0xad } }, 1 }, &devPath);
			THROW_ON_ERROR(hr);
			device.path = TStrToUTF8(devPath.pwszVal);
			
			PropVariantInit(&devName);
			hr = propStore->GetValue(PKEY_Device_FriendlyName, &devName);
			THROW_ON_ERROR(hr);
			device.name = TStrToUTF8(devName.pwszVal);
			
			devices.push_back(device);
		}
		catch (HRESULT hresult)
		{
			ERROR_LOG(AUDIO, "Error while loading audio device: %#X", hresult);
		}

		CoTaskMemFree(id);
		PropVariantClear(&devPath);
		PropVariantClear(&devName);
		if (propStore != NULL)
			propStore->Release();
		if (device_endpoint != NULL)
			device_endpoint->Release();
		
	}

	if (device_enumerator != NULL)
		device_enumerator->Release();
	if (device_collection != NULL)
		device_collection->Release();

#undef THROW_ON_ERROR
#elif defined HAVE_ALSA && HAVE_ALSA

	// get all sound devices from all cards for pcm
	char **hints;
	int err = snd_device_name_hint(-1, "pcm", (void***)&hints);
	if (err != 0)
	{
		ERROR_LOG(AUDIO, "Error while loading audio device list: %#X", err);
		return;
	}

	// iterate over all retrieved sound devices
	char** n = hints;
	snd_pcm_t* handle;
	while (*n != NULL)
	{

		char *name = snd_device_name_get_hint(*n, "NAME");
		char *ioid = snd_device_name_get_hint(*n, "IOID");
		char *desc = snd_device_name_get_hint(*n, "DESC");

		// Name valid && description valid && is output device (NULL means both)
		if (name != NULL && 0 != strcmp("null", name) && desc != NULL && 0 != strcmp("null", desc) && (ioid == NULL || 0 == strcmp("Output", ioid)))
		{
			AudioDevice device;
			device.name = std::string(desc);
			device.id = std::string(name);
			// test if the device is available and applicable for PCM output
			// TODO: Filter the mass of devices different. There are just too many options!
			// But what's the criteria? I don't know.
			if (snd_pcm_open(&handle, device.id.c_str(), SND_PCM_STREAM_PLAYBACK, 0) >= 0)
			{
				// That device works!
				snd_pcm_close(handle);
				// is this the default? If yes, overwrite the "default" default.
				if (device.id == "default")
				{
					device.name = "Default audio device (" + device.name + ")";
					AudioDevice::DEFAULT = device;
				}
				else
				{
					devices.push_back(device);
				}
			}
		}
		free(name);
		free(ioid);
		free(desc);
		n++;
	}

	snd_device_name_free_hint((void**)hints);
	
#endif
	// TODO: support other linux sound backends / PulseAudio?
	// TODO: support osx?

	// Show what we got
	for (AudioDevice& dev : devices)
	{
		INFO_LOG(AUDIO, "Found audio device: Name: <%s>, id: %s", dev.name.c_str(), dev.id.c_str());
	}

	loaded = true;
}

std::vector<AudioDevice>& AudioDevice::GetDevices()
{
	if (!loaded)
		LoadDevices();
	return devices;
}

void AudioDevice::ReloadDevices()
{
	LoadDevices();
}

AudioDevice& AudioDevice::GetDeviceById(const std::string& id)
{
	for (AudioDevice& device : GetDevices())
	{
		if (device.id == id)
		{
			return device;
		}
	}
	// TODO: maybe don't let invalid id's silently fall back to the default device (here)?
	return AudioDevice::DEFAULT;
}

AudioDevice& AudioDevice::GetSelectedDevice()
{
	return GetDeviceById(SConfig::GetInstance().sAudioDevice);
}

AudioDevice& AudioDevice::GetDefaultDevice()
{
	// Make sure devices are loaded, because the default's name might change.
	if (!loaded)
		LoadDevices();
	return AudioDevice::DEFAULT;
}
