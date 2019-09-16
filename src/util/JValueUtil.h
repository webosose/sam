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

    static void addStringToStrArrayNoDuplicate(pbnjson::JValue& arr, std::string& str);

    static bool getValue(const JValue& json, const string& firstKey, const string& secondKey, const string& thirdKey, JValue& value);
    static bool getValue(const JValue& json, const string& firstKey, const string& secondKey, const string& thirdKey, string& value);
    static bool getValue(const JValue& json, const string& firstKey, const string& secondKey, const string& thirdKey, int& value);
    static bool getValue(const JValue& json, const string& firstKey, const string& secondKey, const string& thirdKey, bool& value);

    static bool getValue(const JValue& json, const string& mainKey, const string& subKey, JValue& value);
    static bool getValue(const JValue& json, const string& mainKey, const string& subKey, string& value);
    static bool getValue(const JValue& json, const string& mainKey, const string& subKey, int& value);
    static bool getValue(const JValue& json, const string& mainKey, const string& subKey, bool& value);

    static bool getValue(const JValue& json, const string& key, JValue& value);
    static bool getValue(const JValue& json, const string& key, string& value);
    static bool getValue(const JValue& json, const string& key, int& value);
    static bool getValue(const JValue& json, const string& key, bool& value);

    static JSchema getSchema(string name);

    static bool hasKey(const JValue& json, const string& firstKey, const string& secondKey = "", const string& thirdKey = "");

private:
    static map<string, JSchema> s_schemas;
};

#endif /* UTIL_JVALUEUTIL_H_ */
