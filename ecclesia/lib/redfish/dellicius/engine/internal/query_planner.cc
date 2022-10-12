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

#include "ecclesia/lib/redfish/dellicius/engine/internal/query_planner.h"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/functional/function_ref.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/redfish/dellicius/query/query_result.pb.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/time/proto.h"

namespace ecclesia {

namespace {

constexpr absl::string_view kPredicateSelectAll = "*";

bool ApplySelectAllFilter(const RedfishVariant & /*variant*/) { return true; }

// Only Checks if predicate expression is enclosed in square brackets.
absl::StatusOr<std::pair<std::string, std::string>> GetNodeAndPredicate(
    absl::string_view step) {
  size_t predicate_start = step.find_first_of('[');
  size_t predicate_end = step.find_first_of(']');
  if ((predicate_start == std::string::npos) ||
      (predicate_end == std::string::npos)) {
    return absl::InvalidArgumentError("Invalid location step expression");
  }
  absl::string_view predicate_expr =
      step.substr(predicate_start + 1, (predicate_end - predicate_start - 1));
  absl::string_view node_name = step.substr(0, predicate_start);
  return std::make_pair(std::string(node_name), std::string(predicate_expr));
}

}  // namespace

QueryPlanner::SubqueryHandle::SubqueryHandle(
    const DelliciusQuery::Subquery &subquery)
    : subquery_(subquery) {
  is_redpath_valid_ = false;
  // Step expressions in Subquery's redpath are split into pairs of node name
  // and predicate expression handlers.
  for (absl::string_view step_expression :
       absl::StrSplit(subquery.redpath(), '/', absl::SkipEmpty())) {
    // malformed path error code
    absl::StatusOr<std::pair<std::string, std::string>> node_x_predicate =
        GetNodeAndPredicate(step_expression);
    if (!node_x_predicate.ok()) {
      LOG(ERROR) << node_x_predicate.status();
      return;
    }
    auto [node_name, predicate] = node_x_predicate.value();
    if (predicate == kPredicateSelectAll) {
      steps_in_redpath_.emplace_back(node_name, ApplySelectAllFilter);
    } else {
      LOG(ERROR) << "Unknown predicate " << predicate;
      return;
    }
  }
  // An iterator is configured to traverse the Step expressions in subquery's
  // redpath as redfish requests are dispatched.
  iter_ = steps_in_redpath_.begin();
  is_redpath_valid_ = true;
}

std::optional<std::string> QueryPlanner::SubqueryHandle::NextNodeInRedpath()
    const {
  if (is_redpath_valid_) return iter_->first;
  return std::nullopt;
}

QueryPlanner::PredicateReturnValue QueryPlanner::SubqueryHandle::FilterNodeSet(
    const RedfishVariant &redfish_variant) {
  // Apply the predicate rule on the given RedfishObject.
  bool is_filter_success = iter_->second(redfish_variant);
  if (!is_filter_success) {
    return PredicateReturnValue::kEndByPredicate;
  }
  // If it is the last step expression in the redpath, stop tree traversal.
  if (!steps_in_redpath_.empty() &&
      iter_->first == steps_in_redpath_.back().first) {
    return PredicateReturnValue::kEndOfRedpath;
  }
  ++iter_;
  return PredicateReturnValue::kContinue;
}

void QueryPlanner::QualifyEachSubquery(const RedfishVariant &var,
                                       std::vector<SubqueryHandle> handles,
                                       DelliciusQueryResult &result) {
  std::vector<SubqueryHandle> qualified_subqueries;
  for (auto &subquery_handle : handles) {
    QueryPlanner::PredicateReturnValue status =
        subquery_handle.FilterNodeSet(var);
    // Prepare subquery response if the end of redpath is reached.
    if (status == QueryPlanner::PredicateReturnValue::kEndOfRedpath) {
      DelliciusQuery::Subquery subquery = subquery_handle.GetSubquery();
      // Normalize redfish response per the property requirements in subquery.
      absl::StatusOr<SubqueryDataSet> normalized_data =
          normalizer->Normalize(var, subquery);
      if (normalized_data.ok()) {
        // Populate the response data in subquery output for the given id.
        auto *subquery_output = result.mutable_subquery_output_by_id();
        (*subquery_output)[subquery.subquery_id()].mutable_data_set()->Add(
            std::move(normalized_data.value()));
      }
    } else if (status == QueryPlanner::PredicateReturnValue::kContinue) {
      qualified_subqueries.push_back(std::move(subquery_handle));
    }
  }
  if (qualified_subqueries.empty()) return;
  RunRecursive(var, qualified_subqueries, result);
}

void QueryPlanner::Dispatch(const RedfishVariant &var,
                            NodeToSubqueryHandles &resource_x_sq,
                            DelliciusQueryResult &result) {
  // Dispatch Redfish resource requests for each unique resource identified.
  for (auto &[resource_name, handles] : resource_x_sq) {
    auto variant = var[resource_name];
    if (variant.AsObject() == nullptr) continue;
    std::unique_ptr<RedfishIterable> collection = variant.AsIterable();
    // If resource is a collection, qualify for each resource in the collection.
    if (collection != nullptr) {
      for (const auto member : *collection) {
        QualifyEachSubquery(member, handles, result);
      }
    } else {  // Qualify when resource is a singleton.
      QualifyEachSubquery(variant, handles, result);
    }
  }
}

QueryPlanner::NodeToSubqueryHandles QueryPlanner::DeduplicateExpression(
    const RedfishVariant &var, std::vector<SubqueryHandle> &subquery_handles,
    DelliciusQueryResult &result) {
  NodeToSubqueryHandles node_to_subquery;
  for (auto &subquery_handle : subquery_handles) {
    // Pair resource name and those subqueries that have this resource as next
    // element in their respective redpaths
    auto node_name = subquery_handle.NextNodeInRedpath();
    if (node_name != std::nullopt) {
      node_to_subquery[*node_name].push_back(subquery_handle);
    }
  }
  return node_to_subquery;
}

void QueryPlanner::RunRecursive(const RedfishVariant &variant,
                                std::vector<SubqueryHandle> &subquery_handles,
                                DelliciusQueryResult &result) {
  NodeToSubqueryHandles node_to_subquery =
      DeduplicateExpression(variant, subquery_handles, result);
  if (node_to_subquery.empty()) return;
  Dispatch(variant, node_to_subquery, result);
}

void QueryPlanner::Run(const RedfishVariant &variant, const Clock &clock,
                       DelliciusQueryResult &result) {
  auto timestamp = AbslTimeToProtoTime(clock.Now());
  if (timestamp.ok()) {
    *result.mutable_start_timestamp() = *std::move(timestamp);
  }
  result.add_query_ids(plan_id_);
  RunRecursive(variant, subquery_handles_, result);
  timestamp = AbslTimeToProtoTime(clock.Now());
  if (timestamp.ok()) {
    *result.mutable_end_timestamp() = *std::move(timestamp);
  }
}

QueryPlanner::QueryPlanner(const DelliciusQuery &query, Normalizer *normalizer)
    : normalizer(normalizer), plan_id_(query.query_id()) {
  // Create subquery handles aka subquery plans.
  for (const auto &subquery : query.subquery()) {
    auto subquery_handle = SubqueryHandle(subquery);
    if (subquery_handle.NextNodeInRedpath() == std::nullopt) continue;
    subquery_handles_.push_back(std::move(subquery_handle));
  }
}

}  // namespace ecclesia
