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

#include "core/base/utils.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>

#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

const double NANO_SECOND = 1000000000;

std::string read_file(const std::string& file_name)
{
    std::ifstream file(file_name.c_str(), std::ifstream::in);
    std::string file_contents;

    if (file.is_open() && file.good()) {
        std::stringstream buf;
        buf << file.rdbuf();
        file_contents = buf.str();
    }

    return file_contents;
}

bool makeDir(const std::string &path, bool withParent)
{
    if (!withParent) {
        int result = mkdir(path.c_str(), 0755);
        if (result == 0 || errno == EEXIST)
            return true;
    } else {
        int result = g_mkdir_with_parents(path.c_str(), 0755);
        if (result == 0)
            return true;
    }

    return false;
}

bool writeFile(const std::string &path, const std::string& buffer)
{
    std::ofstream file(path.c_str());
    if (file.is_open()) {
        file << buffer;
        file.close();
    } else {
        return false;
    }
    return true;
}

int rmdirHelper(const char *path, const struct stat *pStat, int flag, struct FTW *ftw)
{
    switch (flag) {
    case FTW_D:
    case FTW_DP:
        if (::rmdir(path) == -1)
            return -1;
        break;

    case FTW_F:
    case FTW_SL:
        if (::unlink(path) == -1)
            return -1;
        break;
    }

    return 0;
}

bool removeDir(const std::string &path)
{
    struct stat oStat;
    memset(&oStat, 0, sizeof(oStat));
    if (lstat(path.c_str(), &oStat) == -1)
        return false;

    if (S_ISDIR(oStat.st_mode)) {
        int flags = FTW_DEPTH;
        if (::nftw(path.c_str(), rmdirHelper, 10, flags) == -1) {
            return false;
        }
    } else
        return false;

    return true;
}

bool dir_exists(const std::string& path)
{
    struct stat st_buf;
    memset(&st_buf, 0, sizeof(st_buf));
    if (lstat(path.c_str(), &st_buf) == -1)
        return false;

    return S_ISDIR(st_buf.st_mode);
}

bool removeFile(const std::string &path)
{
    if (::unlink(path.c_str()) == -1)
        return false;
    return true;
}

bool isUrlValid(const std::string& url)
{
    if (!url.size())
        return false;

    boost::regex r_url("^((https?|ftp|palm|luna)://|(file):///|(www|ftp)\\.).+$|(sprint-music:)");
    if (boost::regex_match(url, r_url)) {
        return true;
    }
    return false;
}

bool isRemoteFile(const char* uri)
{
    if (0 == strlen(uri))
        return false;
    if ((strncasecmp(uri, "file:", 5) == 0) || ('/' == uri[0]))
        return false;
    return true;
}

std::string getResourceNameFromUrl(const std::string& url)
{
    std::string resourceName;
    if (isUrlValid(url)) {
        size_t slashIndex = url.rfind('/');
        if (slashIndex != std::string::npos)
            resourceName = url.c_str() + slashIndex + 1;
    }
    return resourceName;
}

bool getExtensionFromUrl(const std::string& url, std::string& r_extn)
{
    std::string resource;
    if (isUrlValid(url)) {
        resource = getResourceNameFromUrl(url);
    } else {
        resource = url;
    }

    std::string::size_type pos = resource.rfind('.');
    if (pos != std::string::npos) {
        r_extn = resource.substr(pos + 1);
        std::transform(r_extn.begin(), r_extn.end(), r_extn.begin(), tolower);
        return true;
    }
    return false;
}

std::string trimWhitespace(const std::string& s, const std::string& drop)
{
    std::string::size_type first = s.find_first_not_of(drop);
    std::string::size_type last = s.find_last_not_of(drop);

    if (first == std::string::npos || last == std::string::npos)
        return std::string("");
    return s.substr(first, last - first + 1);
}

int splitFileAndPath(const std::string& srcPathAndFile, std::string& pathPart, std::string& filePart)
{

    std::vector<std::string> parts;
    //printf("splitFileAndPath - input [%s]\n",srcPathAndFile.c_str());
    int s = splitStringOnKey(parts, srcPathAndFile, std::string("/"));
    if ((s == 1) && (srcPathAndFile.at(srcPathAndFile.length() - 1) == '/')) {
        //only path part
        pathPart = srcPathAndFile;
        filePart = "";
    } else if (s == 1) {
        //only file part
        if (srcPathAndFile.at(0) == '/') {
            pathPart = "/";
        } else {
            pathPart = "";
        }
        filePart = parts.at(0);
    } else if (s >= 2) {
        for (int i = 0; i < s - 1; i++) {
            if ((parts.at(i)).size() == 0)
                continue;
            pathPart += std::string("/") + parts.at(i);
            //printf("splitFileAndPath - path is now [%s]\n",pathPart.c_str());
        }
        pathPart += std::string("/");
        filePart = parts.at(s - 1);
    }

    return s;
}

