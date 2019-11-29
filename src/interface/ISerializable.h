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

#ifndef INTERFACE_ISERIALIZABLE_H_
#define INTERFACE_ISERIALIZABLE_H_

#include <pbnjson.hpp>

using namespace pbnjson;

class ISerializable {
public:
    ISerializable() {};
    virtual ~ISerializable() {};

    virtual bool fromJson(const JValue& json)
    {
        m_json = json.duplicate();
        return true;
    }

    virtual bool toJson(JValue& json)
    {
        json = m_json.duplicate();
        return true;
    }

    JValue& getJson()
    {
        return m_json;
    }

private:
    JValue m_json;

};

#endif /* INTERFACE_ISERIALIZABLE_H_ */
