// Copyright (c) 2017-2018 LG Electronics, Inc.
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

#include "core/module/logger.h"

#include <sys/stat.h>
#include <cerrno>
#include <glib.h>
#include <time.h>

#include "core/lifecycle/app_life_manager.h"
#include "core/setting/settings.h"

Logger::Logger() :
        file_no_(1), log_no_(1), max_log_file_(10), max_log_per_file_(10000)
{
}

Logger::~Logger()
{
    if (!fs_.is_open())
        return;
    fs_.close();
}

void Logger::Init(const std::string& log_path, uint32_t max_log_file, uint32_t max_log_per_file)
{
    log_path_ = log_path;
    if (max_log_file_ > 0)
        max_log_file_ = max_log_file;
    if (max_log_per_file_ > 0)
        max_log_per_file_ = max_log_per_file;
    OpenFile();
}

void Logger::CompressFile()
{
    std::string file_name = log_path_ + std::to_string(file_no_ - 1);

    const char *argv[] = { "/bin/gzip", file_name.c_str(), NULL };
    GSpawnFlags flags = (GSpawnFlags) (G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL);

    (void) g_spawn_sync(NULL, const_cast<char**>(argv), NULL, flags, NULL, NULL, NULL, NULL, NULL, NULL);
}

void Logger::OpenFile()
{
    if (fs_.is_open()) {
        fs_.close();
        CompressFile();
    }

    log_no_ = 1;

    while (file_no_ <= max_log_file_) {
        std::string file_name = log_path_ + std::to_string(file_no_);
        file_no_++;

        if (!Exist(file_name)) {
            fs_.open(file_name, std::fstream::out | std::ofstream::app);
            if (!fs_.is_open()) {
            }
            break;
        }
    }
}

bool Logger::Exist(const std::string& f)
{
    struct stat buf;
    return (stat(f.c_str(), &buf) == 0);
}

std::string Logger::GetTime()
{
    std::string time(".");
    struct timespec current_time;
    if (clock_gettime(CLOCK_MONOTONIC, &current_time) == -1)
        return time;

    time = std::to_string((long) current_time.tv_sec);
    time += "." + std::to_string(current_time.tv_nsec).substr(0, 3);
    return time;
}

void Logger::WriteLog(const std::string& data)
{

    if (log_no_ > max_log_per_file_)
        OpenFile();
    if (!fs_.is_open())
        return;

    log_no_++;
    std::string time = GetTime();

    fs_ << time << " " << data << std::endl;
}

void LifeCycleLogger::Init()
{
    std::string log_file = SettingsImpl::instance().log_path_ + "sam.lifecycle.";
    life_cycle_logger_.Init(log_file, 10, 10000);

    AppLifeManager::instance().signal_lifecycle_event.connect(boost::bind(&LifeCycleLogger::LifeEventWatcher, this, _1));
    AppLifeManager::instance().signal_app_life_status_changed.connect(boost::bind(&LifeCycleLogger::LifeStatusWatcher, this, _1, _2));
}

void LifeCycleLogger::LifeEventWatcher(const pbnjson::JValue& payload)
{

    std::string app_id = payload["appId"].asString();
    std::string event = payload["event"].asString();
    std::string log = app_id + " e." + event;

    if (payload.hasKey("reason") && payload["reason"].isString()) {
        log = log + " r:" + payload["reason"].asString();
    }
    life_cycle_logger_.WriteLog(log);
}

void LifeCycleLogger::LifeStatusWatcher(const std::string& app_id, const LifeStatus& status)
{

    std::string log = app_id + " s." + AppInfoManager::instance().life_status_to_string(status);
    life_cycle_logger_.WriteLog(log);
}

