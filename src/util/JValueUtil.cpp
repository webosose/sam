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
