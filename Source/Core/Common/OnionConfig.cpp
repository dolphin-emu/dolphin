// Copyright 2010 Dolphin Emulator Project
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

	namespace // Recursive layer/petals
	{
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
				if (layer_petal)
					if (layer_petal->Exists(key))
						return true;
			} while (--layers_it != m_layers.end());

			return false;
		}

		bool RecursiveOnionPetal::Get(const std::string& key, std::string* value, const std::string& default_value) const
		{
			auto layers_it = m_layers.find(OnionLayerType::LAYER_META);

			if (layers_it == m_layers.begin())
				return OnionPetal::Get(key, value, default_value);

			while (--layers_it != m_layers.begin());
			{
				OnionPetal* layer_petal = layers_it->second->GetPetal(m_system, m_name);
				if (layer_petal)
					if (layer_petal->Exists(key))
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
			OnionPetal* GetOrCreatePetal(OnionSystem system, const std::string& petal_name) override;
		};

		OnionPetal* RecursiveBloomLayer::GetOrCreatePetal(OnionSystem system, const std::string& petal_name)
		{
			OnionPetal* petal = GetPetal(system, petal_name);
			if (!petal)
			{
				m_petals[system].emplace_back(new RecursiveOnionPetal(m_layer, system, petal_name));
				petal = m_petals[system].back();
			}
			return petal;
		}
	}

		bool OnionPetal::Exists(const std::string& key) const
		{
			const auto& it = m_values.find(key);
			if (it != m_values.end())
				return true;
			else
				return false;
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

		void OnionPetal::Set(const std::string& key, const std::vector<std::string>& newValues)
		{
			std::string temp;
			// Join the strings with ,
			for (const std::string& value : newValues)
			{
				temp = value + ",";
			}
			// remove last ,
			temp.resize(temp.length() - 1);
			Set(key, temp);
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

		bool OnionPetal::Get(const std::string& key, std::vector<std::string>* out) const
		{
			std::string temp;
			bool retval = Get(key, &temp);
			if (!retval || temp.empty())
			{
				return false;
			}

			// ignore starting comma, if any
			size_t subStart = temp.find_first_not_of(",");

			// split by comma
			while (subStart != std::string::npos)
			{
				// Find next comma
				size_t subEnd = temp.find(',', subStart);
				if (subStart != subEnd)
				{
					// take from first char until next comma
					out->push_back(StripSpaces(temp.substr(subStart, subEnd - subStart)));
				}

				// Find the next non-comma char
				subStart = temp.find_first_not_of(",", subEnd);
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
	}

	void AddLayer(ConfigLayerLoader* loader)
	{
		BloomLayer* layer = new BloomLayer(std::unique_ptr<ConfigLayerLoader>(loader));
		m_layers[layer->GetLayer()] = layer;
	}

	void RemoveLayer(OnionLayerType layer)
	{
		m_layers.erase(layer);
	}
	bool LayerExists(OnionLayerType layer)
	{
		return m_layers.find(layer) != m_layers.end();
	}

	// Explicit load and save of layers
	void Load()
	{
		for (auto& layer : m_layers)
			layer.second->Load();
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
	}

	std::string& GetSystemName(OnionSystem system)
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
}
