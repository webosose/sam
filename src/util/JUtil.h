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

#ifndef JUTIL_H
#define JUTIL_H

#include <pbnjson.hpp>
#include <util/Singleton.h>
#include <string>
#include <map>


//! List of utilites for pbnjson
class JUtil: public Singleton<JUtil> {
public:
    //! Error class used in JUtil
    class Error {
    friend class JUtil;
    public:
        typedef enum {
            None = 0,
            File_Io,
            Schema,
            Parse,
        } ErrorCode;

        //! Constructor
        Error();

        //! Return ErrorCode
        ErrorCode code();

        //! Return Error Detail string
        std::string detail();

    private:
        /*! Set Error code and detail string.
         * If detail value is NULL, detail value set as default error message.
         */
        void set(ErrorCode code, const char *detail = NULL);

    private:
        ErrorCode m_code;
        std::string m_detail;
    };

    /*! Parse given json data using schema.
     * If schemaName is empty, use JSchemaFragment("{}")
     */
    static pbnjson::JValue parse(const char *rawData, const std::string &schemaName, Error *error = NULL);

    /*! Parse given json file path using schema.
     * If schemaName is empty, use JSchemaFragment("{}")
     */
    static pbnjson::JValue parseFile(const std::string &path, const std::string &schemaName, Error *error = NULL);

    /*! Load schema from file.
     * If schemaName is empty, return JSchemaFragment("{}")
     * Schema path is set to "/etc/palm/schemas/sam/" + schemaName + ".schema"
     * If cache set, find cache first and if not exist in cache load schema and store it.
     */
    pbnjson::JSchema loadSchema(const std::string &schemaName, bool cache);

    // Add a string into array if not exist
    static void addStringToStrArrayNoDuplicate(pbnjson::JValue& arr, std::string& str);

protected:
    friend class Singleton<JUtil> ;

    //! Constructor
    JUtil();

    //! Destructor
    ~JUtil();

private:
    std::map<std::string, pbnjson::JSchema> m_mapSchema;
};
#endif /* JUTIL_H */
