from enum import Enum
import dolphin
dolphin.importModule("ConfigAPI", "1.0")
import ConfigAPI

def getLayerNames_mostGlobalFirst():
    return ConfigAPI.getLayerNames_mostGlobalFirst()

def doesLayerExist(layerName):
    return ConfigAPI.doesLayerExist(layerName)


def getListOfSystems():
    return ConfigAPI.getListOfSystems()

def getConfigEnumTypes():
    return ConfigAPI.getConfigEnumTypes()

def getListOfValidValuesForEnumType(enumType):
    return ConfigAPI.getListOfValidValuesForEnumType(enumType)


class _INTERNAL_SettingTypes(Enum):
    BOOLEAN = 1,
    S32 = 2,
    U32 = 3,
    FLOAT = 4,
    STRING = 5,
    UNKNOWN = 6


_INTERNAL_resolutionTable = {}

_INTERNAL_resolutionTable["BOOL"] = _INTERNAL_SettingTypes.BOOLEAN
_INTERNAL_resolutionTable["BOOLEAN"] = _INTERNAL_SettingTypes.BOOLEAN

_INTERNAL_resolutionTable["S32"] = _INTERNAL_SettingTypes.S32
_INTERNAL_resolutionTable["SIGNED32"] = _INTERNAL_SettingTypes.S32
_INTERNAL_resolutionTable["SIGNEDINT"] = _INTERNAL_SettingTypes.S32

_INTERNAL_resolutionTable["U32"] = _INTERNAL_SettingTypes.U32
_INTERNAL_resolutionTable["UNSIGNED32"] = _INTERNAL_SettingTypes.U32
_INTERNAL_resolutionTable["UNSIGNEDINT"] = _INTERNAL_SettingTypes.U32

_INTERNAL_resolutionTable["FLOAT"] = _INTERNAL_SettingTypes.FLOAT

_INTERNAL_resolutionTable["STRING"] = _INTERNAL_SettingTypes.STRING

def ParseConfigType(settingType):
    if settingType is None or len(settingType) == 0:
        return _INTERNAL_SettingTypes.UNKNOWN
	
    typeString = settingType.upper()
    typeString = typeString.replace(" ", "")
    typeString = typeString.replace("_", "")
    typeString = typeString.replace("-", "")
    return _INTERNAL_resolutionTable.get(typeString, _INTERNAL_SettingTypes.UNKNOWN)


_INTERNAL_get_config_setting_for_layer_table = {}
_INTERNAL_get_config_setting_for_layer_table[_INTERNAL_SettingTypes.BOOLEAN] = ConfigAPI.getBooleanConfigSettingForLayer
_INTERNAL_get_config_setting_for_layer_table[_INTERNAL_SettingTypes.S32] = ConfigAPI.getSignedIntConfigSettingForLayer
_INTERNAL_get_config_setting_for_layer_table[_INTERNAL_SettingTypes.U32] = ConfigAPI.getUnsignedIntConfigSettingForLayer
_INTERNAL_get_config_setting_for_layer_table[_INTERNAL_SettingTypes.FLOAT] = ConfigAPI.getFloatConfigSettingForLayer
_INTERNAL_get_config_setting_for_layer_table[_INTERNAL_SettingTypes.STRING] = ConfigAPI.getStringConfigSettingForLayer


def getConfigSettingForLayer(settingType, layerName, systemName, sectionName, settingName):
    parsedType = ParseConfigType(settingType)
    function_to_call = _INTERNAL_get_config_setting_for_layer_table.get(parsedType, None)
    if function_to_call is not None:
        return function_to_call(layerName, systemName, sectionName, settingName)
    else:
        return ConfigAPI.getEnumConfigSettingForLayer(layerName, systemName, sectionName, settingName, settingType)


