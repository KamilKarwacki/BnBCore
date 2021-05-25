#pragma once

#include "Base.h"

namespace BnB{
    // strategy pattern that holds the actual MPI algorithm to schedule the work
    template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
    class OMP_Scheduler
    {
    public:
        virtual Subproblem_Params Execute(
                const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type>& Problem_Def,
                const Problem_Consts& prob,
                const Goal goal,
                const Domain_Type WorstBound) = 0;

        OMP_Scheduler<Problem_Consts, Subproblem_Params, Domain_Type>* Eps(Domain_Type e){eps = e;return this;}
        OMP_Scheduler<Problem_Consts, Subproblem_Params, Domain_Type>* Traversal(TraversalMode mode_){mode = mode_; return this;}
    protected:
        Domain_Type eps;
        TraversalMode mode = TraversalMode::DFS;
    };
}

