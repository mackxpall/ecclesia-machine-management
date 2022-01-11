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

#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/file/test_filesystem.h"
#include "ecclesia/lib/http/client.h"
#include "ecclesia/lib/http/cred.pb.h"
#include "ecclesia/lib/http/curl_client.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/property_definitions.h"
#include "ecclesia/lib/redfish/testing/fake_redfish_server.h"
#include "ecclesia/lib/redfish/transport/http.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "ecclesia/lib/time/clock_fake.h"
#include "single_include/nlohmann/json.hpp"

namespace libredfish {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::UnorderedElementsAre;

// Test harness to start a FakeRedfishServer and create a RedfishInterface
// for testing.
class HttpRedfishInterfaceTest : public ::testing::Test {
 protected:
  HttpRedfishInterfaceTest() {
    server_ = std::make_unique<ecclesia::FakeRedfishServer>(
        "barebones_session_auth/mockup.shar",
        absl::StrCat(ecclesia::GetTestTempUdsDirectory(), "/mockup.socket"));
    auto config = server_->GetConfig();
    ecclesia::HttpCredential creds;
    auto curl_http_client = std::make_unique<ecclesia::CurlHttpClient>(
        ecclesia::LibCurlProxy::CreateInstance(), creds);
    auto transport = ecclesia::HttpRedfishTransport::MakeNetwork(
        std::move(curl_http_client),
        absl::StrFormat("%s:%d", config.hostname, config.port));
    auto cache = std::make_unique<ecclesia::TimeBasedCache>(
        transport.get(), &clock_, absl::Minutes(1));
    cache_ = cache.get();
    intf_ = NewHttpInterface(std::move(transport), std::move(cache),
                             RedfishInterface::kTrusted);
  }

