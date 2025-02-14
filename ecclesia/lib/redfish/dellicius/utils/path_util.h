/*
 * Copyright 2022 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_PATH_UTIL_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_PATH_UTIL_H_

#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/interface.h"

namespace ecclesia {

// Gets all the node names in the given expression.
// parent.child.grandchild -> {parent, child, grandchild}
std::vector<std::string> SplitNodeNameForNestedNodes(absl::string_view expr);

// Helper function to resolve node_name for nested nodes if any and return json
// object to be evaluated for required property.
absl::StatusOr<nlohmann::json> ResolveNodeNameToJsonObj(
    const RedfishVariant &variant, absl::string_view node_name);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_UTILS_PATH_UTIL_H_
