/*
 * Copyright 2020 Google LLC
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

#include "ecclesia/lib/redfish/storage.h"

#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"

namespace libredfish {

std::vector<SmartReading> ReadSmartData(const RedfishObject &obj) {
  std::vector<SmartReading> readings = {
      {"critical_warning",
       obj.GetNodeValue<libredfish::OemGooglePropertyCriticalWarning>()},
      {"composite_temperature_kelvins",
       obj.GetNodeValue<
           libredfish::OemGooglePropertyCompositeTemperatureKelvins>()},
      {"available_spare",
       obj.GetNodeValue<libredfish::OemGooglePropertyAvailableSpare>()},
      {"available_spare_threshold",
       obj.GetNodeValue<
           libredfish::OemGooglePropertyAvailableSpareThreshold>()},
      {"critical_comp_time",
       obj.GetNodeValue<
           libredfish::OemGooglePropertyCriticalTemperatureTimeMinute>()}};
  return readings;
}

absl::optional<std::vector<SmartReading>> ReadSmartDataFromStorage(
    const RedfishObject &obj) {
  auto smart_attributes_obj =
      obj[libredfish::kRfPropertyStorageControllers][0]
      [libredfish::kRfPropertyNvmeControllersProperties]
      [libredfish::kRfPropertyOem]
      [libredfish::kRfOemPropertyGoogle]
      [libredfish::kRfOemPropertySmartAttributes].AsObject();
  if (!smart_attributes_obj) return absl::nullopt;

  return ReadSmartData(*smart_attributes_obj);
}

}  // namespace libredfish
