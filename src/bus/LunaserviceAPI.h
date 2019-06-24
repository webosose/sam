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

#ifndef BUS_LUNASERVICEAPI_H_
#define BUS_LUNASERVICEAPI_H_

// category
#define API_CATEGORY_GENERAL  "/"
#define API_CATEGORY_DEV      "/dev"

// core API
// core API: lifecycle
#define API_LAUNCH                              "launch"
#define API_PAUSE                               "pause"
#define API_CLOSE_BY_APPID                      "closeByAppId"
#define API_RUNNING                             "running"
#define API_GET_APP_LIFE_EVENTS                 "getAppLifeEvents"
#define API_GET_APP_LIFE_STATUS                 "getAppLifeStatus"
#define API_GET_FOREGROUND_APPINFO              "getForegroundAppInfo"
#define API_LOCK_APP                            "lockApp"
#define API_REGISTER_APP                        "registerApp"

// core API: package
#define API_LIST_APPS                           "listApps"
#define API_GET_APP_STATUS                      "getAppStatus"
#define API_GET_APP_INFO                        "getAppInfo"
#define API_GET_APP_BASE_PATH                   "getAppBasePath"

// core API: launchpoint
#define API_ADD_LAUNCHPOINT                     "addLaunchPoint"
#define API_UPDATE_LAUNCHPOINT                  "updateLaunchPoint"
#define API_REMOVE_LAUNCHPOINT                  "removeLaunchPoint"
#define API_MOVE_LAUNCHPOINT                    "moveLaunchPoint"
#define API_LIST_LAUNCHPOINTS                   "listLaunchPoints"

//---------------------------------------------------------------------------
// [ Error code list ]
//---------------------------------------------------------------------------
// common error code
#define API_ERR_CODE_GENERAL          1
#define API_ERR_CODE_INVALID_PAYLOAD  2
#define API_ERR_CODE_DEPRECATED       999

// For core API : lifecycle

// For core API : package

// For core API : launchpoint


#endif  // BUS_LUNASERVICEAPI_H_

