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

#ifndef ECCLESIA_MAGENT_REDFISH_INDUS_SLEIPNIR_SENSOR_H_
#define ECCLESIA_MAGENT_REDFISH_INDUS_SLEIPNIR_SENSOR_H_

#include <string>

#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "ecclesia/magent/lib/ipmi/interface_options.h"
#include "ecclesia/magent/lib/ipmi/ipmi.h"
#include "ecclesia/magent/lib/ipmi/ipmitool.h"
#include "ecclesia/magent/redfish/core/index_resource.h"
#include "ecclesia/magent/redfish/core/json_helper.h"
#include "ecclesia/magent/redfish/core/redfish_keywords.h"
#include "ecclesia/magent/redfish/core/resource.h"
#include "ecclesia/magent/sysmodel/x86/sysmodel.h"
#include "json/value.h"
#include "tensorflow_serving/util/net_http/server/public/server_request_interface.h"

namespace ecclesia {

class SleipnirIpmiSensor : public IndexResource<std::string> {
 public:
  explicit SleipnirIpmiSensor(SystemModel *system_model)
      : IndexResource(kSleipnirSensorUriPattern), system_model_(system_model) {}

 private:
  void Get(tensorflow::serving::net_http::ServerRequestInterface *req,
           const ParamsType &params) override;

  void AddStaticFields(Json::Value *json) {}

  SystemModel *const system_model_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_REDFISH_INDUS_SLEIPNIR_SENSOR_H_
