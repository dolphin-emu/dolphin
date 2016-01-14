// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <list>
#include <map>
#include <memory>
#include <string>

// XXX: Purely for case insensitive compare
#include "Common/IniFile.h"

namespace OnionConfig
{
	enum class OnionLayerType
	{
		LAYER_BASE,
		LAYER_GLOBALGAME,
		LAYER_LOCALGAME,
		LAYER_MOVIE,
		LAYER_NETPLAY,
		LAYER_COMMANDLINE,
		LAYER_CURRENTRUN,
		LAYER_META,
	};

	enum class OnionSystem
	{
		SYSTEM_MAIN,
		SYSTEM_GCPAD,
		SYSTEM_WIIPAD,
		SYSTEM_GFX,
		SYSTEM_LOGGER,
		SYSTEM_DEBUGGER,
		SYSTEM_UI,
	};

	class OnionPetal;
	class BloomLayer;
	class ConfigLayerLoader;

	// XXX: Make unordered
	using PetalValueMap = std::map<std::string, std::string, CaseInsensitiveStringCompare>;
	using BloomLayerMap = std::map<OnionSystem, std::list<OnionPetal*>>;
	// Has to be ordered!
	using OnionBloom = std::map<OnionLayerType, BloomLayer*>;
	using CallbackFunction = std::function<void(void*)>;

	class OnionPetal
	{
		friend BloomLayer;
		friend ConfigLayerLoader;

	public:
		OnionPetal(OnionLayerType layer, OnionSystem system, const std::string& name)
			: m_layer(layer), m_system(system), m_name(name) {}

		virtual bool Exists(const std::string& key) const;
		bool Delete(const std::string& key);

		// Setters
		virtual void Set(const std::string& key, const std::string& value);

		void Set(const std::string& key, u32 newValue);
		void Set(const std::string& key, float newValue);
		void Set(const std::string& key, double newValue);
		void Set(const std::string& key, int newValue);
		void Set(const std::string& key, bool newValue);

		// Setters with default values
		void Set(const std::string& key, const std::string& newValue, const std::string& defaultValue);
		template<typename T>
		void Set(const std::string& key, T newValue, const T defaultValue)
		{
			if (newValue != defaultValue)
				Set(key, newValue);
			else
				Delete(key);
		}

		// Getters
		virtual bool Get(const std::string& key, std::string* value, const std::string& default_value = NULL_STRING) const;

		bool Get(const std::string& key, int* value, int defaultValue = 0) const;
		bool Get(const std::string& key, u32* value, u32 defaultValue = 0) const;
		bool Get(const std::string& key, bool* value, bool defaultValue = false) const;
		bool Get(const std::string& key, float* value, float defaultValue = 0.0f) const;
		bool Get(const std::string& key, double* value, double defaultValue = 0.0) const;

		// Petal chunk
		void SetLines(const std::vector<std::string>& lines) { m_lines = lines; }
		// XXX: Add to recursive layer
		virtual bool GetLines(std::vector<std::string>* lines, const bool remove_comments = true) const;
		virtual bool HasLines() const { return m_lines.size() > 0; }

		const std::string& GetName() const { return m_name; }
		const PetalValueMap& GetValues() const { return m_values; }

	protected:
		OnionLayerType m_layer;
		OnionSystem m_system;
		const std::string m_name;
		static const std::string& NULL_STRING;

		PetalValueMap m_values;

		std::vector<std::string> m_lines;
	};

	// XXX: Allow easy migration!
	class ConfigLayerLoader
	{
	public:
		ConfigLayerLoader(OnionLayerType layer) : m_layer(layer) {}
		~ConfigLayerLoader() {}

		virtual void Load(BloomLayer* config_layer) = 0;
		virtual void Save(BloomLayer* config_layer) = 0;

		OnionLayerType GetLayer() const { return m_layer; }

	private:
		const OnionLayerType m_layer;
	};

	class BloomLayer
	{
	public:
		BloomLayer(OnionLayerType layer) : m_layer(layer) {}
		BloomLayer(std::unique_ptr<ConfigLayerLoader> loader);
		virtual ~BloomLayer();

		// Convenience functions
		bool Exists(OnionSystem system, const std::string& petal_name, const std::string& key);
		bool DeleteKey(OnionSystem system, const std::string& petal_name, const std::string& key);
		template<typename T> bool GetIfExists(OnionSystem system, const std::string& petal_name, const std::string& key, T* value)
		{
			if (Exists(system, petal_name, key))
				return GetOrCreatePetal(system, petal_name)->Get(key, value);

			return false;
		}

		virtual OnionPetal* GetPetal(OnionSystem system, const std::string& petal_name);
		virtual OnionPetal* GetOrCreatePetal(OnionSystem system, const std::string& petal_name);

		// Explicit load and save of layers
		void Load();
		void Save();

		OnionLayerType GetLayer() const { return m_layer; }
		const BloomLayerMap& GetBloomLayerMap() { return m_petals; }

		// Stay away from this routine as much as possible
		ConfigLayerLoader* GetLoader() { return m_loader.get(); }

	protected:
		BloomLayerMap m_petals;
		const OnionLayerType m_layer;
		std::unique_ptr<ConfigLayerLoader> m_loader;
	};

	// Common function used for getting configuration
	OnionPetal* GetOrCreatePetal(OnionSystem system, const std::string& petal_name);

	// Layer management
	OnionBloom* GetFullBloom();
	void AddLayer(BloomLayer* layer);
	void AddLayer(ConfigLayerLoader* loader);
	void AddLoadLayer(BloomLayer* layer);
	void AddLoadLayer(ConfigLayerLoader* loader);
	BloomLayer* GetLayer(OnionLayerType layer);
	void RemoveLayer(OnionLayerType layer);
	bool LayerExists(OnionLayerType layer);

	void AddConfigChangedCallback(CallbackFunction func, void* user_data);

	// Explicit load and save of layers
	void Load();
	void Save();

	void Init();
	void Shutdown();

	const std::string& GetSystemName(OnionSystem system);
	OnionSystem GetSystemFromName(const std::string& system);
	const std::string& GetLayerName(OnionLayerType layer);
}