_INTERNAL_set_config_setting_for_layer_table = {}
_INTERNAL_set_config_setting_for_layer_table[_INTERNAL_SettingTypes.BOOLEAN] = ConfigAPI.setBooleanConfigSettingForLayer
_INTERNAL_set_config_setting_for_layer_table[_INTERNAL_SettingTypes.S32] = ConfigAPI.setSignedIntConfigSettingForLayer
_INTERNAL_set_config_setting_for_layer_table[_INTERNAL_SettingTypes.U32] = ConfigAPI.setUnsignedIntConfigSettingForLayer
_INTERNAL_set_config_setting_for_layer_table[_INTERNAL_SettingTypes.FLOAT] = ConfigAPI.setFloatConfigSettingForLayer
_INTERNAL_set_config_setting_for_layer_table[_INTERNAL_SettingTypes.STRING] = ConfigAPI.setStringConfigSettingForLayer


def setConfigSettingForLayer(settingType, layerName, systemName, sectionName, settingName, newValue):
    parsedType = ParseConfigType(settingType)
    function_to_call = _INTERNAL_set_config_setting_for_layer_table.get(parsedType, None)
    if function_to_call is not None:
        return function_to_call(layerName, systemName, sectionName, settingName, newValue)
    else:
        return ConfigAPI.setEnumConfigSettingForLayer(layerName, systemName, sectionName, settingName, settingType, newValue)


_INTERNAL_delete_config_setting_from_layer_table = {}
_INTERNAL_delete_config_setting_from_layer_table[_INTERNAL_SettingTypes.BOOLEAN] = ConfigAPI.deleteBooleanConfigSettingFromLayer
_INTERNAL_delete_config_setting_from_layer_table[_INTERNAL_SettingTypes.S32] = ConfigAPI.deleteSignedIntConfigSettingFromLayer
_INTERNAL_delete_config_setting_from_layer_table[_INTERNAL_SettingTypes.U32] = ConfigAPI.deleteUnsignedIntConfigSettingFromLayer
_INTERNAL_delete_config_setting_from_layer_table[_INTERNAL_SettingTypes.FLOAT] = ConfigAPI.deleteFloatConfigSettingFromLayer
_INTERNAL_delete_config_setting_from_layer_table[_INTERNAL_SettingTypes.STRING] = ConfigAPI.deleteStringConfigSettingFromLayer


def deleteConfigSettingFromLayer(settingType, layerName, systemName, sectionName, settingName):
    parsedType = ParseConfigType(settingType)
    function_to_call = _INTERNAL_delete_config_setting_from_layer_table.get(parsedType, None)
    if function_to_call is not None:
        return function_to_call(layerName, systemName, sectionName, settingName)
    else:
        return ConfigAPI.deleteEnumConfigSettingFromLayer(layerName, systemName, sectionName, settingName, settingType)


_INTERNAL_get_config_setting_table = {}
_INTERNAL_get_config_setting_table[_INTERNAL_SettingTypes.BOOLEAN] = ConfigAPI.getBooleanConfigSetting
_INTERNAL_get_config_setting_table[_INTERNAL_SettingTypes.S32] = ConfigAPI.getSignedIntConfigSetting
_INTERNAL_get_config_setting_table[_INTERNAL_SettingTypes.U32] = ConfigAPI.getUnsignedIntConfigSetting
_INTERNAL_get_config_setting_table[_INTERNAL_SettingTypes.FLOAT] = ConfigAPI.getFloatConfigSetting
_INTERNAL_get_config_setting_table[_INTERNAL_SettingTypes.STRING] = ConfigAPI.getStringConfigSetting


def getConfigSetting(settingType, systemName, sectionName, settingName):
    parsedType = ParseConfigType(settingType)
    function_to_call = _INTERNAL_get_config_setting_table.get(parsedType, None)
    if function_to_call is not None:
        return function_to_call(systemName, sectionName, settingName)
    else:
        return ConfigAPI.getEnumConfigSetting(systemName, sectionName, settingName, settingType)


def saveSettings():
    ConfigAPI.saveSettings()
