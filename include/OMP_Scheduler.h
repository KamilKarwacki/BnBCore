#pragma once

#include "Base.h"

// strategy pattern that holds the actual MPI algorithm to schedule the work

template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
class OMP_Scheduler
{
public:
    virtual Subproblem_Params Execute(
            const Problem_Definition<Problem_Consts, Subproblem_Params>& Problem_Def,
            const Problem_Consts& prob,
            const Goal goal,
            const Domain_Type WorstBound) = 0;
};
