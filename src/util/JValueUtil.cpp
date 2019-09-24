/* @@@LICENSE
 *
 * Copyright (c) 2019 LG Electronics, Inc.
 *
 * Confidential computer software. Valid license from LG required for
 * possession, use or copying. Consistent with FAR 12.211 and 12.212,
 * Commercial Computer Software, Computer Software Documentation, and
 * Technical Data for Commercial Items are licensed to the U.S. Government
 * under vendor's standard commercial license.
 *
 * LICENSE@@@
 */

#include "util/JValueUtil.h"
#include "setting/SettingsConf.h"

map<string, JSchema> JValueUtil::s_schemas;

void JValueUtil::addUniqueItemToArray(pbnjson::JValue& array, std::string& item)
{
    if (array.isNull() || !array.isArray() || item.empty())
        return;

    for (int i = 0; i < array.arraySize(); ++i) {
        if (array[i].isString() && 0 == item.compare(array[i].asString()))
            return;
    }

    array.append(item);
}

bool JValueUtil::getValue(const JValue& json, const string& firstKey, const string& secondKey, const string& thirdKey, JValue& value)
{
    if (!json.hasKey(firstKey))
        return false;
    if (!json[firstKey].isObject())
        return false;
    return getValue(json[firstKey], secondKey, thirdKey, value);
}

bool JValueUtil::getValue(const JValue& json, const string& firstKey, const string& secondKey, const string& thirdKey, string& value)
{
    if (!json.hasKey(firstKey))
        return false;
    if (!json[firstKey].isObject())
        return false;
    return getValue(json[firstKey], secondKey, thirdKey, value);
}

bool JValueUtil::getValue(const JValue& json, const string& firstKey, const string& secondKey, const string& thirdKey, int& value)
{
    if (!json.hasKey(firstKey))
        return false;
    if (!json[firstKey].isObject())
        return false;
    return getValue(json[firstKey], secondKey, thirdKey, value);
}

bool JValueUtil::getValue(const JValue& json, const string& firstKey, const string& secondKey, const string& thirdKey, bool& value)
{
    if (!json.hasKey(firstKey))
        return false;
    if (!json[firstKey].isObject())
        return false;
    return getValue(json[firstKey], secondKey, thirdKey, value);
}

bool JValueUtil::getValue(const JValue& json, const string& mainKey, const string& subKey, JValue& value)
{
    if (!json.hasKey(mainKey))
        return false;
    if (!json[mainKey].isObject())
        return false;
    return getValue(json[mainKey], subKey, value);
}

bool JValueUtil::getValue(const JValue& json, const string& mainKey, const string& subKey, string& value)
{
    if (!json.hasKey(mainKey))
        return false;
    if (!json[mainKey].isObject())
        return false;
    return getValue(json[mainKey], subKey, value);
}

bool JValueUtil::getValue(const JValue& json, const string& mainKey, const string& subKey, int& value)
{
    if (!json.hasKey(mainKey))
        return false;
    if (!json[mainKey].isObject())
        return false;
    return getValue(json[mainKey], subKey, value);
}

bool JValueUtil::getValue(const JValue& json, const string& mainKey, const string& subKey, bool& value)
{
    if (!json.hasKey(mainKey))
        return false;
    if (!json[mainKey].isObject())
        return false;
    return getValue(json[mainKey], subKey, value);
}

bool JValueUtil::getValue(const JValue& json, const string& key, JValue& value)
{
    if (!json.hasKey(key))
        return false;
    value = json[key];
    return true;
}

bool JValueUtil::getValue(const JValue& json, const string& key, string& value)
{
    if (!json.hasKey(key))
        return false;
    if (!json[key].isString())
        return false;
    if (json[key].asString(value) != CONV_OK) {
        value = "";
        return false;
    }
    return true;
}

bool JValueUtil::getValue(const JValue& json, const string& key, int& value)
{
    if (!json.hasKey(key))
        return false;
    if (!json[key].isNumber())
        return false;
    if (json[key].asNumber<int>(value) != CONV_OK) {
        value = 0;
        return false;
    }
    return true;
}

bool JValueUtil::getValue(const JValue& json, const string& key, bool& value)
{
    if (!json.hasKey(key))
        return false;
    if (!json[key].isBoolean())
        return false;
    if (json[key].asBool(value) != CONV_OK) {
        value = false;
        return false;
    }
    return true;
}

JSchema JValueUtil::getSchema(string name)
{
    if (name.empty())
        return JSchema::AllSchema();

    auto it = s_schemas.find(name);
    if (it != s_schemas.end())
        return it->second;

    std::string path = PATH_SAM_SCHEMAS + name + ".schema";
    pbnjson::JSchema schema = JSchema::fromFile(path.c_str());
    if (!schema.isInitialized())
        return JSchema::AllSchema();

    s_schemas.insert(std::pair<std::string, pbnjson::JSchema>(name, schema));
    return schema;
}

bool JValueUtil::hasKey(const JValue& json, const string& firstKey, const string& secondKey, const string& thirdKey)
{
    if (!json.isObject())
        return false;
    if (!json.hasKey(firstKey))
        return false;
    if (!secondKey.empty() && (!json[firstKey].isObject() || !json[firstKey].hasKey(secondKey)))
        return false;
    if (!thirdKey.empty() && (!json[firstKey][secondKey].isObject() || !json[firstKey][secondKey].hasKey(thirdKey)))
        return false;
    return true;
}
