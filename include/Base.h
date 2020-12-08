#pragma once

// includes used throughout the project
#include <tuple>
#include <functional>
#include <iostream>
#include <sstream>
#include <any>
#include <variant>
#include <queue>
#include <memory>
#include <cstring>
#include <type_traits>

#include "mpi.h"
#include "omp.h"

#define OMP_LOGGING 1
#define MPI_LOGGING 1

#if __MPI
#define printProc(x) {int __id; \
                     MPI_Comm_rank(MPI_COMM_WORLD, &__id); \
                     std::cout << "proc "<< __id <<": " << x << std::endl;}
#endif
#if OMP_LOGGING
#define printProc(x)  { std::cout << "proc "<< omp_get_thread_num() <<": " << x << std::endl;}
#endif

namespace BnB
{
    // TODO maybe wrap it in a class so that we dont need to use std::get all the time but string names
    // TODO investigate if the efficiency wont drop too hard
    // shall be specified by the user, constans will be initialized once
    template<typename... Args>
    using Problem_Constants = std::tuple<Args...>;
    // params define a subproblem and will be communicated
    template<typename... Args>
    using Subproblem_Parameters = std::tuple<Args...>;
}

// holds the functions that will perform actions on the subproblems
template<typename Problem_Consts, typename Subproblem_Params>
class Problem_Definition
{
public:
    // get the initial sub-problem
    std::function<Subproblem_Params               (const Problem_Consts&)> GetInitialSubproblem = nullptr;
    // takes a sub-problem and splits it up further
    std::function<std::vector<Subproblem_Params>  (const Problem_Consts&, const Subproblem_Params&)> SplitSolution = nullptr;
    // says whether a solution is precise enough
    std::function<bool                            (const Problem_Consts&, const Subproblem_Params&)> IsFeasible = nullptr;
    // the branch and bound algorithm needs a method that can calculate a Bound otherwise we will split and never discard
    std::function<std::variant<int, float, double>(const Problem_Consts&, const Subproblem_Params&)> GetBound = nullptr;
    // takes a sub problem and lower bound and decides if the subproblem needs to be discarded
    std::function<bool                            (const Problem_Consts&, const Subproblem_Params&, const std::variant<int, float, double>)> Discard = nullptr;
    // trivial function to display the solution
    std::function<void                            (const Subproblem_Params& params)> PrintSolution = nullptr;
};



enum class Goal : bool {
    MAX = true,
    MIN = false
};
