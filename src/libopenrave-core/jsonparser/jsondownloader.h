// -*- coding: utf-8 -*-
// Copyright (C) 2022 Rosen Diankov <rosen.diankov@gmail.com>
//
// This file is part of OpenRAVE.
// OpenRAVE is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
/** \file jsondownloader.h
    \brief Helper class for jsonreader to download files remotely
 */
#include "jsoncommon.h"

#include <openrave/openravejson.h>
#include <openrave/openravemsgpack.h>
#include <openrave/openrave.h>
#include <rapidjson/istreamwrapper.h>
#include <string>
#include <fstream>
#include <curl/curl.h> /// Required for remote URI


#ifndef OPENRAVE_JSON_DOWNLOADER_H
#define OPENRAVE_JSON_DOWNLOADER_H




namespace OpenRAVE
{


static bool _EndsWith(const std::string& fullString, const std::string& endString) {
    if (fullString.length() >= endString.length()) {
        return fullString.compare(fullString.length() - endString.length(), endString.length(), endString) == 0;
    }
    return false;
}


// Need to remove fragement to check for .json or .msgpack
static std::string _GetPath(const std::string& uri)
{
    std::string path = uri;
    size_t hashindex = path.find_last_of('#');

    if (hashindex != std::string::npos) {
        path = path.substr(0, hashindex);
    }
    return path;
}

/// \brief get the scheme of the uri, e.g. file: or openrave:
static void ParseURI(const std::string& uri, std::string& scheme, std::string& path, std::string& fragment)
{
    path = uri;
    size_t hashindex = path.find_last_of('#');
    if (hashindex != std::string::npos) {
        fragment = path.substr(hashindex + 1);
        path = path.substr(0, hashindex);
    }

    size_t colonindex = path.find_first_of(':');
    if (colonindex != std::string::npos) {
        // notice: in python code, like realtimerobottask3.py, it pass scheme as {openravescene: mujin}. No colon,
        scheme = path.substr(0, colonindex);
        path = path.substr(colonindex + 1);
    }

}

static std::string ResolveURI(const std::string& scheme, const std::string& path, const std::string& curdir, const std::vector<std::string>& vOpenRAVESchemeAliases)
{
    if (scheme.empty() && path.empty() ) {
        return std::string();
    }
    else if (scheme == "file")
    {
        return RaveFindLocalFile(path, curdir);
    }
    else if (find(vOpenRAVESchemeAliases.begin(), vOpenRAVESchemeAliases.end(), scheme) != vOpenRAVESchemeAliases.end())
    {
        return RaveFindLocalFile(path, curdir);
    }
    return std::string();
}

/// \brief resolve a uri
static std::string ResolveURI(const std::string& uri, const std::string& curdir, const std::vector<std::string>& vOpenRAVESchemeAliases)
{
    std::string scheme, path, fragment;
    ParseURI(uri, scheme, path, fragment);

    return ResolveURI(scheme, path, curdir, vOpenRAVESchemeAliases);
}


}



class JSONRemoteHelper
{

    /// \brief data type that is passed to curl handler containing information on parsing incoming files
    struct CurlData
    {
       rapidjson::StringBuffer buffer; // Internal buffer assigned once curl is set up to download
        CURL * curl = nullptr; // Curl handler requires to be initlized
        std::string uri;

        CurlData(){
            curl = curl_easy_init();
        };
        ~CurlData(){
            curl_easy_cleanup(curl);
        };
    };


public:
    JSONRemoteHelper(std::map<std::string, boost::shared_ptr<const rapidjson::Document> > &rapidJSONMap, std::string &remoteUrl, std::vector<std::string>& schemeVector);

    ~JSONRemoteHelper();


    /// @brief Downloads all the remote files, parse them, download other references, then store them in document map
    /// @param rEnvInfo top layer
    void DownloadRecursively(const rapidjson::Value &rEnvInfo);

    /// @brief Downloads all the remote files, parse them, download other references, then store them in document map
    /// @param referenceUri if needed to download starting from a string
    void DownloadRecursively(const std::string &referenceUri);

    void DownloadOne(const std::string& currentUri, rapidjson::Document& doc);

    void DownloadConnectedBodies();

    /// @brief Adds referenceUri to download
    /// @param referenceUri URI to download
    /// @return
    int AddReferenceURIToDownload(const std::string& referenceUri);

    bool IsUrlAlreadyStaged(std::string uri);
    


protected:


    /// @brief Write back function for libcurl, all data that is retrieves is then pushed into a rapidjson::StringBuffer
    /// @param data Data recieved from libcurl
    /// @param size size
    /// @param nmemb size
    /// @param userdata Own structure to store incoming data
    /// @return Returns the total size back to libcurl
    static size_t _WriteBackDataFromCurl(char *data, size_t size, size_t dataSize, CurlData &userdata)
    {

        std::memcpy(userdata.buffer.Push(size * dataSize),   data, size * dataSize);
        return size * dataSize;
    }
    
    /// @brief Places document into the map of loaded documents, does a check to make sure that it is not already in there
    /// @param fullURLname map key
    /// @param document  map value document
    void _PutDocumentIntoRapidJSONMap(const std::string& fullURLname, boost::shared_ptr<const rapidjson::Document> document);

    /// @brief This will parse the given document for reference URIs then put them in a queue to down in parallel
    /// @param doc document to parse
    void _ParseDocumentForNewURLs(const rapidjson::Value& doc);

    std::string _ResolveRemoteUri(const std::string &uri);

    /// \brief returns true if the referenceUri is a valid URI that can be loaded
    bool _IsExpandableReferenceUri(const std::string& referenceUri) const;


    std::map<std::string, boost::shared_ptr<const rapidjson::Document> > *_rapidJSONDocuments; ///< cache for opened rapidjson Documents
    std::string _remoteUrl; ///< remote url for scheme
    CURLM *_curlMultiHandle = nullptr; /// < curl multi handler, used to downlod files simultaneously
    std::vector<boost::shared_ptr<CurlData> > _curlDataVector; ///< Holds all curl handlers
    std::vector<std::string> _vOpenRAVESchemeAliases;
    std::vector<std::string> _urlsAlreadyStaged; ///< Holds the URLs that have already been downloaded or will be


};

#endif