  ecclesia::RedfishCachedGetterInterface *cache_;
  ecclesia::FakeClock clock_;
  std::unique_ptr<ecclesia::FakeRedfishServer> server_;
  std::unique_ptr<RedfishInterface> intf_;
};

TEST_F(HttpRedfishInterfaceTest, GetRoot) {
  auto root = intf_->GetRoot();
  EXPECT_THAT(nlohmann::json::parse(root.DebugString(), nullptr,
                                    /*allow_exceptions=*/false),
              Eq(nlohmann::json::parse(R"json({
  "@odata.context": "/redfish/v1/$metadata#ServiceRoot.ServiceRoot",
  "@odata.id": "/redfish/v1",
  "@odata.type": "#ServiceRoot.v1_5_0.ServiceRoot",
  "Chassis": {
      "@odata.id": "/redfish/v1/Chassis"
  },
  "Id": "RootService",
  "Links": {
      "Sessions": {
          "@odata.id": "/redfish/v1/SessionService/Sessions"
      }
  },
  "Name": "Root Service",
  "RedfishVersion": "1.6.1"
})json")));
}

TEST_F(HttpRedfishInterfaceTest, CrawlToChassisCollection) {
  auto chassis_collection = intf_->GetRoot()[libredfish::kRfPropertyChassis];
  EXPECT_THAT(nlohmann::json::parse(chassis_collection.DebugString(), nullptr,
                                    /*allow_exceptions=*/false),
              Eq(nlohmann::json::parse(R"json({
    "@odata.context": "/redfish/v1/$metadata#ChassisCollection.ChassisCollection",
    "@odata.id": "/redfish/v1/Chassis",
    "@odata.type": "#ChassisCollection.ChassisCollection",
    "Members": [
        {
            "@odata.id": "/redfish/v1/Chassis/chassis"
        }
    ],
    "Members@odata.count": 1,
    "Name": "Chassis Collection"
})json")));
}

TEST_F(HttpRedfishInterfaceTest, CrawlToChassis) {
  auto chassis_collection = intf_->GetRoot()[libredfish::kRfPropertyChassis][0];
  EXPECT_THAT(nlohmann::json::parse(chassis_collection.DebugString(), nullptr,
                                    /*allow_exceptions=*/false),
              Eq(nlohmann::json::parse(R"json({
    "@odata.context": "/redfish/v1/$metadata#Chassis.Chassis",
    "@odata.id": "/redfish/v1/Chassis/chassis",
    "@odata.type": "#Chassis.v1_10_0.Chassis",
    "Id": "chassis",
    "Name": "chassis",
    "Status": {
        "State": "StandbyOffline"
    }
})json")));
}

TEST_F(HttpRedfishInterfaceTest, GetUri) {
  auto chassis = intf_->UncachedGetUri("/redfish/v1/Chassis/chassis");
  EXPECT_THAT(nlohmann::json::parse(chassis.DebugString(), nullptr,
                                    /*allow_exceptions=*/false),
              Eq(nlohmann::json::parse(R"json({
    "@odata.context": "/redfish/v1/$metadata#Chassis.Chassis",
    "@odata.id": "/redfish/v1/Chassis/chassis",
    "@odata.type": "#Chassis.v1_10_0.Chassis",
    "Id": "chassis",
    "Name": "chassis",
    "Status": {
        "State": "StandbyOffline"
    }
})json")));
}

TEST_F(HttpRedfishInterfaceTest, GetUriFragmentString) {
  auto chassis = intf_->UncachedGetUri("/redfish/v1/Chassis/chassis#/Name");
  EXPECT_THAT(chassis.DebugString(), Eq("\"chassis\""));
}

TEST_F(HttpRedfishInterfaceTest, GetUriFragmentObject) {
  auto status = intf_->UncachedGetUri("/redfish/v1/Chassis/chassis#/Status");
  EXPECT_THAT(nlohmann::json::parse(status.DebugString(), nullptr,
                                    /*allow_exceptions=*/false),
              Eq(nlohmann::json::parse(R"json({
    "State": "StandbyOffline"
})json")));
}

TEST_F(HttpRedfishInterfaceTest, EachTest) {
  std::vector<std::string> names;
  intf_->GetRoot()[libredfish::kRfPropertyChassis].Each().Do(
      [&names](std::unique_ptr<libredfish::RedfishObject> &obj) {
        auto name = obj->GetNodeValue<libredfish::PropertyName>();
        if (name.has_value()) names.push_back(*std::move(name));
        return libredfish::RedfishIterReturnValue::kContinue;
      });
  EXPECT_THAT(names, ElementsAre("chassis"));
}

TEST_F(HttpRedfishInterfaceTest, ForEachPropertyTest) {
  auto chassis = intf_->UncachedGetUri("/redfish/v1/Chassis/chassis");
  std::vector<std::pair<std::string, std::string>> all_properties;
  chassis.AsObject()->ForEachProperty(
      [&all_properties](absl::string_view name, RedfishVariant value) {
        all_properties.push_back(
            std::make_pair(std::string(name), value.DebugString()));
        return RedfishIterReturnValue::kContinue;
      });
  EXPECT_THAT(
      all_properties,
      UnorderedElementsAre(
          std::make_pair("@odata.context",
                         "\"/redfish/v1/$metadata#Chassis.Chassis\""),
          std::make_pair("@odata.id", "\"/redfish/v1/Chassis/chassis\""),
          std::make_pair("@odata.type", "\"#Chassis.v1_10_0.Chassis\""),
          std::make_pair("Id", "\"chassis\""),
          std::make_pair("Name", "\"chassis\""),
          std::make_pair("Status", "{\"State\":\"StandbyOffline\"}")));
}

TEST_F(HttpRedfishInterfaceTest, ForEachPropertyTestStop) {
  auto chassis = intf_->UncachedGetUri("/redfish/v1/Chassis/chassis");
  std::vector<std::pair<std::string, std::string>> all_properties;
  int called = 0;
  chassis.AsObject()->ForEachProperty(
      [&called](absl::string_view name, RedfishVariant value) {
        ++called;
        return RedfishIterReturnValue::kStop;
      });
  EXPECT_THAT(called, Eq(1));
}

TEST_F(HttpRedfishInterfaceTest, CachedGet) {
  int called_count = 0;
  auto result_json = nlohmann::json::parse(R"json({
  "Id": "1",
  "Name": "MyResource",
  "Description": "My Test Resource"
})json");
  server_->AddHttpGetHandler(
      "/my/uri",
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        called_count++;
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->Reply();
      });

  // The first GET will need to hit the backend as the cache is empty.
  {
    auto result = intf_->CachedGetUri("/my/uri", GetParams{});
    EXPECT_THAT(called_count, Eq(1));
    EXPECT_THAT(nlohmann::json::parse(result.DebugString(), nullptr, false),
                Eq(result_json));
  }

  // The next GET should hit the cache. called_count should not increase.
  clock_.AdvanceTime(absl::Seconds(1));
  {
    auto result = intf_->CachedGetUri("/my/uri", GetParams{});
    EXPECT_THAT(called_count, Eq(1));
    EXPECT_THAT(nlohmann::json::parse(result.DebugString(), nullptr, false),
                Eq(result_json));
  }

  // After the age expires, called_count should increase.
  clock_.AdvanceTime(absl::Minutes(1));
  {
    auto result = intf_->CachedGetUri("/my/uri", GetParams{});
    EXPECT_THAT(called_count, Eq(2));
    EXPECT_THAT(nlohmann::json::parse(result.DebugString(), nullptr, false),
                Eq(result_json));
  }
}

TEST_F(HttpRedfishInterfaceTest, CachedGetWithOperator) {
  int parent_called_count = 0;
  int child_called_count = 0;
  auto json_parent = nlohmann::json::parse(R"json({
  "Id": "1",
  "Name": "MyResource",
  "Description": "My Test Resource",
  "Reference": { "@odata.id": "/my/other/uri" }
})json");
  auto json_child = nlohmann::json::parse(R"json({
  "Id": "2",
  "Name": "MyOtherResource",
  "Description": "My Other Test Resource"
})json");
  server_->AddHttpGetHandler(
      "/my/uri",
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        parent_called_count++;
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(json_parent.dump());
        req->Reply();
      });
  server_->AddHttpGetHandler(
      "/my/other/uri",
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        child_called_count++;
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(json_child.dump());
        req->Reply();
      });

