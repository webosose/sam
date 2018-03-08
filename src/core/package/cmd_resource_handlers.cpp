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

#include "core/package/cmd_resource_handlers.h"

#include <stdlib.h>
#include <string.h>

#include <boost/regex.hpp>
#include <string>

#include "core/base/jutil.h"
#include "core/package/mime_system.h"

/**
 * Constructor.
 */
RedirectHandler::RedirectHandler(const std::string& urlRe, const std::string& appId , bool schemeform, int32_t index )
    : m_urlRe(urlRe)
    , m_appId(appId)
    , m_valid(true)
    , m_schemeForm(schemeform)
    , m_tag("")
    , m_index(index)
{
    if(m_index < 0)
    {
         m_index = MimeSystem::assignIndex();
    }
    if (!urlRe.empty()){
        m_urlReg.set_expression(urlRe);
    }
}

RedirectHandler::RedirectHandler(const std::string& urlRe, const std::string& appId , bool schemeform, const std::string& handler_tag, int32_t index)
    : m_urlRe(urlRe)
    , m_appId(appId)
    , m_valid(true)
    , m_schemeForm(schemeform)
    , m_tag(handler_tag)
    , m_index(index)
{
    if(m_index < 0)
    {
         m_index = MimeSystem::assignIndex();
    }
    if (!urlRe.empty()) {
        m_urlReg.set_expression(urlRe);
    }
}

RedirectHandler::RedirectHandler(const RedirectHandler& c)
{
    m_urlRe = c.m_urlRe;
    m_appId = c.m_appId;
    m_valid = c.m_valid;
    m_tag = c.m_tag;
    m_index = c.m_index;
    m_schemeForm = c.m_schemeForm;
    m_verbs = c.m_verbs;

    if (!m_urlRe.empty()) {
        m_urlReg.set_expression(m_urlRe);
    }
}
RedirectHandler& RedirectHandler::operator=(const RedirectHandler& c)
{
    if (this == &c)
        return *this;

    m_urlRe = c.m_urlRe;
    m_appId = c.m_appId;
    m_valid = c.m_valid;
    m_tag = c.m_tag;
    m_index = c.m_index;
    m_schemeForm = c.m_schemeForm;
    m_verbs = c.m_verbs;

    if (!m_urlRe.empty()) {
        m_urlReg.set_expression(m_urlRe);
    }

    return *this;
}

RedirectHandler::RedirectHandler() : m_valid(false), m_schemeForm(false), m_index(0)
{
}

/**
 * Destructor.
 */
RedirectHandler::~RedirectHandler()
{
}

/**
 * Determine if a given URL string matches the URL regular expression
 * for this redirect handler.
 */
bool RedirectHandler::matches(const std::string& url) const
{
    return !url.empty() && reValid() && boost::regex_match(url, m_urlReg);
}

/**
 * Is the URL regular expression for this redirect handler valid?
 */
bool RedirectHandler::reValid() const
{
    return m_urlReg.status() == 0;
}

bool RedirectHandler::addVerb(const std::string& verb,const std::string& jsonizedParams)
{
    pbnjson::JValue jobj(jsonizedParams);
    if(jobj.isNull()){
        return false;
    }
    m_verbs[verb] = jsonizedParams;
    return true;
}

void RedirectHandler::removeVerb(const std::string& verb)
{
    m_verbs.erase(verb);
}

std::string RedirectHandler::toJsonString()
{
    pbnjson::JValue jobj = toJValue();
    return JUtil::jsonToString(jobj);
}

pbnjson::JValue RedirectHandler::toJValue()
{
    pbnjson::JValue jobj = pbnjson::Object();
    jobj.put("url",m_urlRe);
    jobj.put("appId",m_appId);
    jobj.put("index",m_index);
    if (m_tag.size())
    {
        jobj.put("tag",m_tag);
    }
    jobj.put("schemeForm",m_schemeForm);

    if (m_verbs.size()) {
        pbnjson::JValue jparam = pbnjson::Object();
        for (std::map<std::string,std::string>::iterator it = m_verbs.begin();
             it != m_verbs.end();++it)
        {
            jparam.put(it->first, it->second);
        }
        jobj.put("verbs", jparam);
    }
    return jobj;
}

///--------------------------------------------------------------------------------------------------------------------


ResourceHandler::ResourceHandler(const std::string& ext, const std::string& contentType,
                                 const std::string& appId, bool stream, int32_t index)
    : m_fileExt(ext)
    , m_contentType(contentType)
    , m_appId(appId)
    , m_stream(stream)
    , m_valid(true)
    , m_tag("")
    , m_index(index)
{
    if(m_index < 0)
    {
        m_index = MimeSystem::assignIndex();
    }
}

ResourceHandler::ResourceHandler(const std::string& ext,
                                 const std::string& contentType,
                                 const std::string& appId,
                                 bool stream,
                                 const std::string& handler_tag,
                                 int32_t index)
    : m_fileExt(ext)
    , m_contentType(contentType)
    , m_appId(appId)
    , m_stream(stream)
    , m_valid(true)
    , m_tag(handler_tag)
    , m_index(index)
{
    if(m_index < 0)
    {
        m_index = MimeSystem::assignIndex();
    }
}

ResourceHandler::ResourceHandler(const ResourceHandler& c)
{
    m_fileExt = c.m_fileExt;
    m_contentType = c.m_contentType;
    m_appId = c.m_appId;
    m_stream = c.m_stream;
    m_valid = c.m_valid;
    m_tag = c.m_tag;
    m_index = c.m_index;
    m_verbs = c.m_verbs;

}

ResourceHandler& ResourceHandler::operator=(const ResourceHandler& c)
{
    if (this == &c)
        return *this;
    m_fileExt = c.m_fileExt;
    m_contentType = c.m_contentType;
    m_appId = c.m_appId;
    m_stream = c.m_stream;
    m_valid = c.m_valid;
    m_tag = c.m_tag;
    m_index = c.m_index;
    m_verbs = c.m_verbs;

    return *this;
}

bool ResourceHandler::addVerb(const std::string& verb,const std::string& jsonizedParams)
{
    pbnjson::JValue jobj(jsonizedParams);
    if(jobj.isNull()){
        return false;
    }
    m_verbs[verb] = jsonizedParams;

    return true;
}

void ResourceHandler::removeVerb(const std::string& verb)
{
    m_verbs.erase(verb);
}

std::string ResourceHandler::toJsonString()
{
    pbnjson::JValue jobj = toJValue();
    return JUtil::jsonToString(jobj);
}

pbnjson::JValue ResourceHandler::toJValue()
{
    pbnjson::JValue jobj = pbnjson::Object();
    jobj.put("mime", m_contentType);
    jobj.put("extension", m_fileExt);
    jobj.put("appId", m_appId);
    jobj.put("streamable", m_stream);
    jobj.put("index", m_index);
    if (m_tag.size())
    {
        jobj.put("tag", m_tag);
    }
    if (m_verbs.size()) {
        pbnjson::JValue jparam = pbnjson::Object();
        for (std::map<std::string,std::string>::iterator it = m_verbs.begin();
             it != m_verbs.end();++it)
        {
            jparam.put(it->first, it->second);
        }
        jobj.put("verbs", jparam);
    }
    return jobj;
}
