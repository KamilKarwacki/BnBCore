#pragma once

#include "Base.h"

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

    void Eps(Domain_Type e){eps = e;}
    void Traversal(TraversalMode mode_){mode = mode_;}
protected:
    Domain_Type eps;
    TraversalMode mode = TraversalMode::DFS;
};
