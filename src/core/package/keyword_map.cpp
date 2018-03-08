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

#include "core/package/keyword_map.h"

#include <string.h>

#include <pbnjson.hpp>

/// ------------------------------------ public: -----------------------------------------------------------------------

KeywordMap::KeywordMap()
{

}

KeywordMap::~KeywordMap()
{
    clear();
}

static void deep_copy(const std::list<gchar*>& src, std::list<gchar*>& dest)
{
    auto it = src.begin();
    auto end = src.end();
    for (; it != end; ++it) {
        dest.push_back(g_strdup(*it));
    }
}

KeywordMap::KeywordMap(const KeywordMap& c)
{
    deep_copy(c.m_keywords, m_keywords);
}

KeywordMap& KeywordMap::operator=(const KeywordMap& rhs)
{
    if (this == &rhs)
        return *this;
    clear();
    deep_copy(rhs.m_keywords, m_keywords);
    return *this;
}

void KeywordMap::clear()
{
    auto it = m_keywords.begin();
    auto end = m_keywords.end();
    for (; it != end; ++it) {
        g_free(static_cast<gchar*>(*it));
    }
    m_keywords.clear();
}

void KeywordMap::addKeywords(pbnjson::JValue jsonArray)
{
    if (!jsonArray.isArray())
        return;

    int numItems = jsonArray.arraySize();
    for (int i=0; i < numItems; i++) {

        pbnjson::JValue key = jsonArray[i];
        if (key.isString()) {
            gchar* newKeyword = g_utf8_strdown(key.asString().c_str(), -1);
            if (newKeyword)
                m_keywords.push_back(newKeyword);
        }
    }
}

bool KeywordMap::hasMatch(const gchar* keyword, bool onlyExact) const
{
    if (!keyword || m_keywords.empty())
        return false;

    size_t keyLen = strlen(keyword);
    std::list<gchar*>::const_iterator it = m_keywords.begin();
    std::list<gchar*>::const_iterator end = m_keywords.end();
    for (; it != end; ++it) {

        const gchar* cur = static_cast<const gchar*>(*it);
        if (g_str_has_prefix(cur, keyword)) {
            if (onlyExact && keyLen != strlen(cur))
                continue;
            return true;
        }
    }
    return false;
}

const std::list<gchar*>& KeywordMap::allKeywords() const
{
    return m_keywords;
}
