dolphin:importModule("ConfigAPI", "1.0")

ConfigClass = {}

function ConfigClass:new(obj)
	obj = obj or {}
	self.__index = self
	return setmetatable(obj, self)
end

config = ConfigClass:new(nil)

function ConfigClass:getLayerNames_mostGlobalFirst()
	return ConfigAPI:getLayerNames_mostGlobalFirst()
end

function ConfigClass:doesLayerExist(layerName)
	return ConfigAPI:doesLayerExist(layerName)
end

function ConfigClass:getListOfSystems()
	return ConfigAPI:getListOfSystems()
end

function ConfigClass:getConfigEnumTypes()
	return ConfigAPI:getConfigEnumTypes()
end

function ConfigClass:getListOfValidValuesForEnumType(enumType)
	return ConfigAPI:getListOfValidValuesForEnumType(enumType)
end


SettingTypes = {BOOLEAN = 1, S32 = 2, U32 = 3, FLOAT = 4, STRING = 5, UNKNOWN = 6}

internalResolutionTable = {
BOOL = "BOOLEAN",
BOOLEAN = "BOOLEAN",

S32 = "S32",
SIGNED32 = "S32",
SIGNEDINT = "S32",

U32 = "U32",
UNSIGNED32 = "U32",
UNSIGNEDINT = "U32",

FLOAT = "FLOAT",

STRING = "STRING"
}

function ParseConfigType(settingType)
	if settingType == nil or string.len(settingType) == 0 then
		return SettingTypes.UNKNOWN
	end
	
	typeString = string.upper(settingType)
	typeString = typeString:gsub(" ", "")
	typeString = typeString:gsub("_", "")
	typeString = typeString:gsub("-", "")
	
	returnTypeString = internalResolutionTable[typeString]
	
	if returnTypeString ~= nil then
		return SettingTypes[returnTypeString]
	else
		return SettingTypes.UNKNOWN
	end
end

get_config_setting_for_layer_table = {
[SettingTypes.BOOLEAN] = function (layerName, systemName, sectionName, settingName) return ConfigAPI:getBooleanConfigSettingForLayer(layerName, systemName, sectionName, settingName) end,
[SettingTypes.S32] = function (layerName, systemName, sectionName, settingName) return ConfigAPI:getSignedIntConfigSettingForLayer(layerName, systemName, sectionName, settingName) end,
[SettingTypes.U32] = function (layerName, systemName, sectionName, settingName) return ConfigAPI:getUnsignedIntConfigSettingForLayer(layerName, systemName, sectionName, settingName) end,
[SettingTypes.FLOAT] = function (layerName, systemName, sectionName, settingName) return ConfigAPI:getFloatConfigSettingForLayer(layerName, systemName, sectionName, settingName) end,
[SettingTypes.STRING] = function (layerName, systemName, sectionName, settingName) return ConfigAPI:getStringConfigSettingForLayer(layerName, systemName, sectionName, settingName) end
}

function ConfigClass:getConfigSettingForLayer(settingType, layerName, systemName, sectionName, settingName)
	parsedType = ParseConfigType(settingType)
	function_to_call = get_config_setting_for_layer_table[parsedType]
	if function_to_call ~= nil then
		return function_to_call(layerName, systemName, sectionName, settingName)
	else
		return ConfigAPI:getEnumConfigSettingForLayer(layerName, systemName, sectionName, settingName, settingType)
	end
end

