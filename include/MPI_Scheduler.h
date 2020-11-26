#pragma once

#include "Base.h"

// strategy pattern that holds the actual MPI algorithm to schedule the work

template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
class MPI_Scheduler
{
public:
    virtual void Execute(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
                         const Prob_Consts& prob,
                         const MPI_Message_Encoder<Subproblem_Params>& encoder,
                         const Goal goal) = 0;
};