  // The first GET will need to hit the backend as the cache is empty.
  auto parent = intf_->CachedGetUri("/my/uri", GetParams{});
  EXPECT_THAT(parent_called_count, Eq(1));
  EXPECT_THAT(nlohmann::json::parse(parent.DebugString(), nullptr, false),
              Eq(json_parent));

  // Get the child, cache is empty and will increment the child's handler once.
  auto child = parent["Reference"];
  EXPECT_THAT(child_called_count, Eq(1));
  EXPECT_THAT(nlohmann::json::parse(child.DebugString(), nullptr, false),
              Eq(json_child));

  // Getting the parent again should retrieve the cached result.
  auto parent2 = intf_->CachedGetUri("/my/uri", GetParams{});
  EXPECT_THAT(parent_called_count, Eq(1));
  EXPECT_THAT(nlohmann::json::parse(parent2.DebugString(), nullptr, false),
              Eq(json_parent));

  // Getting the child again should hit the cache.
  auto child2 = parent2["Reference"];
  EXPECT_THAT(child_called_count, Eq(1));
  EXPECT_THAT(nlohmann::json::parse(child2.DebugString(), nullptr, false),
              Eq(json_child));

  // Getting the child directly should still hit the cache.
  auto direct_child = intf_->CachedGetUri("/my/other/uri", GetParams{});
  EXPECT_THAT(child_called_count, Eq(1));
  EXPECT_THAT(nlohmann::json::parse(direct_child.DebugString(), nullptr, false),
              Eq(json_child));

