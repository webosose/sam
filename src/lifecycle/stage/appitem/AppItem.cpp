// Copyright (c) 2019 LG Electronics, Inc.
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

#include <lifecycle/stage/appitem/AppItem.h>

AppItem::AppItem(const std::string& appId, const std::string& display, const std::string& pid)
    : m_appId(appId),
      m_display(display),
      m_pid(pid)
{
    /*
     * old-style
     *    boost::uuids::uuid uid = boost::uuids::random_generator()();
     *    m_uid = boost::lexical_cast<std::string>(uid);
     */
    if (display.empty())
        m_display = "default";
    m_uid = appId +  "_" + display;
}

AppItem::~AppItem()
{

}