int splitStringOnKey(std::vector<std::string>& returnSplitSubstrings, const std::string& baseStr, const std::string& delims)
{

    std::string base = trimWhitespace(baseStr);
    std::string::size_type start = 0;
    std::string::size_type mark = 0;
    std::string extracted;

    int i = 0;
    while (start < baseStr.size()) {
        //find the start of a non-delims
        start = baseStr.find_first_not_of(delims, mark);
        if (start == std::string::npos)
            break;
        //find the end of the current substring (where the next instance of delim lives, or end of the string)
        mark = baseStr.find_first_of(delims, start);
        if (mark == std::string::npos)
            mark = baseStr.size();

        extracted = baseStr.substr(start, mark - start);
        if (extracted.size() > 0) {
            //valid string...add it
            returnSplitSubstrings.push_back(extracted);
            ++i;
        }
        start = mark;
    }

    return i;
}

bool isNonErrorProcExit(int ecode, int normalCode)
{

    if (!WIFEXITED(ecode))
        return false;
    if (WEXITSTATUS(ecode) != normalCode)
        return false;

    return true;
}

bool splitWindowIdentifierToAppAndProcessId(const std::string& id, std::string& appId, std::string& processId)
{
    if (id.empty())
        return false;

    gchar** strArray = g_strsplit(id.c_str(), " ", 2);
    if (!strArray)
        return false;

    int index = 0;
    while (strArray[index]) {

        if (index == 0)
            appId = strArray[0];
        else if (index == 1)
            processId = strArray[1];

        ++index;
    }

    g_strfreev(strArray);

    return true;
}

bool isNumber(const std::string& str)
{
    return !str.empty() && std::find_if(str.begin(), str.end(), [](char c) -> bool {return !std::isdigit(c);}) == str.end();
}

timespec diff(timespec start, timespec end)
{
    timespec temp;
    if ((end.tv_nsec - start.tv_nsec) < 0) {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return temp;
}

void set_slash_to_base_path(std::string& path)
{
    if (!path.empty() && path[path.length() - 1] == '/')
        return;

    path.append("/");
}

bool concat_to_filename(const std::string originPath, std::string& returnPath, const std::string addingStr)
{
    if (originPath.empty() || addingStr.empty())
        return false;

    returnPath = "";

    std::string dir_path, filename, name_only, ext;
    size_t pos_dir = originPath.find_last_of("/");

    if (std::string::npos == pos_dir) {
        filename = originPath;
    } else {
        pos_dir = pos_dir + 1;
        dir_path = originPath.substr(0, pos_dir);
        filename = originPath.substr(pos_dir);
    }

    size_t pos_ext = filename.find_last_of(".");

    if (std::string::npos == pos_ext)
        return false;

    name_only = filename.substr(0, pos_ext);
    ext = filename.substr(pos_ext);

    if (ext.length() < 2)
        return false;

    returnPath = dir_path + name_only + addingStr + ext;

    return true;
}

double get_current_time()
{
    timespec current_time;
    if (clock_gettime(CLOCK_MONOTONIC, &current_time) == -1)
        return -1;
    return (double) current_time.tv_sec * NANO_SECOND + (double) current_time.tv_nsec;
}

bool convertToWindowTypeSAM(const std::string& lsm, std::string& sam)
{
    // The following window types are found in appinfo.json in current application dirs.
    // SAM shoud modify this, whenever LSM add a window type.
    // For later, SAM should distinguish which is necessary for SAM/MM.
    // E.g. SAM can have only 4 window types as enum { card, minimal, screenSaver, others }.
    // In addition, window type is correct only for webapp. it can be wrong, for other type apps.
    // We shoud provide a guide for naitve developers.

    static struct {
        const std::string sam;
        const std::string lsm;
    } windowTypeMap[] = {
        { "card", "_WEBOS_WINDOW_TYPE_CARD" },
        { "favoriteShows", "_WEBOS_WINDOW_TYPE_FAVORITE_SHOWS" },
        { "favoriteshows", "_WEBOS_WINDOW_TYPE_FAVORITE_SHOWS" },
        { "floating", "_WEBOS_WINDOW_TYPE_FLOATING" },
        { "minimal", "_WEBOS_WINDOW_TYPE_RESTRICTED" },
        { "overlay", "_WEBOS_WINDOW_TYPE_OVERLAY" },
        { "popup", "_WEBOS_WINDOW_TYPE_POPUP" },
        { "screenSaver", "_WEBOS_WINDOW_TYPE_SCREENSAVER" },
        { "showcase", "_WEBOS_WINDOW_TYPE_SHOWCASE" },
    };

    static int size = sizeof(windowTypeMap) / sizeof((windowTypeMap)[0]);

    for (int i = 0; i < size; i++) {
        if (windowTypeMap[i].lsm == lsm) {
            sam = windowTypeMap[i].sam;
            return true;
        }
    }

    return false;
}

std::string generate_uid()
{
    boost::uuids::uuid uid = boost::uuids::random_generator()();
    return std::string(boost::lexical_cast<std::string>(uid));
}
