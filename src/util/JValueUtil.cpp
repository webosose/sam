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
#include "Environment.h"

map<string, JSchema> JValueUtil::s_schemas;

void JValueUtil::addUniqueItemToArray(pbnjson::JValue& array, string& item)
{
    if (array.isNull() || !array.isArray() || item.empty())
        return;

    for (int i = 0; i < array.arraySize(); ++i) {
        if (array[i].isString() && 0 == item.compare(array[i].asString()))
            return;
    }

    array.append(item);
}

JSchema JValueUtil::getSchema(string name)
{
    if (name.empty())
        return JSchema::AllSchema();

    auto it = s_schemas.find(name);
    if (it != s_schemas.end())
        return it->second;

    string path = PATH_SAM_SCHEMAS + name + ".schema";
    pbnjson::JSchema schema = JSchema::fromFile(path.c_str());
    if (!schema.isInitialized())
        return JSchema::AllSchema();

    s_schemas.insert(pair<string, pbnjson::JSchema>(name, schema));
    return schema;
}

bool JValueUtil::convertValue(const JValue& json, JValue& value)
{
    value = json;
    return true;
}

bool JValueUtil::convertValue(const JValue& json, string& value)
{
    if (!json.isString())
        return false;
    if (json.asString(value) != CONV_OK) {
        value = "";
        return false;
    }
    return true;
}

bool JValueUtil::convertValue(const JValue& json, int& value)
{
    if (!json.isNumber())
        return false;
    if (json.asNumber<int>(value) != CONV_OK) {
        value = 0;
        return false;
    }
    return true;
}

bool JValueUtil::convertValue(const JValue& json, bool& value)
{
    if (!json.isBoolean())
        return false;
    if (json.asBool(value) != CONV_OK) {
        value = false;
        return false;
    }
    return true;
}
