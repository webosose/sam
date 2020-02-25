// Copyright (c) 2019-2020 LG Electronics, Inc.
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

#include "Logger.h"

#include <PmLogLib.h>

#include <string.h>

const string Logger::EMPTY = "";

void Logger::logAPIRequest(const string& className, const string& functionName, Message& request, JValue& requestPayload)
{
    if (isVerbose()) {
        if (request.getSenderServiceName())
            getInstance().write(LogLevel_INFO, className, functionName, "APIRequest", format("API(%s) Sender(%s)", request.getKind(), request.getSenderServiceName()), requestPayload.stringify("    "));
        else
            getInstance().write(LogLevel_INFO, className, functionName, "APIRequest", format("API(%s) Sender(%s)", request.getKind(), request.getApplicationID()), requestPayload.stringify("    "));
    } else {
        if (request.getSenderServiceName())
            getInstance().write(LogLevel_INFO, className, functionName, "APIRequest", format("API(%s) Sender(%s)", request.getKind(), request.getSenderServiceName()), EMPTY);
        else
            getInstance().write(LogLevel_INFO, className, functionName, "APIRequest", format("API(%s) Sender(%s)", request.getKind(), request.getApplicationID()), EMPTY);
    }
}

void Logger::logAPIResponse(const string& className, const string& functionName, Message& request, JValue& responsePayload)
{
    if (isVerbose()) {
        if (request.getSenderServiceName())
            getInstance().write(LogLevel_INFO, className, functionName, "APIResponse", format("API(%s) Sender(%s)", request.getKind(), request.getSenderServiceName()), responsePayload.stringify("    "));
        else
            getInstance().write(LogLevel_INFO, className, functionName, "APIResponse", format("API(%s) Sender(%s)", request.getKind(), request.getApplicationID()), responsePayload.stringify("    "));
    } else {
        if (request.getSenderServiceName())
            getInstance().write(LogLevel_INFO, className, functionName, "APIResponse", format("API(%s) Sender(%s)", request.getKind(), request.getSenderServiceName()), EMPTY);
        else
            getInstance().write(LogLevel_INFO, className, functionName, "APIResponse", format("API(%s) Sender(%s)", request.getKind(), request.getApplicationID()), EMPTY);
    }
}

void Logger::logCallRequest(const string& className, const string& functionName, const string& method, JValue& requestPayload)
{
    if (isVerbose())
        getInstance().write(LogLevel_INFO, className, functionName, "CallRequest", method.c_str(), requestPayload.stringify("    "));
    else
        getInstance().write(LogLevel_INFO, className, functionName, "CallRequest", method.c_str(), EMPTY);
}

void Logger::logCallResponse(const string& className, const string& functionName, Message& response, JValue& responsePayload)
{
    if (isVerbose())
        getInstance().write(LogLevel_INFO, className, functionName, "CallResponse", response.getSenderServiceName(), responsePayload.stringify("    "));
    else
        getInstance().write(LogLevel_INFO, className, functionName, "CallResponse", response.getSenderServiceName(), EMPTY);
}

void Logger::logSubscriptionRequest(const string& className, const string& functionName, const string& method, JValue& requestPayload)
{
    if (isVerbose())
        getInstance().write(LogLevel_INFO, className, functionName, "SubscriptionRequest", method.c_str(), requestPayload.stringify("    "));
    else
        getInstance().write(LogLevel_INFO, className, functionName, "SubscriptionRequest", method.c_str(), EMPTY);
}

void Logger::logSubscriptionResponse(const string& className, const string& functionName, Message& response, JValue& subscriptionPayload)
{
    if (isVerbose())
        getInstance().write(LogLevel_INFO, className, functionName, "SubscriptionResponse", response.getSenderServiceName(), subscriptionPayload.stringify("    "));
    else
        getInstance().write(LogLevel_INFO, className, functionName, "SubscriptionResponse", response.getSenderServiceName(), EMPTY);
}

void Logger::logSubscriptionPost(const string& className, const string& functionName, const LS::SubscriptionPoint& point, JValue& subscriptionPayload)
{
    if (isVerbose())
        getInstance().write(LogLevel_INFO, className, functionName, "SubscriptionPost", Logger::format("Count=%d", point.getSubscribersCount()), subscriptionPayload.stringify("    "));
    else
        getInstance().write(LogLevel_INFO, className, functionName, "SubscriptionPost", Logger::format("Count=%d", point.getSubscribersCount()), EMPTY);
}

void Logger::logSubscriptionPost(const string& className, const string& functionName, const string& key, JValue& subscriptionPayload)
{
    if (isVerbose())
        getInstance().write(LogLevel_INFO, className, functionName, "SubscriptionPost", key, subscriptionPayload.stringify("    "));
    else
        getInstance().write(LogLevel_INFO, className, functionName, "SubscriptionPost", key, EMPTY);
}

void Logger::debug(const string& className, const string& functionName, const string& what)
{
    getInstance().write(LogLevel_DEBUG, className, functionName, EMPTY, what, EMPTY);
}

void Logger::info(const string& className, const string& functionName, const string& what)
{
    getInstance().write(LogLevel_INFO, className, functionName, EMPTY, what, EMPTY);
}

