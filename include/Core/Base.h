#pragma once

// includes used throughout the project
#include <tuple>
#include <functional>
#include <iostream>
#include <sstream>
#include <any>
#include <variant>
#include <fstream>
#include <queue>
#include <memory>
#include <cstring>
#include <type_traits>

#include "mpi.h"
#include "omp.h"

#define    OMP_LOGGING 1
#define    MPI_LOGGING 0
#define HYBRID_LOGGING 0

#if MPI_LOGGING
#define printProc(x) {int __id; \
                     MPI_Comm_rank(MPI_COMM_WORLD, &__id); \
                     std::cout << "proc "<< __id <<": " << x << std::endl;}
#endif
#if OMP_LOGGING
#define printProc(x)  { std::cout << "proc (omp) "<< omp_get_thread_num() <<": " << x << std::endl;}
#endif
#if HYBRID_LOGGING
#define printProc(x) {int __id; \
                     MPI_Comm_rank(MPI_COMM_WORLD, &__id); \
                     std::cout << "proc "<< __id << "("<<omp_get_thread_num() << ")"  << ": " << x << std::endl;}
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

    enum class FEASIBILITY{Full, PARTIAL, NONE};
}


struct interval_MPI
{
    double lower, upper;
};

// holds the functions that will perform actions on the subproblems
template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
class Problem_Definition
{
public:
    // get the initial sub-problem
    std::function<Subproblem_Params               (const Problem_Consts&)> GetInitialSubproblem = nullptr;
    // takes a sub-problem and splits it up further
    std::function<std::vector<Subproblem_Params>  (const Problem_Consts&, const Subproblem_Params&)> SplitSolution = nullptr;
    // says whether a solution lies in a feasible domain
    std::function<BnB::FEASIBILITY                (const Problem_Consts&, const Subproblem_Params&)> IsFeasible = nullptr;
    // for a minimum, get a lower bound for the lower bound
    std::function<std::tuple<Domain_Type, Domain_Type> (const Problem_Consts&, const Subproblem_Params&)> GetEstimateForBounds = nullptr;
    // for a minimum this should calculate the upper bound for the minimum, this is an achievable value for the minimum
    std::function<Domain_Type(const Problem_Consts&, const Subproblem_Params&)> GetContainedUpperBound = nullptr;
    // trivial function to display the solution
    std::function<void                            (const Subproblem_Params& params)> PrintSolution = nullptr;


    /// EXPERIMENTAL maybe new class that user can implement
    std::function<void(const Subproblem_Params&, std::vector<interval_MPI>&)> PackData = nullptr;
    std::function<void(std::deque<Subproblem_Params>&, const std::vector<interval_MPI>&, int)> UnpackData = nullptr;
};


enum class TraversalMode {DFS, BFS, AUTOMATIC};

template<typename Subproblem_Params>
Subproblem_Params GetNextSubproblem(std::deque<Subproblem_Params>& Container, TraversalMode Mode){
    Subproblem_Params NextSubproblem;
    switch(Mode){
        case TraversalMode::DFS:
            NextSubproblem = Container.back(); Container.pop_back();
			break;
        case TraversalMode::BFS:
            NextSubproblem = Container.front(); Container.pop_front();
			break;
        case TraversalMode::AUTOMATIC:
            if(Container.size() > 20000){
                NextSubproblem = Container.back(); Container.pop_back();
            }else{
                NextSubproblem = Container.front(); Container.pop_front();
            }
    }
    return NextSubproblem;
}



enum class Goal : bool {
    MAX = true,
    MIN = false
};