  // Advance time and ensure this invalidates the cache and refetches the URI.
  clock_.AdvanceTime(absl::Seconds(1) + absl::Minutes(1));
  auto child3 = parent2["Reference"];
  EXPECT_THAT(child_called_count, Eq(2));
  EXPECT_THAT(nlohmann::json::parse(child3.DebugString(), nullptr, false),
              Eq(json_child));
}
TEST_F(HttpRedfishInterfaceTest, EnsureFreshPayloadDoesNotDoubleGet) {
  int called_count = 0;
  auto result_json = nlohmann::json::parse(R"json({
  "@odata.id": "/my/uri",
  "Id": "1",
  "Name": "MyResource",
  "Description": "My Test Resource"
})json");
  server_->AddHttpGetHandler(
      "/my/uri",
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        called_count++;
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->Reply();
      });

  // The first GET will need to hit the backend as the cache is empty.
  {
    auto result = intf_->CachedGetUri("/my/uri", GetParams{});
    EXPECT_THAT(called_count, Eq(1));
    EXPECT_THAT(nlohmann::json::parse(result.DebugString(), nullptr, false),
                Eq(result_json));

    // Converting to object and checking for freshness should not hit backend
    // again. called_count should not increase.
    auto obj = result.AsObject();
    ASSERT_TRUE(obj);
    auto new_obj = obj->EnsureFreshPayload();
    ASSERT_TRUE(new_obj);
    EXPECT_THAT(called_count, Eq(1));
    EXPECT_THAT(new_obj->DebugString(), Eq(obj->DebugString()));
  }

  // The next GET should hit the cache. called_count should not increase.
  clock_.AdvanceTime(absl::Seconds(1));
  {
    auto result = intf_->CachedGetUri("/my/uri", GetParams{});
    EXPECT_THAT(called_count, Eq(1));
    EXPECT_THAT(nlohmann::json::parse(result.DebugString(), nullptr, false),
                Eq(result_json));
    // Converting to object and checking for freshness should cause a new
    // fetch from the backend. called_count should increase.
    auto obj = result.AsObject();
    ASSERT_TRUE(obj);
    auto new_obj = obj->EnsureFreshPayload();
    ASSERT_TRUE(new_obj);
    EXPECT_THAT(called_count, Eq(2));
    EXPECT_THAT(new_obj->DebugString(), Eq(obj->DebugString()));
  }

  // After the age expires, called_count should increase.
  clock_.AdvanceTime(absl::Minutes(1));
  {
    auto result = intf_->CachedGetUri("/my/uri", GetParams{});
    EXPECT_THAT(called_count, Eq(3));
    EXPECT_THAT(nlohmann::json::parse(result.DebugString(), nullptr, false),
                Eq(result_json));

    // Converting to object and checking for freshness should not hit backend
    // again. called_count should not increase.
    auto obj = result.AsObject();
    ASSERT_TRUE(obj);
    auto new_obj = obj->EnsureFreshPayload();
    ASSERT_TRUE(new_obj);
    EXPECT_THAT(called_count, Eq(3));
    EXPECT_THAT(new_obj->DebugString(), Eq(obj->DebugString()));
  }
}
TEST_F(HttpRedfishInterfaceTest, EnsureFreshPayloadDoesNotDoubleGetUncached) {
  int called_count = 0;
  auto result_json = nlohmann::json::parse(R"json({
  "@odata.id": "/my/uri",
  "Id": "1",
  "Name": "MyResource",
  "Description": "My Test Resource"
})json");
  server_->AddHttpGetHandler(
      "/my/uri",
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        called_count++;
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->Reply();
      });

  // The first GET will need to hit the backend as the cache is empty.
  {
    auto result = intf_->CachedGetUri("/my/uri", GetParams{});
    EXPECT_THAT(called_count, Eq(1));
    EXPECT_THAT(nlohmann::json::parse(result.DebugString(), nullptr, false),
                Eq(result_json));
    // Converting to object and checking for freshness should not hit backend
    // again. called_count should not increase.
    auto obj = result.AsObject();
    ASSERT_TRUE(obj);
    auto new_obj = obj->EnsureFreshPayload();
    ASSERT_TRUE(new_obj);
    EXPECT_THAT(called_count, Eq(1));
    EXPECT_THAT(new_obj->DebugString(), Eq(obj->DebugString()));
  }

  // The next GET is explicitly uncached. called_count should increase.
  {
    auto result = intf_->UncachedGetUri("/my/uri", GetParams{});
    EXPECT_THAT(called_count, Eq(2));
    EXPECT_THAT(nlohmann::json::parse(result.DebugString(), nullptr, false),
                Eq(result_json));
    // Converting to object and checking for freshness should not hit backend
    // again. called_count should not increase.
    auto obj = result.AsObject();
    ASSERT_TRUE(obj);
    auto new_obj = obj->EnsureFreshPayload();
    ASSERT_TRUE(new_obj);
    EXPECT_THAT(called_count, Eq(2));
    EXPECT_THAT(new_obj->DebugString(), Eq(obj->DebugString()));
  }
}
TEST_F(HttpRedfishInterfaceTest, EnsureFreshPayloadFailsWithNoOdataId) {
  auto result_json = nlohmann::json::parse(R"json({
  "Id": "1",
  "Name": "MyResource",
  "Description": "My Test Resource With no @odata.id property"
})json");
  server_->AddHttpGetHandler(
      "/my/uri",
      [&](::tensorflow::serving::net_http::ServerRequestInterface *req) {
        ::tensorflow::serving::net_http::SetContentType(req,
                                                        "application/json");
        req->WriteResponseString(result_json.dump());
        req->Reply();
      });

  // First GET primes the cache.
  auto result1 = intf_->CachedGetUri("/my/uri", GetParams{});
  // Second GET returns the cached copy.
  auto result2 = intf_->CachedGetUri("/my/uri", GetParams{});
  auto obj = result2.AsObject();
  ASSERT_TRUE(obj);
  auto new_obj = obj->EnsureFreshPayload();
  EXPECT_FALSE(new_obj);
}

}  // namespace
}  // namespace libredfish
