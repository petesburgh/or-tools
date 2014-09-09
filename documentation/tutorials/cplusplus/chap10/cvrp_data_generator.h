// Copyright 2011-2014 Google
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//
//  Very basic CVRPDataGenerator class.
//
//  The demands are created by constructing a feasible solution.
//  The capacity if FULLY used on each route. i.e. the sum of all demands
//  along a route is equal to FLAGS_capacity.
//  A node can have a zero capacity. If you want to force each node to
//  have a capacity of at least 1, set FLAGS_allow_zero_capacity to false.
//
//  We don't consider min and max capacities.

#ifndef OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_CVRP_GENERATOR_H
#define OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_CVRP_GENERATOR_H

#include "base/commandlineflags.h"
#include "routing_common/routing_data_generator.h"
#include "routing_common/tsplib.h"

DEFINE_int32(number_vehicles, 2, "Number of vehicles.");
DEFINE_int64(capacity, 100, "Capacity of all vehicles.");

DEFINE_bool(allow_zero_capacity, true, "Allow node with zero capacity?");

namespace operations_research {

class CVRPDataGenerator : public RoutingDataGenerator {
public:
  CVRPDataGenerator(std::string instance_name, int32 size, std::string problem_name = "CVRP") : RoutingDataGenerator(problem_name, instance_name, size),
  comment_("Generated by VRPDataGenerator."), capacity_corrector_(FLAGS_allow_zero_capacity ? 0 : 1), capacity_(FLAGS_capacity) {
    CreateFeasibleSolution();
  }

  std::string Comment() const {
    return comment_;
  }

  void SetComment(const std::string comment) {
    comment_ = comment;
  }

  RoutingModel::NodeIndex Depot() const {
      return depot_;
  }

  void SetDepot(const RoutingModel::NodeIndex d) {
    depot_ = d;
  }

int64 Capacity() const {
  return capacity_;
}

  int64 Demand(const RoutingModel::NodeIndex i) const {
    return demands_[i.value()];
  }

  TSPLIB_NODE_COORD_TYPE_TYPES_enum NodeCoordinateType() const {
    return TWOD_COORDS;
  }

  TSPLIB_DISPLAY_DATA_TYPE_TYPES_enum DisplayDataType() const {
    return COORD_DISPLAY;
  }

  bool HasDimensionTwo() const {
    return true;
  }
private:
  void CreateFeasibleSolution() {
    //  first: shuffle nodes
    std::vector<RoutingModel::NodeIndex> nodes(Size());
    for (RoutingModel::NodeIndex i(0); i < Size(); ++i) {
      nodes[i.value()] = i;
    }
    //  0 must be the depot
    std::random_shuffle(nodes.begin() + 1, nodes.end(), randomizer_);

    //  second: distribute nodes into routes of random length
    sol_.resize(Size());
    int number_of_nodes = 0;
    int total_number_of_used_nodes = 0;
    VLOG(1) << "Optimal solution used:\n";
    for (int i = 0; i < FLAGS_number_vehicles; ++i) {
      if (i == FLAGS_number_vehicles - 1) { // add the rest
        number_of_nodes = Size() - 1 - total_number_of_used_nodes;
      } else { // add random number of nodes
        number_of_nodes = randomizer_.Uniform(Size() - 1 - total_number_of_used_nodes - FLAGS_number_vehicles + i) + 1;
      }
      sol_[i].resize(number_of_nodes);
      VLOG(1) << "Vehicle: " << i;
      for (int j = 0; j < number_of_nodes; ++j) {
        sol_[i][j] = nodes[total_number_of_used_nodes + j + 1];
        VLOG(1) << sol_[i][j] << " - "; 
      }
      VLOG(1) << std::endl;
      total_number_of_used_nodes += number_of_nodes;
    }

    //  third: allocate the capacity for each route
    demands_.resize(Size());
    demands_[0] = 0;
    int64 total_capacity_used = 0;
    int64 total_nodes_with_capacity = 0;
    int64 capacity = 0;
    for (int i = 0; i < FLAGS_number_vehicles; ++i) {

      number_of_nodes = sol_[i].size();
      total_capacity_used = 0;
      for (int j = 0; j < number_of_nodes; ++j) {
        if (j == number_of_nodes - 1) { // add the rest
          ++total_nodes_with_capacity;
          capacity = FLAGS_capacity - total_capacity_used;
        } else { // add random capacity
          ++total_nodes_with_capacity;
          capacity  = randomizer_.Uniform(FLAGS_capacity - total_capacity_used - number_of_nodes + j - capacity_corrector_
                                      - capacity_corrector_ * (Size() - total_nodes_with_capacity)) + capacity_corrector_;
        }
        demands_[sol_[i][j].value()] = capacity;
        total_capacity_used += capacity;
      }
    }
  }
  
  std::string comment_;
  RoutingModel::NodeIndex depot_;
  std::vector<std::vector<RoutingModel::NodeIndex> > sol_;
  std::vector<int64> demands_;
  const int64 capacity_;
  const int capacity_corrector_;
};

}  //  namespace operations_research

#endif  // OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_CVRP_GENERATOR_H