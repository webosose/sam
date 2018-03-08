// Copyright (c) 2012-2018 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef APPLICATIONERRORS_H
#define APPLICATIONERRORS_H

#include <string>

#include <pbnjson.hpp>

//! error value from applicationManager
////////////////////////////////////////////////////////////////////////
// POLICY:
//   These error codes are for external return
//   Do not add error code for specific internal error case
//   Consider when adding new codes
//   1. whether it's general case or product specific case
//   2. whether it's internal error or not
//      e.g.) it means it's not general
//            if error codes are subject to change when refactoring codes
////////////////////////////////////////////////////////////////////////

// TODO:
// 1. restructure product specific error codes
// 2. remove internal error codes
enum
{
    SAM_ERR_NONE                  =  0,

    //error related to app sequence internally
    APP_LAUNCH_ERR_GENERAL        = -1,
    SCHEMA_ERR_BADPARAM           = -2,
    MEM_ERR_NOT_ENOUGH            = -3,
    MEM_ERR_ALLOCATE              = -4,
    DOWNLOAD_SUPPORT_ERR          = -5,
    HANDLER_ERR_NO_HANDLE         = -6,
    ONLAUNCHED_ERR_SEND           = -7,
    PERMISSION_DENIED             = -8,

    //error related app's property or type
    APP_ERR_NOT_FOUND_UNSUPPORT   = -101,
    APP_ERR_LOCKED                = -102,
    APP_ERR_SECURITY_CHECK        = -103,
    APP_ERR_PRIVILEGED            = -104,
    APP_ERR_INVALID_APPID         = -105,
    APP_ERR_UNSUPPORTED           = -106,
    APP_ERR_CHECK_LASTAPP         = -107,
    APP_ERR_CHECK_PRELOADED       = -108,
    APP_ERR_AUTO_SWITCH           = -109,

    //error related app's status
    APP_ERR_NATIVE_WAITING_REG    = -201,
    APP_ERR_NATIVE_SPAWN          = -202,
    APP_ERR_NATIVE_IS_LAUNCHING   = -203,
    APP_ERR_NATIVE_BOOSTER_LS2    = -204,
    APP_ERR_NATIVE_BOOSTER_PID    = -205,
    APP_ERR_NATIVE_BOOSTER_APPID  = -206,
    APP_IS_ALREADY_RUNNING        = -207,
    APP_IS_NOT_RUNNING            = -208,

    //error in callchain
    //there are releated to external service
    CALLCHANIN_ERR_GENERAL        = -300,
    INSTALLRO_ERR                 = -301,
    DRM_ERR_CHECK                 = -302,
    APPSIGNING_ERR_CHECK          = -303,
    PARENTAL_ERR_GET_SETTINGVAL   = -304,
    PARENTAL_ERR_CEHCK_PINCODE    = -305,
    PARENTAL_ERR_UNMATCH_PINCODE  = -306,
    DBREGISTER_ERR_KEYFIND        = -308,
    DBREGISTER_ERR_KINDREG        = -309,
    DBREGISTER_ERR_PERMITREG      = -310,
    EULA_ERR_CHECK                = -311,
    EULA_NOT_ACCEPTED             = -312,
    APPUPDATE_POPUP_ERR           = -313,
    OUTOFSERVICE_POPUP_ERR        = -314,
    VERIFY_APP_SIGNING_ERR        = -315,

    //related to virtual app
    VIRTUALAPP_ERR_CREATE_GENERAL = -400,
    VIRTUALAPP_ERR_REMOVE_GENERAL = -401,

    ERROR_UNKNOWN                 = -0xFFFF // -65535
};

//! error structure from applicationManger
typedef struct _ErrorInfo
{
    int errorCode;
    std::string errorText;

    _ErrorInfo()
        : errorCode(SAM_ERR_NONE)
        , errorText("")
    {}

    void setError(int _errorCode, const std::string& _errorText)
    {
        errorCode = _errorCode;
        errorText = _errorText;
    }

    void toJson(pbnjson::JValue& json)
    {
        if (SAM_ERR_NONE == errorCode)
        {
            json.put("returnValue", true);
        }
        else
        {
            json.put("returnValue", false);
            json.put("errorCode", errorCode);
            json.put("errorText", errorText);
        }
    }
}ErrorInfo;

#endif

