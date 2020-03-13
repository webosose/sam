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

#ifndef UTIL_JVALUEUTIL_H_
#define UTIL_JVALUEUTIL_H_

#include <iostream>
#include <map>
#include <pbnjson.hpp>

using namespace std;
using namespace pbnjson;

class JValueUtil {
public:
    JValueUtil() {}
    virtual ~JValueUtil() {}

    static void addUniqueItemToArray(JValue& arr, string& str);
    static JSchema getSchema(string name);

    template <typename T>
    static bool getValue(const JValue& json, const string& key, T& value) {
        if (!json)
            return false;
        if (!json.hasKey(key))
            return false;
        return convertValue(json[key], value);
    }

    template <typename... Args>
    static bool getValue(const JValue& json, const string& key, const string& nextKey, Args& ...rest) {
        if (!json)
            return false;
        if (!json.hasKey(key))
            return false;
        if (!json[key].isObject())
            return false;
        return getValue(json[key], nextKey, rest...);
    }

    template <typename... Args>
    static bool hasKey(const JValue& json, Args ...rest) {
        JValue value;
        return getValue(json, rest..., value);
    }

private:
    static bool convertValue(const JValue& json, JValue& value);
    static bool convertValue(const JValue& json, string& value);
    static bool convertValue(const JValue& json, int& value);
    static bool convertValue(const JValue& json, bool& value);

private:
    static map<string, JSchema> s_schemas;
};

#endif /* UTIL_JVALUEUTIL_H_ */
