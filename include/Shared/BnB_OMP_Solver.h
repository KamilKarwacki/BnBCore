#pragma once
#include <omp.h>
#include "Base.h"
#include "BnB_Solver.h"
#include "OMP_Scheduler_Tasking.h"
#include "OMP_Scheduler_Queue.h"


enum class OMP_Scheduler_Type{TASKING, QUEUE};

template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
class Solver_omp : public BnB_Solver<Problem_Consts, Subproblem_Params, Domain_Type>
{
public:

    void setNumThreads(size_t num) {numThreads = num;};
    void setEps(Domain_Type e) {scheduler->Eps(e);}
    void setTraversal(TraversalMode mode){scheduler->Traversal(mode);}

    // if not called default scheduler will be used
    void SetScheduler(OMP_Scheduler_Type);

    Subproblem_Params Maximize(const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type>& Problem_Def, const Problem_Consts& prob);
    Subproblem_Params Minimize(const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type>& Problem_Def, const Problem_Consts& prob);

private:
    // number of threads that will be used for execution
    size_t numThreads = 1;

    std::unique_ptr<OMP_Scheduler<Problem_Consts, Subproblem_Params, Domain_Type>> scheduler =
            std::make_unique<OMP_Scheduler_Tasking<Problem_Consts, Subproblem_Params, Domain_Type>>();
};


template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params Solver_omp<Problem_Consts, Subproblem_Params, Domain_Type>::Maximize(const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type>& Problem_Def, const Problem_Consts& prob)
{
    Domain_Type WorstSolution = std::numeric_limits<Domain_Type>::lowest();
    omp_set_num_threads(numThreads);
    Subproblem_Params result = scheduler->Execute(Problem_Def, prob, Goal::MAX, WorstSolution);
    return result;
}

template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params Solver_omp<Problem_Consts, Subproblem_Params, Domain_Type>::Minimize(const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type>& Problem_Def, const Problem_Consts& prob)
{
    Domain_Type WorstSolution = std::numeric_limits<Domain_Type>::max();
    omp_set_num_threads(numThreads);
    Subproblem_Params result = scheduler->Execute(Problem_Def, prob, Goal::MIN, WorstSolution);
    return result;
}

template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
void Solver_omp<Problem_Consts, Subproblem_Params, Domain_Type>::SetScheduler(OMP_Scheduler_Type type)
{
    switch(type){
        case OMP_Scheduler_Type::TASKING:
            scheduler = std::make_unique<OMP_Scheduler_Tasking<Problem_Consts, Subproblem_Params, Domain_Type>>();
            break;
        case OMP_Scheduler_Type::QUEUE:
            scheduler = std::make_unique<OMP_Scheduler_Queue<Problem_Consts, Subproblem_Params, Domain_Type>>();
            break;
    }
}