set_config_setting_for_layer_table = {
[SettingTypes.BOOLEAN] = function (layerName, systemName, sectionName, settingName, newValue) return ConfigAPI:setBooleanConfigSettingForLayer(layerName, systemName, sectionName, settingName, newValue) end,
[SettingTypes.S32] = function (layerName, systemName, sectionName, settingName, newValue) return ConfigAPI:setSignedIntConfigSettingForLayer(layerName, systemName, sectionName, settingName, newValue) end,
[SettingTypes.U32] = function (layerName, systemName, sectionName, settingName, newValue) return ConfigAPI:setUnsignedIntConfigSettingForLayer(layerName, systemName, sectionName, settingName, newValue) end,
[SettingTypes.FLOAT] = function (layerName, systemName, sectionName, settingName, newValue) return ConfigAPI:setFloatConfigSettingForLayer(layerName, systemName, sectionName, settingName, newValue) end,
[SettingTypes.STRING] = function (layerName, systemName, sectionName, settingName, newValue) return ConfigAPI:setStringConfigSettingForLayer(layerName, systemName, sectionName, settingName, newValue) end
}

function ConfigClass:setConfigSettingForLayer(settingType, layerName, systemName, sectionName, settingName, newValue)
	parsedType = ParseConfigType(settingType)
	function_to_call = set_config_setting_for_layer_table[parsedType]
	if function_to_call ~= nil then
		return function_to_call(layerName, systemName, sectionName, settingName, newValue)
	else
		return ConfigAPI:setEnumConfigSettingForLayer(layerName, systemName, sectionName, settingName, settingType, newValue)
	end
end


delete_config_setting_from_layer_table = {
	[SettingTypes.BOOLEAN] = function(layerName, systemName, sectionName, settingName) return ConfigAPI:deleteBooleanConfigSettingFromLayer(layerName, systemName, sectionName, settingName) end,
	[SettingTypes.S32] = function(layerName, systemName, sectionName, settingName) return ConfigAPI:deleteSignedIntConfigSettingFromLayer(layerName, systemName, sectionName, settingName) end,
	[SettingTypes.U32] = function(layerName, systemName, sectionName, settingName) return ConfigAPI:deleteUnsignedIntConfigSettingFromLayer(layerName, systemName, sectionName, settingName) end,
	[SettingTypes.FLOAT] = function(layerName, systemName, sectionName, settingName) return ConfigAPI:deleteFloatConfigSettingFromLayer(layerName, systemName, sectionName, settingName) end,
	[SettingTypes.STRING] = function(layerName, systemName, sectionName, settingName) return ConfigAPI:deleteStringConfigSettingFromLayer(layerName, systemName, sectionName, settingName) end
}

function ConfigClass:deleteConfigSettingFromLayer(settingType, layerName, systemName, sectionName, settingName)
	parsedType = ParseConfigType(settingType)
	function_to_call = delete_config_setting_from_layer_table[parsedType]
	if function_to_call ~= nil then
		return function_to_call(layerName, systemName, sectionName, settingName)
	else
		return ConfigAPI:deleteEnumConfigSettingFromLayer(layerName, systemName, sectionName, settingName, settingType)
	end
end

get_config_setting_table = {
[SettingTypes.BOOLEAN] = function (systemName, sectionName, settingName) return ConfigAPI:getBooleanConfigSetting(systemName, sectionName, settingName) end,
[SettingTypes.S32] = function (systemName, sectionName, settingName) return ConfigAPI:getSignedIntConfigSetting(systemName, sectionName, settingName) end,
[SettingTypes.U32] = function (systemName, sectionName, settingName) return ConfigAPI:getUnsignedIntConfigSetting(systemName, sectionName, settingName) end,
[SettingTypes.FLOAT] = function (systemName, sectionName, settingName) return ConfigAPI:getFloatConfigSetting(systemName, sectionName, settingName) end,
[SettingTypes.STRING] = function (systemName, sectionName, settingName) return ConfigAPI:getStringConfigSetting(systemName, sectionName, settingName) end
}

function ConfigClass:getConfigSetting(settingType, systemName, sectionName, settingName)
	parsedType = ParseConfigType(settingType)
	function_to_call = get_config_setting_table[parsedType]
	if function_to_call ~= nil then
		return function_to_call(systemName, sectionName, settingName)
	else
		return ConfigAPI:getEnumConfigSetting(systemName, sectionName, settingName, settingType)
	end
end

function ConfigClass:saveSettings()
	ConfigAPI:saveSettings()
end