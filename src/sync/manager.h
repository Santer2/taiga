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

#ifndef TAIGA_SYNC_MANAGER_H
#define TAIGA_SYNC_MANAGER_H

#include <map>
#include <memory>
#include <string>
#include "service.h"
#include "base/types.h"
#include "taiga/http.h"

namespace sync {

// The service manager handles all the communication between services and the
// application.

class Manager {
public:
  Manager();
  ~Manager();

  void MakeRequest(Request& request);
  void HandleHttpError(HttpResponse& http_response, string_t error);
  void HandleHttpResponse(HttpResponse& http_response);

  const Service* service(ServiceId service_id);
  const Service* service(const string_t& canonical_name);

  ServiceId GetServiceIdByName(const string_t& canonical_name);
  string_t GetServiceNameById(ServiceId service_id);

private:
  void HandleError(Response& response, HttpResponse& http_response);
  void HandleResponse(Response& response, HttpResponse& http_response);

  std::map<std::wstring, Request> requests_;
  std::map<ServiceId, std::unique_ptr<Service>> services_;
};

}  // namespace sync

extern sync::Manager ServiceManager;

#endif  // TAIGA_SYNC_MANAGER_H