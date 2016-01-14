// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <map>

#include "Common/Assert.h"
#include "Common/OnionConfig.h"
#include "Common/StringUtil.h"

namespace OnionConfig
{
	const std::string& OnionPetal::NULL_STRING = "";
	OnionBloom m_layers;
	std::list<std::pair<CallbackFunction, void*>> m_callbacks;

	void CallbackSystems();

	// Only to be used with the meta-layer
	class RecursiveOnionPetal final : public OnionPetal
	{
	public:
		RecursiveOnionPetal(OnionLayerType layer, OnionSystem system, const std::string& name)
			: OnionPetal(layer, system, name) {}

		bool Exists(const std::string& key) const override;

		bool Get(const std::string& key, std::string* value, const std::string& default_value = NULL_STRING) const override;

		void Set(const std::string& key, const std::string& value) override;
	};

	bool RecursiveOnionPetal::Exists(const std::string& key) const
	{
		auto layers_it = m_layers.find(OnionLayerType::LAYER_META);
		do
		{
			OnionPetal* layer_petal = layers_it->second->GetPetal(m_system, m_name);
			if (layer_petal && layer_petal->Exists(key))
				return true;
		} while (--layers_it != m_layers.end());

		return false;
	}

	bool RecursiveOnionPetal::Get(const std::string& key, std::string* value, const std::string& default_value) const
	{
		std::array <OnionLayerType, 7> search_order =
		{
			// Skip the meta layer
			OnionLayerType::LAYER_CURRENTRUN,
			OnionLayerType::LAYER_COMMANDLINE,
			OnionLayerType::LAYER_MOVIE,
			OnionLayerType::LAYER_NETPLAY,
			OnionLayerType::LAYER_LOCALGAME,
			OnionLayerType::LAYER_GLOBALGAME,
			OnionLayerType::LAYER_BASE,
		};

		for (auto layer_id : search_order)
		{
			auto layers_it = m_layers.find(layer_id);
			if (layers_it == m_layers.end())
				continue;

			OnionPetal* layer_petal = layers_it->second->GetPetal(m_system, m_name);
			if (layer_petal && layer_petal->Exists(key))
				return layer_petal->Get(key, value, default_value);
		}

		return OnionPetal::Get(key, value, default_value);
	}

	void RecursiveOnionPetal::Set(const std::string& key, const std::string& value)
	{
		_assert_msg_(COMMON, false, "Don't try to set values here!");
	}

	class RecursiveBloomLayer final : public BloomLayer
	{
	public:
		RecursiveBloomLayer() : BloomLayer(OnionLayerType::LAYER_META) {}
		OnionPetal* GetPetal(OnionSystem system, const std::string& petal_name) override;
		OnionPetal* GetOrCreatePetal(OnionSystem system, const std::string& petal_name) override;
	};

	OnionPetal* RecursiveBloomLayer::GetPetal(OnionSystem system, const std::string& petal_name)
	{
		// Always queries backwards recursively, so it doesn't matter if it exists or not on this layer
		return GetOrCreatePetal(system, petal_name);
	}

	OnionPetal* RecursiveBloomLayer::GetOrCreatePetal(OnionSystem system, const std::string& petal_name)
	{
		OnionPetal* petal = BloomLayer::GetPetal(system, petal_name);
		if (!petal)
		{
			m_petals[system].emplace_back(new RecursiveOnionPetal(m_layer, system, petal_name));
			petal = m_petals[system].back();
		}
		return petal;
	}

	bool OnionPetal::Exists(const std::string& key) const
	{
		return m_values.find(key) != m_values.end();
	}

	bool OnionPetal::Delete(const std::string& key)
	{
		auto it = m_values.find(key);
		if (it == m_values.end())
			return false;

		m_values.erase(it);
		return true;
	}

	void OnionPetal::Set(const std::string& key, const std::string& value)
	{
		auto it = m_values.find(key);
		if (it != m_values.end())
			it->second = value;
		else
			m_values[key] = value;
	}

	void OnionPetal::Set(const std::string& key, u32 newValue)
	{
		OnionPetal::Set(key, StringFromFormat("0x%08x", newValue));
	}

	void OnionPetal::Set(const std::string& key, float newValue)
	{
		OnionPetal::Set(key, StringFromFormat("%#.9g", newValue));
	}

	void OnionPetal::Set(const std::string& key, double newValue)
	{
		OnionPetal::Set(key, StringFromFormat("%#.17g", newValue));
	}

