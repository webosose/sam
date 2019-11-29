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

#include "File.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

void File::set_slash_to_base_path(string& path)
{
    if (!path.empty() && path[path.length() - 1] == '/')
        return;

    path.append("/");
}

string File::readFile(const string& file_name)
{
    ifstream file(file_name.c_str(), ifstream::in);
    string file_contents;

    if (file.is_open() && file.good()) {
        stringstream buf;
        buf << file.rdbuf();
        file_contents = buf.str();
    }

    return file_contents;
}

bool File::writeFile(const string &path, const string& buffer)
{
    ofstream file(path.c_str());
    if (file.is_open()) {
        file << buffer;
        file.close();
    } else {
        return false;
    }
    return true;
}

bool File::concatToFilename(const string originPath, string& returnPath, const string addingStr)
{
    if (originPath.empty() || addingStr.empty())
        return false;

    returnPath = "";

    string dir_path, filename, name_only, ext;
    size_t pos_dir = originPath.find_last_of("/");

    if (string::npos == pos_dir) {
        filename = originPath;
    } else {
        pos_dir = pos_dir + 1;
        dir_path = originPath.substr(0, pos_dir);
        filename = originPath.substr(pos_dir);
    }

    size_t pos_ext = filename.find_last_of(".");

    if (string::npos == pos_ext)
        return false;

    name_only = filename.substr(0, pos_ext);
    ext = filename.substr(pos_ext);

    if (ext.length() < 2)
        return false;

    returnPath = dir_path + name_only + addingStr + ext;

    return true;
}

bool File::isDirectory(const string& path)
{
    struct stat dirStat;
    if (stat(path.c_str(), &dirStat) != 0 || (dirStat.st_mode & S_IFDIR) == 0) {
        return false;
    }
    return true;
}

bool File::isFile(const string& path)
{
    struct stat fileStat;

    if (stat(path.c_str(), &fileStat) != 0 || (fileStat.st_mode & S_IFREG) == 0) {
        return false;
    }
    return true;
}

bool File::createFile(const string& path)
{
    return File::writeFile(path, "");
}

string File::join(const string& a, const string& b)
{
    string path = "";

    if (a.back() == '/') {
        if (b.front() == '/') {
            path = a + b.substr(1);
        }
        else {
            path = a + b;
        }
    } else {
        if (b.front() == '/') {
            path = a + b;
        }
        else {
            path = a + "/" + b;
        }
    }
    return path;
}

File::File()
{
}

File::~File()
{
}
