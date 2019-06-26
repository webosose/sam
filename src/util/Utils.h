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

#ifndef UTILS_H
#define UTILS_H

#include <glib.h>
#include <time.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct AppVersion {
    int major;
    int minor;
    int micro;

    AppVersion()
    {
        major = 0;
        minor = 0;
        micro = 0;
    }
    bool operator>(const AppVersion& other) const
    {
        if (major > other.major)
            return true;
        else if (major < other.major)
            return false;
        if (minor > other.minor)
            return true;
        else if (minor < other.minor)
            return false;
        if (micro > other.micro)
            return true;
        else if (micro < other.micro)
            return false;
        else
            return false;
    }
    bool operator==(const AppVersion& other) const
    {
        if ((major == other.major) && (minor == other.minor) && (micro == other.micro))
            return true;
        return false;
    }
};

std::string readFile(const std::string& file_name);
bool writeFile(const std::string& filePath, const std::string& buffer);
bool makeDir(const std::string &path, bool withParent = true);
bool removeDir(const std::string &path);
bool removeFile(const std::string &path);
bool dirExists(const std::string& path);

template<class T>
std::string toSTLString(const T &arg)
{
    std::ostringstream out;
    out << arg;
    return (out.str());
}

bool isUrlValid(const std::string& url);
bool isRemoteFile(const char* uri);
std::string getResourceNameFromUrl(const std::string& url);
bool getExtensionFromUrl(const std::string& url, std::string& r_extn);

std::string trimWhitespace(const std::string& s, const std::string& drop = "\r\n\t ");

#if DISABLE_FOR_SAM
bool getNthSubstring(unsigned int n,std::string& target, const std::string& str,const std::string& delims = " \t\n\r");
int splitFileAndPath(const std::string& srcPathAndFile,std::string& pathPart,std::string& filePart);
int splitFileAndExtension(const std::string& srcFileAndExt,std::string& filePart,std::string& extensionPart);
#endif

int splitStringOnKey(std::vector<std::string>& returnSplitSubstrings, const std::string& baseStr, const std::string& delims);

#if DISABLED_FOR_SAM
int remap_stdx_pipes(int readPipe, int writePipe);
#endif
bool isNonErrorProcExit(int ecode, int normalCode = 0);
#if DISABLED_FOR_SAM
std::string windowIdentifierFromAppAndProcessId(const std::string& appId, const std::string& processId);
#endif

bool splitWindowIdentifierToAppAndProcessId(const std::string& id, std::string& appId, std::string& processId);
int splitFileAndPath(const std::string& srcPathAndFile, std::string& pathPart, std::string& filePart);

#if DISABLED_FOR_SAM
std::string getHtml5DatabaseFolderNameForApp(const std::string& appId,std::string appFolderPath);
void threadCleanup();
#endif

bool isNumber(const std::string& str);
timespec diff(timespec start, timespec end);
void set_slash_to_base_path(std::string& path);
bool concatToFilename(const std::string originPath, std::string& returnPath, const std::string addingStr);
double getCurrentTime();

#if 0
std::string base64_encode(unsigned char const* , unsigned int len);
std::string base64_decode(std::string const& s);

bool doesExistOnFilesystem(const char * pathAndFile);
int fileCopy(const char * srcFileAndPath,const char * dstFileAndPath);

gboolean compare_regex (const gchar * regex,const gchar * string);

int determineEnclosingDir(const std::string& fileNameAndPath,std::string& r_enclosingDir);

// Build an std::string using printf-style formatting
std::string string_printf(const char *format, ...) G_GNUC_PRINTF(1, 2);

// Append a printf-style string to an existing std::string
std::string & append_format(std::string & str, const char * format, ...) G_GNUC_PRINTF(2, 3);
#endif

bool convertToWindowTypeSAM(const std::string& lsm, std::string& sam);
std::string generateUid();

#endif /* UTILS_H */