	void OnionPetal::Set(const std::string& key, int newValue)
	{
		OnionPetal::Set(key, StringFromInt(newValue));
	}

	void OnionPetal::Set(const std::string& key, bool newValue)
	{
		OnionPetal::Set(key, StringFromBool(newValue));
	}

	void OnionPetal::Set(const std::string& key, const std::string& newValue, const std::string& defaultValue)
	{
		if (newValue != defaultValue)
			Set(key, newValue);
		else
			Delete(key);
	}

	bool OnionPetal::Get(const std::string& key, std::string* value, const std::string& default_value) const
	{
		const auto& it = m_values.find(key);
		if (it != m_values.end())
		{
			*value = it->second;
			return true;
		}
		else if (&default_value != &NULL_STRING)
		{
			*value = default_value;
			return true;
		}

		return false;
	}

	bool OnionPetal::Get(const std::string& key, int* value, int defaultValue) const
	{
		std::string temp;
		bool retval = Get(key, &temp);

		if (retval && TryParse(temp, value))
			return true;

		*value = defaultValue;
		return false;
	}

	bool OnionPetal::Get(const std::string& key, u32* value, u32 defaultValue) const
	{
		std::string temp;
		bool retval = Get(key, &temp);

		if (retval && TryParse(temp, value))
			return true;

		*value = defaultValue;
		return false;
	}

	bool OnionPetal::Get(const std::string& key, bool* value, bool defaultValue) const
	{
		std::string temp;
		bool retval = Get(key, &temp);

		if (retval && TryParse(temp, value))
			return true;

		*value = defaultValue;
		return false;
	}

	bool OnionPetal::Get(const std::string& key, float* value, float defaultValue) const
	{
		std::string temp;
		bool retval = Get(key, &temp);

		if (retval && TryParse(temp, value))
			return true;

		*value = defaultValue;
		return false;
	}

	bool OnionPetal::Get(const std::string& key, double* value, double defaultValue) const
	{
		std::string temp;
		bool retval = Get(key, &temp);

		if (retval && TryParse(temp, value))
			return true;

		*value = defaultValue;
		return false;
	}

	// Return a list of all lines in a petal
	bool OnionPetal::GetLines(std::vector<std::string>* lines, const bool remove_comments) const
	{
		lines->clear();

		for (std::string line : m_lines)
		{
			line = StripSpaces(line);

			if (remove_comments)
			{
				size_t commentPos = line.find('#');
				if (commentPos == 0)
				{
					continue;
				}

				if (commentPos != std::string::npos)
				{
					line = StripSpaces(line.substr(0, commentPos));
				}
			}

			lines->push_back(line);
		}

		return true;
	}

	// Onion layers
	BloomLayer::BloomLayer(std::unique_ptr<ConfigLayerLoader> loader)
		: m_layer(loader->GetLayer()), m_loader(std::move(loader))
	{
		Load();
	}

	BloomLayer::~BloomLayer()
	{
		Save();
	}

	bool BloomLayer::Exists(OnionSystem system, const std::string& petal_name, const std::string& key)
	{
		OnionPetal* petal = GetPetal(system, petal_name);
		if (!petal)
			return false;
		return petal->Exists(key);
	}

	bool BloomLayer::DeleteKey(OnionSystem system, const std::string& petal_name, const std::string& key)
	{
		OnionPetal* petal = GetPetal(system, petal_name);
		if (!petal)
			return false;
		return petal->Delete(key);
	}

	OnionPetal* BloomLayer::GetPetal(OnionSystem system, const std::string& petal_name)
	{
		for (OnionPetal* petal : m_petals[system])
			if (!strcasecmp(petal->m_name.c_str(), petal_name.c_str()))
				return petal;
		return nullptr;
	}

	OnionPetal* BloomLayer::GetOrCreatePetal(OnionSystem system, const std::string& petal_name)
	{
		OnionPetal* petal = GetPetal(system, petal_name);
		if (!petal)
		{
			if (m_layer == OnionLayerType::LAYER_META)
				m_petals[system].emplace_back(new RecursiveOnionPetal(m_layer, system, petal_name));
			else
				m_petals[system].emplace_back(new OnionPetal(m_layer, system, petal_name));
			petal = m_petals[system].back();
		}
		return petal;
	}

	void BloomLayer::Load()
	{
		if (m_loader)
			m_loader->Load(this);
		CallbackSystems();
	}

	void BloomLayer::Save()
	{
		if (m_loader)
			m_loader->Save(this);
	}

