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

#ifndef INTERFACE_ILISTENER_H_
#define INTERFACE_ILISTENER_H_

#include <iostream>

using namespace std;

template <class T>
class IListener {
public:
    IListener() : m_listener(nullptr) {};
    virtual ~IListener() {};

    void setListener(T* listener)
    {
        m_listener = listener;
    }

    T* getListener()
    {
        return m_listener;
    }

protected:
    T* m_listener;

};

#endif /* INTERFACE_ILISTENER_H_ */
