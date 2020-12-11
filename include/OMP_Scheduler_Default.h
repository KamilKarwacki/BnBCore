#pragma once

#include "OMP_Scheduler.h"


template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
class OMP_Scheduler_Default : public OMP_Scheduler<Problem_Consts, Subproblem_Params, Domain_Type>
{
public:
    Subproblem_Params Execute(
            const Problem_Definition<Problem_Consts, Subproblem_Params>& Problem_Def,
            const Problem_Consts& prob,
            const Goal goal,
            Domain_Type BestBound) override;

    void DoTask(
            const Problem_Definition<Problem_Consts, Subproblem_Params>& Problem_Def,
            const Problem_Consts& prob,
            const Goal goal,
            Subproblem_Params& BestSubproblem,
            Domain_Type& BestBound,
            const Subproblem_Params& subpr);
};



template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
void OMP_Scheduler_Default<Problem_Consts, Subproblem_Params, Domain_Type>::DoTask(
        const Problem_Definition<Problem_Consts, Subproblem_Params>& Problem_Def,
        const Problem_Consts& prob,
        const Goal goal,
        Subproblem_Params& BestSubproblem,
        Domain_Type& BestBound,
        const Subproblem_Params& subpr)
{
    // try to discard
    if(Problem_Def.Discard(prob, subpr, BestBound))
        return;

    // try to set new bound
    Domain_Type newBound = std::get<Domain_Type>(Problem_Def.GetBound(prob, subpr));
    if(((bool)goal && (newBound >= BestBound))
    || (!(bool)goal && (newBound <= BestBound))){
#pragma omp critical
        {
            BestBound = newBound;
            BestSubproblem = subpr;
        }
    }

    // if not small/far enough, split further
    if(!Problem_Def.IsFeasible(prob, subpr))
    {
        std::vector<Subproblem_Params> result = Problem_Def.SplitSolution(prob, subpr);
        for(const auto& r : result)
        {
            //int prio = std::get<0>(r);
            #pragma omp task firstprivate(r) shared(BestSubproblem, BestBound)
            {
                DoTask(Problem_Def, prob, goal, BestSubproblem, BestBound, r);
            }
        }
    }
}




template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params OMP_Scheduler_Default<Problem_Consts, Subproblem_Params, Domain_Type>::Execute(
                                  const Problem_Definition<Problem_Consts, Subproblem_Params>& Problem_Def,
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
    }
    Problem_Def.PrintSolution(BestSubproblem);
    std::cout << BestBound << std::endl;
    return BestSubproblem;
}