	OnionPetal* GetOrCreatePetal(OnionSystem system, const std::string& petal_name)
	{
		return m_layers[OnionLayerType::LAYER_META]->GetOrCreatePetal(system, petal_name);
	}

	OnionBloom* GetFullBloom()
	{
		return &m_layers;
	}

	void AddLayer(BloomLayer* layer)
	{
		m_layers[layer->GetLayer()] = layer;
		CallbackSystems();
	}

	void AddLayer(ConfigLayerLoader* loader)
	{
		BloomLayer* layer = new BloomLayer(std::unique_ptr<ConfigLayerLoader>(loader));
		AddLayer(layer);
	}

	void AddLoadLayer(BloomLayer* layer)
	{
		layer->Load();
		AddLayer(layer);
	}

	void AddLoadLayer(ConfigLayerLoader* loader)
	{
		BloomLayer* layer = new BloomLayer(std::unique_ptr<ConfigLayerLoader>(loader));
		layer->Load();
		AddLayer(loader);
	}

	BloomLayer* GetLayer(OnionLayerType layer)
	{
		if (!LayerExists(layer))
			return nullptr;
		return m_layers[layer];
	}

	void RemoveLayer(OnionLayerType layer)
	{
		m_layers.erase(layer);
		CallbackSystems();
	}
	bool LayerExists(OnionLayerType layer)
	{
		return m_layers.find(layer) != m_layers.end();
	}

	void AddConfigChangedCallback(CallbackFunction func, void* user_data)
	{
		m_callbacks.emplace_back(std::make_pair(func, user_data));
	}

	void CallbackSystems()
	{
		for (auto& callback : m_callbacks)
			callback.first(callback.second);
	}

	// Explicit load and save of layers
	void Load()
	{
		for (auto& layer : m_layers)
			layer.second->Load();

		CallbackSystems();
	}

	void Save()
	{
		for (auto& layer : m_layers)
			layer.second->Save();
	}

	void Init()
	{
		// This layer always has to exist
		m_layers[OnionLayerType::LAYER_META] = new RecursiveBloomLayer();
	}

	void Shutdown()
	{
		auto it = m_layers.begin();
		while (it != m_layers.end())
		{
			delete it->second;
			it = m_layers.erase(it);
		}

		m_callbacks.clear();
	}

	const std::string& GetSystemName(OnionSystem system)
	{
		static std::map<OnionSystem, std::string> system_to_name =
		{
			{ OnionSystem::SYSTEM_MAIN,     "Dolphin" },
			{ OnionSystem::SYSTEM_GCPAD,    "GCPad" },
			{ OnionSystem::SYSTEM_WIIPAD,   "Wiimote" },
			{ OnionSystem::SYSTEM_GFX,      "Graphics" },
			{ OnionSystem::SYSTEM_LOGGER,   "Logger" },
			{ OnionSystem::SYSTEM_DEBUGGER, "Debugger" },
			{ OnionSystem::SYSTEM_UI,       "UI" },
		};
		return system_to_name[system];
	}

	OnionSystem GetSystemFromName(const std::string& system)
	{
		std::map<std::string, OnionSystem> name_to_system =
		{
			{ "Dolphin",  OnionSystem::SYSTEM_MAIN },
			{ "GCPad",    OnionSystem::SYSTEM_GCPAD },
			{ "Wiimote",  OnionSystem::SYSTEM_WIIPAD },
			{ "Graphics", OnionSystem::SYSTEM_GFX },
			{ "Logger",   OnionSystem::SYSTEM_LOGGER },
			{ "Debugger", OnionSystem::SYSTEM_DEBUGGER },
			{ "UI",       OnionSystem::SYSTEM_UI },
		};
		return name_to_system[system];
	}

	const std::string& GetLayerName(OnionLayerType layer)
	{
		static std::map<OnionLayerType, std::string> layer_to_name =
		{
			{ OnionLayerType::LAYER_BASE,        "Base" },
			{ OnionLayerType::LAYER_GLOBALGAME,  "Global GameINI" },
			{ OnionLayerType::LAYER_LOCALGAME,   "Local GameINI" },
			{ OnionLayerType::LAYER_NETPLAY,     "Netplay" },
			{ OnionLayerType::LAYER_MOVIE,       "Movie" },
			{ OnionLayerType::LAYER_COMMANDLINE, "Command Line" },
			{ OnionLayerType::LAYER_CURRENTRUN,  "Current Run" },
			{ OnionLayerType::LAYER_META,        "Top" },
		};
		return layer_to_name[layer];
	}
}
