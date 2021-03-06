// Copyright 2010-2014 Google
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
// This model implements a simple jobshop problem.
//
// A jobshop is a standard scheduling problem where you must schedule a
// set of jobs on a set of machines.  Each job is a sequence of tasks
// (a task can only start when the preceding task finished), each of
// which occupies a single specific machine during a specific
// duration. Therefore, a job is simply given by a sequence of pairs
// (machine id, duration).

// The objective is to minimize the 'makespan', which is the duration
// between the start of the first task (across all machines) and the
// completion of the last task (across all machines).
//
// This will be modelled by sets of intervals variables (see class
// IntervalVar in constraint_solver/constraint_solver.h), one per
// task, representing the [start_time, end_time] of the task.  Tasks
// in the same job will be linked by precedence constraints.  Tasks on
// the same machine will be covered by Sequence constraints.
//
// Search will be implemented as local search on the sequence variables.

#include "cpp/jobshop_ls.h"
#include <cstdio>
#include <cstdlib>

#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/bitmap.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "cpp/jobshop.h"

DEFINE_string(
    data_file, "",
    "Required: input file description the scheduling problem to solve, "
    "in our jssp format:\n"
    "  - the first line is \"instance <instance name>\"\n"
    "  - the second line is \"<number of jobs> <number of machines>\"\n"
    "  - then one line per job, with a single space-separated "
    "list of \"<machine index> <duration>\"\n"
    "note: jobs with one task are not supported");
DEFINE_int32(time_limit_in_ms, 60000, "Time limit in ms, 0 means no limit.");
DEFINE_int32(shuffle_length, 4, "Length of sub-sequences to shuffle LS.");
DEFINE_int32(sub_sequence_length, 4,
             "Length of sub-sequences to relax in LNS.");
DEFINE_int32(lns_seed, 1, "Seed of the LNS random search");
DEFINE_int32(lns_limit, 30,
             "Limit the size of the search tree in a LNS fragment");

