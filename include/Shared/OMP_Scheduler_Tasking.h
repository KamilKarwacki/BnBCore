#pragma once

#include "OMP_Scheduler.h"


template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
class OMP_Scheduler_Tasking: public OMP_Scheduler<Problem_Consts, Subproblem_Params, Domain_Type>
{
public:
    Subproblem_Params Execute(
            const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type>& Problem_Def,
            const Problem_Consts& prob,
            const Goal goal,
            Domain_Type BestBound) override;

private:
    void DoTask(
            const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type>& Problem_Def,
            const Problem_Consts& prob,
            const Goal goal,
            Subproblem_Params& BestSubproblem,
            Domain_Type& BestBound,
            const Subproblem_Params& subpr);

    int TasksWorkedOn = 0;
};

template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
void OMP_Scheduler_Tasking<Problem_Consts, Subproblem_Params, Domain_Type>::DoTask(
        const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type>& Problem_Def,
        const Problem_Consts& prob,
        const Goal goal,
        Subproblem_Params& BestSubproblem,
        Domain_Type& BestBound,
        const Subproblem_Params& subpr)
{
    #pragma atomic
    TasksWorkedOn++;
    //ignore if its bound is worse than already known best sol.
    auto [LowerBound, UpperBound] = Problem_Def.GetEstimateForBounds(prob, subpr);
    if (((bool) goal && LowerBound < BestBound)
        || (!(bool) goal && LowerBound > BestBound)) {
        return;
    }

    // try to make the bound better
    auto Feasibility = Problem_Def.IsFeasible(prob, subpr);
    Domain_Type CandidateBound;

    if (Feasibility == BnB::FEASIBILITY::FULL) {
        CandidateBound = Problem_Def.GetContainedUpperBound(prob, subpr);
        #pragma omp critical
        {
            if (((bool) goal && CandidateBound >= BestBound)
                || (!(bool) goal && CandidateBound <= BestBound)) {
                BestBound = CandidateBound;
                BestSubproblem = subpr;
            }
        }
    }else if(Feasibility == BnB::FEASIBILITY::PARTIAL){
        CandidateBound = UpperBound;
    }
    else if (Feasibility == BnB::FEASIBILITY::NONE)
        return;

    //printProc("BestBound= " << BestBound << " lowerBound= " << LowerBound);
    double diff = std::abs(CandidateBound - LowerBound);
    if(diff > this->eps)
    {
        std::vector<Subproblem_Params> result = Problem_Def.SplitSolution(prob, subpr);
        //for(auto it = result.rbegin(); it != result.rend(); ++it)
        for(const auto& r : result)
        {
            #pragma omp task firstprivate(r) shared(BestSubproblem, BestBound) //priority(int(1/TaskWorkedOn*1000))
            {
                DoTask(Problem_Def, prob, goal, BestSubproblem, BestBound, r);
            }
        }
    }
}




template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params OMP_Scheduler_Tasking<Problem_Consts, Subproblem_Params, Domain_Type>::Execute(
                                  const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type>& Problem_Def,
                                  const Problem_Consts& prob,
                                  const Goal goal,
                                  Domain_Type BestBound)
{
    Subproblem_Params BestSubproblem;
    Subproblem_Params initial = Problem_Def.GetInitialSubproblem(prob);

#pragma omp parallel shared(BestBound, BestSubproblem)
    {
#pragma omp single
        {
#pragma omp task
            {
                DoTask(Problem_Def, prob, goal, BestSubproblem, BestBound, initial);
            }
        }
#pragma omp taskwait
        printProc("I have worked on " << TasksWorkedOn << " Tasks");
    }
    Problem_Def.PrintSolution(BestSubproblem);
    return BestSubproblem;
}




