// Copyright 2010 Dolphin Emulator Project
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
		LAYER_GAME,
		LAYER_NETPLAY,
		LAYER_MOVIE,
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
		void Set(const std::string& key, const std::vector<std::string>& newValues);

		// Getters
		virtual bool Get(const std::string& key, std::string* value, const std::string& default_value = NULL_STRING) const;

		bool Get(const std::string& key, int* value, int defaultValue = 0) const;
		bool Get(const std::string& key, u32* value, u32 defaultValue = 0) const;
		bool Get(const std::string& key, bool* value, bool defaultValue = false) const;
		bool Get(const std::string& key, float* value, float defaultValue = 0.0f) const;
		bool Get(const std::string& key, double* value, double defaultValue = 0.0) const;
		bool Get(const std::string& key, std::vector<std::string>* values) const;

		const std::string& GetName() { return m_name; }
		const PetalValueMap& GetValues() { return m_values; }

	protected:
		OnionLayerType m_layer;
		OnionSystem m_system;
		const std::string m_name;
		static const std::string& NULL_STRING;

		PetalValueMap m_values;
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

		OnionPetal* GetPetal(OnionSystem system, const std::string& petal_name);
		virtual OnionPetal* GetOrCreatePetal(OnionSystem system, const std::string& petal_name);

		// Explicit load and save of layers
		void Load();
		void Save();

		OnionLayerType GetLayer() const { return m_layer; }
		const BloomLayerMap& GetBloomLayerMap() { return m_petals; }

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
	void RemoveLayer(OnionLayerType layer);
	bool LayerExists(OnionLayerType layer);

	// XXX: Callback system to let systems know configuration has changed

	// Explicit load and save of layers
	void Load();
	void Save();

	void Init();
	void Shutdown();

	std::string& GetSystemName(OnionSystem system);
}
