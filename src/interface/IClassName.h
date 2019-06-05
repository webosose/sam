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

#ifndef INTERFACE_ICLASSNAME_H_
#define INTERFACE_ICLASSNAME_H_

#include <iostream>

using namespace std;

class IClassName {
public:
    IClassName() : m_name("Unknown") {};
    virtual ~IClassName() {};

    string& getClassName()
    {
        return m_name;
    }

    void setClassName(string name)
    {
        m_name = name;
    }

private:
    string m_name;

};

#endif /* INTERFACE_ICLASSNAME_H_ */
