#pragma once

#include "Base.h"

// strategy pattern that holds the actual MPI algorithm to schedule the work

template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
class MPI_Scheduler
{
public:
    // TODO has to be private!!!!!!!! we need some friend mechanism to let the solver use it
    virtual Subproblem_Params Execute(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
                         const Prob_Consts& prob,
                         const MPI_Message_Encoder<Subproblem_Params>& encoder,
                         const Goal goal,
                         const Domain_Type WorstBound) = 0;

    void CommFrequency(int num){Communication_Frequency = num;}
    void Threads(size_t num) {OpenMPThreads = num;}
    void Eps(Domain_Type e) {eps = e;}

protected:
    int Communication_Frequency = 1;
    Domain_Type eps;

    // only for Hybrid
    int OpenMPThreads = 1;
};