void Logger::warning(const string& className, const string& functionName, const string& what)
{
    getInstance().write(LogLevel_WARNING, className, functionName, EMPTY, what, EMPTY);
}

void Logger::error(const string& className, const string& functionName, const string& what)
{
    getInstance().write(LogLevel_ERROR, className, functionName, EMPTY, what, EMPTY);
}

void Logger::debug(const string& className, const string& functionName, const string& who, const string& what, const string& detail)
{
    getInstance().write(LogLevel_DEBUG, className, functionName, who, what, detail);
}

void Logger::info(const string& className, const string& functionName, const string& who, const string& what, const string& detail)
{
    getInstance().write(LogLevel_INFO, className, functionName, who, what, detail);
}

void Logger::warning(const string& className, const string& functionName, const string& who, const string& what, const string& detail)
{
    getInstance().write(LogLevel_WARNING, className, functionName, who, what, detail);
}

void Logger::error(const string& className, const string& functionName, const string& who, const string& what, const string& detail)
{
    getInstance().write(LogLevel_ERROR, className, functionName, who, what, detail);
}

const string& Logger::toString(const enum LogLevel& level)
{
    static const string DEBUG = "D";
    static const string INFO = "I";
    static const string WARNING = "W";
    static const string ERROR = "E";

    switch(level) {
    case LogLevel_DEBUG:
        return DEBUG;

    case LogLevel_INFO:
        return INFO;

    case LogLevel_WARNING:
        return WARNING;

    case LogLevel_ERROR:
       return ERROR;
    }
    return DEBUG;
}

Logger::Logger()
    : m_level(LogLevel_DEBUG),
      m_type(LogType_CONSOLE)
{
}

Logger::~Logger()
{
}

void Logger::setLevel(enum LogLevel level)
{
    m_level = level;
}

void Logger::setType(enum LogType type)
{
    m_type = type;
}

void Logger::write(const enum LogLevel& level, const string& className, const string& functionName, const string& who, const string& what, const string& detail)
{
    if (level < m_level)
        return;

    switch (m_type) {
    case LogType_CONSOLE:
        writeConsole(level, className, functionName, who, what, detail);
        break;

    case LogType_PMLOG:
        writePmlog(level, className, functionName, who, what, detail);
        break;

    default:
        cerr << "Unsupported Log Type" << endl;
        break;
    }
}

void Logger::writeConsole(const enum LogLevel& level, const string& className, const string& functionName, const string& who, const string& what, const string& detail)
{
    const string& str = toString(level);
    if (who.empty() && detail.empty()) {
        printf("[%s][%s][%s] %s\n", str.c_str(), className.c_str(), functionName.c_str(), what.c_str());
    } else if (who.empty()) {
        printf("[%s][%s][%s] %s\n%s\n", str.c_str(), className.c_str(), functionName.c_str(), what.c_str(), detail.c_str());
    } else if (detail.empty()) {
        printf("[%s][%s][%s][%s] %s\n", str.c_str(), className.c_str(), functionName.c_str(), who.c_str(), what.c_str());
    } else {
        printf("[%s][%s][%s][%s] %s\n%s\n", str.c_str(), className.c_str(), functionName.c_str(), who.c_str(), what.c_str(), detail.c_str());
    }
}

void Logger::writePmlog(const enum LogLevel& level, const string& className, const string& functionName, const string& who, const string& what, const string& detail)
{
    static PmLogContext context = nullptr;
    if (context == nullptr) {
        context = PmLogGetContextInline("SAM");
    }

    // TODO handle empty who
    switch(level) {
    case LogLevel_DEBUG:
        PmLogDebug(context, detail.c_str());
        break;

    case LogLevel_INFO:
        if (detail.empty()) {
            PmLogInfo(context, className.c_str(), 3,
                      PMLOGKS("function", functionName.c_str()),
                      PMLOGKS("who", who.c_str()),
                      PMLOGKS("what", what.c_str()), "");
        } else {
            PmLogInfo(context, className.c_str(), 3,
                      PMLOGKS("function", functionName.c_str()),
                      PMLOGKS("who", who.c_str()),
                      PMLOGKS("what", what.c_str()), "%s", detail.c_str());
        }
        break;

    case LogLevel_WARNING:
        if (detail.empty()) {
            PmLogWarning(context, className.c_str(), 3,
                         PMLOGKS("function", functionName.c_str()),
                         PMLOGKS("who", who.c_str()),
                         PMLOGKS("what", what.c_str()), "");
        } else {
            PmLogWarning(context, className.c_str(), 3,
                         PMLOGKS("function", functionName.c_str()),
                         PMLOGKS("who", who.c_str()),
                         PMLOGKS("what", what.c_str()), "%s", detail.c_str());
        }
        break;

    case LogLevel_ERROR:
        if (detail.empty()) {
            PmLogError(context, className.c_str(), 3,
                       PMLOGKS("function", functionName.c_str()),
                       PMLOGKS("who", who.c_str()),
                       PMLOGKS("what", what.c_str()), "");
        } else {
            PmLogError(context, className.c_str(), 3,
                       PMLOGKS("function", functionName.c_str()),
                       PMLOGKS("who", who.c_str()),
                       PMLOGKS("what", what.c_str()), "%s", detail.c_str());
        }
        break;
    }
}
