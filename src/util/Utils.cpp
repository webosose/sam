// Copyright (c) 2012-2019 LG Electronics, Inc.
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
#include <util/Utils.h>

const double NANO_SECOND = 1000000000;

std::string readFile(const std::string& file_name)
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

std::string trimWhitespace(const std::string& s, const std::string& drop)
{
    std::string::size_type first = s.find_first_not_of(drop);
    std::string::size_type last = s.find_last_not_of(drop);

    if (first == std::string::npos || last == std::string::npos)
        return std::string("");
    return s.substr(first, last - first + 1);
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

bool concatToFilename(const std::string originPath, std::string& returnPath, const std::string addingStr)
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

double getCurrentTime()
{
    timespec current_time;
    if (clock_gettime(CLOCK_MONOTONIC, &current_time) == -1)
        return -1;
    return (double) current_time.tv_sec * NANO_SECOND + (double) current_time.tv_nsec;
}

std::string generateUid()
{
    boost::uuids::uuid uid = boost::uuids::random_generator()();
    return std::string(boost::lexical_cast<std::string>(uid));
}
