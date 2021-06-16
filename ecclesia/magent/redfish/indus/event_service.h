/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ECCLESIA_MAGENT_REDFISH_INDUS_EVENT_SERVICE_H_
#define ECCLESIA_MAGENT_REDFISH_INDUS_EVENT_SERVICE_H_

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ecclesia/magent/redfish/core/index_resource.h"
#include "ecclesia/magent/redfish/core/json_helper.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/sysmodel/x86/chassis.h"
#include "ecclesia/magent/sysmodel/x86/sysmodel.h"
#include "json/value.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

class EventService : public Resource {
 public:
  static constexpr char kClearAction[] = "Clear";
  EventService() : Resource(kEventServiceUri) {}

 private:
  void Get(tensorflow::serving::net_http::ServerRequestInterface *req,
           const ParamsType &params) override {
    Json::Value json;
    json[kOdataType] = "#EventService.v1_5_0.EventService";
    json[kOdataId] = std::string(Uri());
    json[kActions][absl::StrCat("#EventService.", kClearAction)][kTarget] =
        absl::StrCat(Uri(), "/Actions/EventService.", kClearAction);

    json[kName] = "Event Service";
    json[kId] = "EventService";

    JSONResponseOK(json, req);
  }
};

}  // namespace ecclesia
#endif  // ECCLESIA_MAGENT_REDFISH_INDUS_EVENT_SERVICE_H_
