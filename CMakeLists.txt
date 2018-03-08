# Copyright (c) 2012-2018 LG Electronics, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

#
# sam/CMakeLists.txt
#

cmake_minimum_required(VERSION 2.8.7)

project(sam C CXX)

include(webOS/webOS)
webos_modules_init(1 0 0 QUALIFIER RC4)
webos_component(2 0 0)
webos_distro_dep()

webos_add_compiler_flags(ALL -Wall -std=gnu++0x)
webos_add_compiler_flags(ALL -DBOOST_SIGNALS_NO_DEPRECATION_WARNING)

add_definitions(-DLOGGER_ENABLED)

#Couldn't use check_cxx_compiler_flag()
#looks like this is bug when using gcc-4.8

if(USE_ADDRESS_SANITIZER)
    set(GCC_MIN_VERSION_FOR_ADDRESS_SANITIZER 4.8.0)
    execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion
                    OUTPUT_VARIABLE GCC_VERSION)

    if(NOT GCC_VERSION VERSION_LESS GCC_MIN_VERSION_FOR_ADDRESS_SANITIZER)
        message(STATUS "Enabling address-sanitizer")
        webos_add_compiler_flags(ALL -fsanitize=address)
    endif()

endif()

#Address Sanitizer End

include(FindPkgConfig)

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${GLIB2_CFLAGS_OTHER})

pkg_check_modules(GIO2 REQUIRED gio-2.0)
include_directories(${GIO2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${GIO2_CFLAGS_OTHER})

pkg_check_modules(LUNASERVICE2 REQUIRED luna-service2)
include_directories(${LUNASERVICE2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${LUNASERVICE2_CFLAGS_OTHER})

pkg_check_modules(PBNJSON_CPP REQUIRED pbnjson_cpp)
include_directories(${PBNJSON_CPP_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${PBNJSON_CPP_CFLAGS_OTHER})

pkg_check_modules(PMLOG REQUIRED PmLogLib)
include_directories(${PMLOG_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${PMLOG_CFLAGS_OTHER})

find_package(Boost REQUIRED COMPONENTS signals regex system filesystem)
include_directories(${Boost_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${Boost_CFLAGS_OTHER})

pkg_check_modules(WEBOSI18N REQUIRED webosi18n)
include_directories(WEBOSI18N_INCLUDE_DIRS)
webos_add_compiler_flags(ALL ${WEBOSI18N_CFLAGS})

pkg_check_modules(PROCPS REQUIRED libprocps)
include_directories(PROCPS_INCLUDE_DIRS)
webos_add_compiler_flags(ALL ${PROCPS_CFLAGS})

find_library(ICU NAMES icuuc)
if(ICU STREQUAL "ICU-NOTFOUND")
   message(FATAL_ERROR "Failed to find ICU4C libraries. Please install.")
endif()

find_library(RT NAMES rt)
if(RT STREQUAL "RT-NOTFOUND")
   message(FATAL_ERROR "Failed to find rt libraries. Please link.")
endif()

# Require that all undefined symbols are satisfied by the libraries from target_link_libraries()
webos_add_linker_options(ALL --no-undefined)

# Build the sam executable
include_directories(src/fake/criu)
file(GLOB_RECURSE common_src src/core/*.cpp)
file(GLOB_RECURSE extension_src src/extensions/webos_base/*.cpp)
set(SOURCES ${common_src} ${extension_src})

webos_configure_source_files(cfg ${PROJECT_SOURCE_DIR}/src/core/setting/settings_conf.h)
webos_configure_source_files(cfg ${PROJECT_SOURCE_DIR}/src/extensions/webos_base/base_settings_conf.h)

include_directories(src)
include_directories(${PROJECT_BINARY_DIR}/Configured/src)
add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})

set(LIBS ${GLIB2_LDFLAGS}
         ${GIO2_LDFLAGS}
         ${PBNJSON_CPP_LDFLAGS}
         ${PMLOG_LDFLAGS}
         ${Boost_LIBRARIES}
         ${CRIU_LDFLAGS}
         ${ICU}
         ${RT}
         ${WEBOSI18N_LDFLAGS}
         ${PROCPS_LDFLAGS})

target_link_libraries(${CMAKE_PROJECT_NAME} ${LIBS}
                          ${LUNASERVICE2_LDFLAGS})

webos_build_daemon(RESTRICTED_PERMISSIONS)
webos_build_system_bus_files()

webos_build_configured_file(files/launch/sam.conf SYSCONFDIR init)

file(GLOB_RECURSE SCHEMAS files/schema/*.schema)
install(FILES ${SCHEMAS} DESTINATION ${WEBOS_INSTALL_WEBOS_SYSCONFDIR}/schemas/sam)

# sam conf files
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/files/conf/sam-conf.json.in ${CMAKE_CURRENT_BINARY_DIR}/files/conf/sam-conf.json)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/files/conf/sam-conf-base.json.in ${CMAKE_CURRENT_BINARY_DIR}/files/conf/sam-conf-base.json)

file(GLOB_RECURSE SAM_CONF_FILES ${CMAKE_CURRENT_BINARY_DIR}/files/conf/*)
install(FILES ${SAM_CONF_FILES} DESTINATION ${WEBOS_INSTALL_WEBOS_SYSCONFDIR})

webos_config_build_doxygen(doc Doxyfile)
