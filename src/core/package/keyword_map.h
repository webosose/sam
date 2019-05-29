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

#ifndef KEYWORDMAP_H
#define KEYWORDMAP_H

/*
 * class for doing keyword matching. Used by ApplicationDescription and ApplicationManager[Service];
 * simple container for now for the std::map class but designed as a container to allow swapping in better keyword
 * matching implementations later
 *
 */

#include <list>
#include <string>

#include <glib.h>

namespace pbnjson {
class JValue;
}

class KeywordMap {
public:

    KeywordMap();
    ~KeywordMap();
    KeywordMap(const KeywordMap& c);
    KeywordMap& operator=(const KeywordMap& rhs);

    void addKeywords(pbnjson::JValue jsonArray);

    // NOTE: assumes keyword has already been lowercase'd via g_utf8_strdown
    bool hasMatch(const gchar* keyword, bool onlyExact) const;

    void clear();

    const std::list<gchar*>& allKeywords() const;

private:
    std::list<gchar*> m_keywords;
};

#endif /*  KEYWORDMAP_H  */
