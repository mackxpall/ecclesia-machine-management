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

#ifndef ECCLESIA_LIB_REDFISH_PROXY_GRPC_H_
#define ECCLESIA_LIB_REDFISH_PROXY_GRPC_H_

#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "ecclesia/lib/atomic/sequence.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/interfaces.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.grpc.pb.h"
#include "ecclesia/lib/redfish/proto/redfish_v1.pb.h"
#include "grpcpp/client_context.h"
#include "grpcpp/server_context.h"
#include "grpcpp/support/status.h"

namespace ecclesia {

class RedfishV1GrpcProxy final : public redfish::v1::RedfishV1::Service {
 public:
  // Define a proxy with the given name that forward requests to the given stub.
  // The name is for use in logging and other debugging and tracing contexts.
  RedfishV1GrpcProxy(std::string name,
                     redfish::v1::RedfishV1::StubInterface *stub);

  // The name of the proxy service.
  const std::string &name() const { return name_; }

  // All of the proxy RPCs.
  grpc::Status Get(grpc::ServerContext *context,
                   const redfish::v1::Request *request,
                   redfish::v1::Response *response) override;
  grpc::Status Post(grpc::ServerContext *context,
                    const redfish::v1::Request *request,
                    redfish::v1::Response *response) override;
  grpc::Status Patch(grpc::ServerContext *context,
                     const redfish::v1::Request *request,
                     redfish::v1::Response *response) override;
  grpc::Status Put(grpc::ServerContext *context,
                   const redfish::v1::Request *request,
                   redfish::v1::Response *response) override;
  grpc::Status Delete(grpc::ServerContext *context,
                      const redfish::v1::Request *request,
                      redfish::v1::Response *response) override;

 private:
  // Generate a new sequence number. These numbers have no intrinsic meaning and
  // are just intended to allow log statements associated with a specific proxy
  // RPC to be matched up with each other.
  SequenceNumberGenerator::ValueType GenerateSeqNum() {
    return seq_num_generator_.GenerateValue();
  }

  // Log an info-level message associated with a specific RPC sequence number.
  // Just calls InfoLog, but prefixes it with the proxy and RPC info.
  LogMessageStream RpcInfoLog(
      SequenceNumberGenerator::ValueType seq_num,
      SourceLocation loc = SourceLocation::current()) const {
    return std::move(InfoLog(loc)
                     << "proxy(" << name_ << "), seq=" << seq_num << ": ");
  }

  // Generic method that gets called before every request is forwarded. Is given
  // the RPC name and the request. Used for any generic pre-RPC operations such
  // as logging the request.
  void PreCall(SequenceNumberGenerator::ValueType seq_num,
               absl::string_view rpc_name, grpc::ServerContext &context,
               const redfish::v1::Request &request,
               grpc::ClientContext &client_context);

  // Generic method that gets called after very request returns. It is given
  // the RPC name, the request, and the status of the result. Used for any
  // generic post-RPC operations such as logging the result of the RPC.
  void PostCall(SequenceNumberGenerator::ValueType seq_num,
                absl::string_view rpc_name, const redfish::v1::Request &request,
                const grpc::Status &rpc_status);

  std::string name_;
  SequenceNumberGenerator seq_num_generator_;
  redfish::v1::RedfishV1::StubInterface *stub_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_PROXY_GRPC_H_
