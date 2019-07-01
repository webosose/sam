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

#include <bus/ResBundleAdaptor.h>
#include <util/JUtil.h>
#include <util/Logging.h>
#include <util/LSUtils.h>
#include <util/Utils.h>


ResBundleAdaptor::ResBundleAdaptor()
{
}

ResBundleAdaptor::~ResBundleAdaptor()
{
}

std::string ResBundleAdaptor::getLocString(const std::string& key)
{
    return m_resBundle->getLocString(key);
}

void ResBundleAdaptor::setLocale(const std::string& locale)
{
    if (locale.empty()) {
        LOG_DEBUG("locale is empty");
        return;
    }

    m_resFile = "cppstrings.json";
    m_resPath = "/usr/share/localization/sam";

    m_resBundle = std::make_shared<ResBundle>(locale, m_resFile, m_resPath);
    LOG_DEBUG("Locale is set as %s", locale.c_str());
}
