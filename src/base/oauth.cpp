/*
** Taiga
** Copyright (C) 2010-2014, Eren Okka
** 
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <windows.h>
#include <winhttp.h>
#include <list>
#include <time.h>
#include <vector>

#include "base64.h"
#include "crypto.h"
#include "string.h"
#include "oauth.h"
#include "url.h"

#ifndef SIZEOF
#define SIZEOF(x) (sizeof(x) / sizeof(*x))
#endif

////////////////////////////////////////////////////////////////////////////////
// OAuth implementation is based on codebrook-twitter-oauth example code
// Copyright (c) 2010 Brook Miles
// https://code.google.com/p/codebrook-twitter-oauth/

std::wstring OAuth::BuildAuthorizationHeader(
    const std::wstring& url,
    const std::wstring& http_method,
    const OAuthParameters* post_parameters,
    const std::wstring& oauth_token,
    const std::wstring& oauth_token_secret,
    const std::wstring& pin) {
  // Build request parameters
  OAuthParameters get_parameters = ParseQueryString(UrlGetQuery(url));

  // Build signed OAuth parameters
  OAuthParameters signed_parameters =
      BuildSignedParameters(get_parameters, url, http_method, post_parameters,
                            oauth_token, oauth_token_secret, pin);

  // Build and return OAuth header
  std::wstring oauth_header = L"OAuth ";
  for (OAuthParameters::const_iterator it = signed_parameters.begin();
       it != signed_parameters.end(); ++it) {
    if (it != signed_parameters.begin())
      oauth_header += L", ";
    oauth_header += it->first + L"=\"" + it->second + L"\"";
  }
  oauth_header += L"\r\n";
  return oauth_header;
}

OAuthParameters OAuth::ParseQueryString(const std::wstring& url) {
  OAuthParameters parsed_parameters;

  std::vector<std::wstring> parameters;
  Split(url, L"&", parameters);

  for (size_t i = 0; i < parameters.size(); ++i) {
    std::vector<std::wstring> elements;
    Split(parameters[i], L"=", elements);
    if (elements.size() == 2) {
      parsed_parameters[elements[0]] = elements[1];
    }
  }

  return parsed_parameters;
}

////////////////////////////////////////////////////////////////////////////////

OAuthParameters OAuth::BuildSignedParameters(
    const OAuthParameters& get_parameters,
    const std::wstring& url,
    const std::wstring& http_method,
    const OAuthParameters* post_parameters,
    const std::wstring& oauth_token,
    const std::wstring& oauth_token_secret,
    const std::wstring& pin) {
  // Create OAuth parameters
  OAuthParameters oauth_parameters;
  oauth_parameters[L"oauth_callback"] = L"oob";
  oauth_parameters[L"oauth_consumer_key"] = consumer_key;
  oauth_parameters[L"oauth_nonce"] = CreateNonce();
  oauth_parameters[L"oauth_signature_method"] = L"HMAC-SHA1";
  oauth_parameters[L"oauth_timestamp"] = CreateTimestamp();
  oauth_parameters[L"oauth_version"] = L"1.0";

  // Add the request token
  if (!oauth_token.empty())
    oauth_parameters[L"oauth_token"] = oauth_token;
  // Add the authorization PIN
  if (!pin.empty())
    oauth_parameters[L"oauth_verifier"] = pin;

  // Create a parameter list containing both OAuth and original parameters
  OAuthParameters all_parameters = get_parameters;
  if (CompareStrings(http_method, L"POST") == 0 && post_parameters)
    all_parameters.insert(post_parameters->begin(), post_parameters->end());
  all_parameters.insert(oauth_parameters.begin(), oauth_parameters.end());

  // Prepare the signature base
  std::wstring normal_url = NormalizeUrl(url);
  std::wstring sorted_parameters = SortParameters(all_parameters);
  std::wstring signature_base = http_method +
                                L"&" + EncodeUrl(normal_url) +
                                L"&" + EncodeUrl(sorted_parameters);

  // Obtain a signature and add it to header parameters
  std::wstring signature = CreateSignature(signature_base, oauth_token_secret);
  oauth_parameters[L"oauth_signature"] = signature;

  return oauth_parameters;
}

////////////////////////////////////////////////////////////////////////////////

std::wstring OAuth::CreateNonce() {
  static const wchar_t alphanumeric[] =
      L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  std::wstring nonce;

  for (int i = 0; i <= 16; ++i)
    nonce += alphanumeric[rand() % (SIZEOF(alphanumeric) - 1)];

  return nonce;
}

std::wstring OAuth::CreateSignature(const std::wstring& signature_base,
                                    const std::wstring& oauth_token_secret) {
  // Create a SHA-1 hash of signature
  std::wstring key = EncodeUrl(consumer_secret) +
                     L"&" + EncodeUrl(oauth_token_secret);
  std::string hash = HmacSha1(WstrToStr(key), WstrToStr(signature_base));

  // Encode signature in Base64
  std::wstring signature = Base64Encode(StrToWstr(hash));

  // Return URL-encoded signature
  return EncodeUrl(signature);
}

std::wstring OAuth::CreateTimestamp() {
  __time64_t utcNow;
  __time64_t ret = _time64(&utcNow);
  wchar_t buf[100] = {'\0'};
  swprintf_s(buf, SIZEOF(buf), L"%I64u", utcNow);
  return buf;
}

std::wstring OAuth::NormalizeUrl(const std::wstring& url) {
  wchar_t scheme[1024 * 4] = {'\0'};
  wchar_t host[1024 * 4] = {'\0'};
  wchar_t path[1024 * 4] = {'\0'};

  URL_COMPONENTS components = {sizeof(URL_COMPONENTS)};
  components.lpszScheme = scheme;
  components.dwSchemeLength = SIZEOF(scheme);
  components.lpszHostName = host;
  components.dwHostNameLength = SIZEOF(host);
  components.lpszUrlPath = path;
  components.dwUrlPathLength = SIZEOF(path);

  std::wstring normal_url = url;
  if (::WinHttpCrackUrl(url.c_str(), url.size(), 0, &components)) {
    // Include port number if it is non-standard
    wchar_t port[10] = {};
    if ((CompareStrings(scheme, L"http") == 0 && components.nPort != 80) ||
        (CompareStrings(scheme, L"https") == 0 && components.nPort != 443)) {
      swprintf_s(port, SIZEOF(port), L":%u", components.nPort);
    }
    // Strip off ? and # elements in the path
    std::wstring path_only = path;
    std::wstring::size_type pos = path_only.find_first_of(L"#?");
    if (pos != std::wstring::npos)
      path_only = path_only.substr(0, pos);
    // Build normal URL
    normal_url = std::wstring(scheme) + L"://" + host + port + path_only;
  }
  return normal_url;
}

std::wstring OAuth::SortParameters(const OAuthParameters& parameters) {
  std::list<std::wstring> sorted;
  for (OAuthParameters::const_iterator it = parameters.begin();
       it != parameters.end(); ++it) {
    std::wstring param = it->first + L"=" + it->second;
    sorted.push_back(param);
  }
  sorted.sort();

  std::wstring params;
  for (std::list<std::wstring>::iterator it = sorted.begin();
       it != sorted.end(); ++it) {
    if (params.size() > 0)
      params += L"&";
    params += *it;
  }
  return params;
}

std::wstring OAuth::UrlGetQuery(const std::wstring& url) {
  std::wstring query;
  wchar_t buf[1024 * 4] = {'\0'};

  URL_COMPONENTS components = {sizeof(URL_COMPONENTS)};
  components.dwExtraInfoLength = SIZEOF(buf);
  components.lpszExtraInfo = buf;

  if (::WinHttpCrackUrl(url.c_str(), url.size(), 0, &components)) {
    query = components.lpszExtraInfo;
    std::wstring::size_type q = query.find_first_of(L'?');
    if (q != std::wstring::npos)
      query = query.substr(q + 1);
    std::wstring::size_type h = query.find_first_of(L'#');
    if (h != std::wstring::npos)
      query = query.substr(0, h);
  }

  return query;
}