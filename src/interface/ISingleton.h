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

#ifndef INTERFACE_ISINGLETON_H_
#define INTERFACE_ISINGLETON_H_

#include <iostream>

using namespace std;

template <class T>
class ISingleton {
public:
    virtual ~ISingleton() {};

    static T& getInstance()
    {
        static T _instance;
        return _instance;
    }

protected:
    ISingleton() {};

};

#endif /* INTERFACE_ISINGLETON_H_ */