namespace operations_research {
// ----- Model and Solve -----

void JobshopLs(const JobShopData& data) {
  Solver solver("jobshop");
  const int machine_count = data.machine_count();
  const int job_count = data.job_count();
  const int horizon = data.horizon();

  // ----- Creates all Intervals and vars -----

  // Stores all tasks attached interval variables per job.
  std::vector<std::vector<IntervalVar*> > jobs_to_tasks(job_count);
  // machines_to_tasks stores the same interval variables as above, but
  // grouped my machines instead of grouped by jobs.
  std::vector<std::vector<IntervalVar*> > machines_to_tasks(machine_count);

  // Creates all individual interval variables.
  for (int job_id = 0; job_id < job_count; ++job_id) {
    const std::vector<JobShopData::Task>& tasks = data.TasksOfJob(job_id);
    for (int task_index = 0; task_index < tasks.size(); ++task_index) {
      const JobShopData::Task& task = tasks[task_index];
      CHECK_EQ(job_id, task.job_id);
      const std::string name =
          StringPrintf("J%dM%dI%dD%d", task.job_id, task.machine_id, task_index,
                       task.duration);
      IntervalVar* const one_task = solver.MakeFixedDurationIntervalVar(
          0, horizon, task.duration, false, name);
      jobs_to_tasks[task.job_id].push_back(one_task);
      machines_to_tasks[task.machine_id].push_back(one_task);
    }
  }

  // ----- Creates model -----

  // Creates precedences inside jobs.
  for (int job_id = 0; job_id < job_count; ++job_id) {
    const int task_count = jobs_to_tasks[job_id].size();
    for (int task_index = 0; task_index < task_count - 1; ++task_index) {
      IntervalVar* const t1 = jobs_to_tasks[job_id][task_index];
      IntervalVar* const t2 = jobs_to_tasks[job_id][task_index + 1];
      Constraint* const prec =
          solver.MakeIntervalVarRelation(t2, Solver::STARTS_AFTER_END, t1);
      solver.AddConstraint(prec);
    }
  }

  // Adds disjunctive constraints on unary resources, and creates
  // sequence variables. A sequence variable is a dedicated variable
  // whose job is to sequence interval variables.
  std::vector<SequenceVar*> all_sequences;
  for (int machine_id = 0; machine_id < machine_count; ++machine_id) {
    const std::string name = StringPrintf("Machine_%d", machine_id);
    DisjunctiveConstraint* const ct =
        solver.MakeDisjunctiveConstraint(machines_to_tasks[machine_id], name);
    solver.AddConstraint(ct);
    all_sequences.push_back(ct->MakeSequenceVar());
  }

  // Creates array of end_times of jobs.
  std::vector<IntVar*> all_ends;
  for (int job_id = 0; job_id < job_count; ++job_id) {
    const int task_count = jobs_to_tasks[job_id].size();
    IntervalVar* const task = jobs_to_tasks[job_id][task_count - 1];
    all_ends.push_back(task->EndExpr()->Var());
  }

  // Objective: minimize the makespan (maximum end times of all tasks)
  // of the problem.
  IntVar* const objective_var = solver.MakeMax(all_ends)->Var();

  // ----- Search monitors and decision builder -----

  // This decision builder will rank all tasks on all machines.
  DecisionBuilder* const sequence_phase =
      solver.MakePhase(all_sequences, Solver::SEQUENCE_DEFAULT);

  // After the ranking of tasks, the schedule is still loose and any
  // task can be postponed at will. But, because the problem is now a PERT
  // (http://en.wikipedia.org/wiki/Program_Evaluation_and_Review_Technique),
  // we can schedule each task at its earliest start time. This is
  // conveniently done by fixing the objective variable to its
  // minimum value.
  DecisionBuilder* const obj_phase = solver.MakePhase(
      objective_var, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);

  Assignment* const first_solution = solver.MakeAssignment();
  first_solution->Add(all_sequences);
  first_solution->AddObjective(objective_var);
  // Store the first solution in the 'solution' object.
  DecisionBuilder* const store_db = solver.MakeStoreAssignment(first_solution);

  // The main decision builder (ranks all tasks, then fixes the
  // objective_variable).
  DecisionBuilder* const first_solution_phase =
      solver.Compose(sequence_phase, obj_phase, store_db);

  LOG(INFO) << "Looking for the first solution and improving with local search";
  std::vector<LocalSearchOperator*> operators;
  LOG(INFO) << "  - use swap operator";
  LocalSearchOperator* const swap_operator =
      solver.RevAlloc(new SwapIntervals(all_sequences));
  operators.push_back(swap_operator);
  LOG(INFO) << "  - use shuffle operator with a max length of "
            << FLAGS_shuffle_length;
  LocalSearchOperator* const shuffle_operator = solver.RevAlloc(
      new ShuffleIntervals(all_sequences, FLAGS_shuffle_length));
  operators.push_back(shuffle_operator);
  LOG(INFO) << "  - use free sub sequences of length "
            << FLAGS_sub_sequence_length << " lns operator";
  LocalSearchOperator* const lns_operator = solver.RevAlloc(new SequenceLns(
      all_sequences, FLAGS_lns_seed, FLAGS_sub_sequence_length));
  operators.push_back(lns_operator);

  // Creates the local search decision builder.
  LocalSearchOperator* const concat =
      solver.ConcatenateOperators(operators, true);

  SearchLimit* const ls_limit =
      solver.MakeLimit(kint64max, FLAGS_lns_limit, kint64max, kint64max);
  DecisionBuilder* const random_sequence_phase =
      solver.MakePhase(all_sequences, Solver::CHOOSE_RANDOM_RANK_FORWARD);
  DecisionBuilder* const ls_db = solver.MakeSolveOnce(
      solver.Compose(random_sequence_phase, obj_phase), ls_limit);

  LocalSearchPhaseParameters* const parameters =
      solver.MakeLocalSearchPhaseParameters(concat, ls_db);
  DecisionBuilder* const final_db = solver.MakeLocalSearchPhase(
      all_sequences, first_solution_phase, parameters);

  OptimizeVar* const objective_monitor = solver.MakeMinimize(objective_var, 1);

  // Search log.
  const int kLogFrequency = 1000000;
  SearchMonitor* const search_log =
      solver.MakeSearchLog(kLogFrequency, objective_monitor);

  SearchLimit* const limit = FLAGS_time_limit_in_ms > 0
                                 ? solver.MakeTimeLimit(FLAGS_time_limit_in_ms)
                                 : NULL;

  // Search.
  solver.Solve(final_db, search_log, objective_monitor, limit);
}
}  // namespace operations_research

static const char kUsage[] =
    "Usage: see flags.\nThis program runs a simple job shop optimization "
    "output besides the debug LOGs of the solver.";

int main(int argc, char** argv) {
  google::SetUsageMessage(kUsage);
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_data_file.empty()) {
    LOG(FATAL) << "Please supply a data file with --data_file=";
  }
  operations_research::JobShopData data;
  data.Load(FLAGS_data_file);
  operations_research::JobshopLs(data);
  return 0;
}
