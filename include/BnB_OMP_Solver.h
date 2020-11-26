#pragma once
#include <omp.h>
#include "Base.h"

template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
class Solver_omp
{
private:
    // goal will be set by Maximize/Minimize function
    Goal goal;
    // best solution to the problem which is the lower/upper bound
    Domain_Type BestSolution;
    // number of threads that will be used for execution
    size_t numThreads = 1;
    // The subproblem which resulted in
    Subproblem_Params BestSubproblem;

    Subproblem_Params Solve(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def, const Prob_Consts& prob);
    void PushTask(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def, const Prob_Consts& prob, Subproblem_Params& subpr);

public:
    void setNumThreads(size_t num) {numThreads = num;};
    Subproblem_Params Maximize(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def, const Prob_Consts& prob)
    {
        goal = Goal::MAX;
        BestSolution = std::numeric_limits<Domain_Type>::lowest();
        Solve(Problem_Def, prob);
        return BestSubproblem;
    }

    Subproblem_Params Minimize(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def, const Prob_Consts& prob)
    {
        goal = Goal::MIN;
        BestSolution = std::numeric_limits<Domain_Type>::max();
        Solve(Problem_Def, prob);
        return BestSubproblem;
    }
};


template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params Solver_omp<Prob_Consts, Subproblem_Params, Domain_Type>::Solve(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
                                                   const Prob_Consts& prob) {

#pragma omp parallel shared(BestSolution, BestSubproblem) num_threads(numThreads)
    {
#pragma omp single
        {
            Subproblem_Params initial = Problem_Def.GetInitialSubproblem(prob);
            #pragma omp task
            {
                PushTask(Problem_Def, prob, initial);
            }
        }
    #pragma omp taskwait
    }
    Problem_Def.PrintSolution(BestSubproblem);
    return BestSubproblem;
}


template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
void Solver_omp<Prob_Consts, Subproblem_Params, Domain_Type>::PushTask(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
                                                       const Prob_Consts& prob, Subproblem_Params& subpr)
{
    Domain_Type newBound = std::get<Domain_Type>(Problem_Def.GetBound(prob, subpr));
    bool discard = true;
#pragma omp critical
    {
        if(((bool)goal && (newBound >= BestSolution))
            || (!(bool)goal && (newBound <= BestSolution))){
            BestSolution = newBound;
            BestSubproblem = subpr;
            discard = false;
        }
    }
    if(discard)
    {
        for_each(std::get<0>(subpr).begin(), std::get<0>(subpr).end(), [](const auto& el){std::cout << el << " ";});
        std::cout << std::endl;
        return;
    }

    if(!Problem_Def.IsFeasible(prob, subpr))
    {
        std::vector<Subproblem_Params> result = Problem_Def.SplitSolution(prob, subpr);
        for(auto& r : result)
        {
            #pragma omp task
            {
                PushTask(Problem_Def, prob, r);
            }
        }
    }
}

