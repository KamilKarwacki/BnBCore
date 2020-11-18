#pragma once
#include <omp.h>
#include "BnB_Solver.h" // refactor into types and Solver_MPI

template<typename Prob_Consts, typename Subproblem_Params>
class Solver_omp
{
private:
    enum class Goal : bool {
        MAX = true,
        MIN = false
    };

    Goal goal;
    double BestSolution; // should this hold different types
    size_t numThreads = 1;

    void Solve(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def, const Prob_Consts& prob);

    void PushTask(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
            const Prob_Consts& prob,
            Subproblem_Params& params);
public:
    void setNumThreads(size_t num) {numThreads = num;};
    void Maximize(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def, const Prob_Consts& prob)
    {
        goal = Goal::MAX;
        BestSolution = std::numeric_limits<double>::lowest();
        Solve(Problem_Def, prob);
    }

    void Minimize(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def, const Prob_Consts& prob)
    {
        goal = Goal::MIN;
        BestSolution = std::numeric_limits<double>::max();
        Solve(Problem_Def, prob);
    }
};



template<typename Prob_Consts, typename Subproblem_Params>
void Solver_omp<Prob_Consts, Subproblem_Params>::Solve(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
                                                   const Prob_Consts& prob) {
/*
#pragma omp parallel shared(BestSolution) num_threads(numThreads){
#pragma omp single{
    Subproblem_Params initial = Problem_Def.GetInitialSubproblem(prob);
        #pragma omp task
        {
            PushTask(Problem_Def, prob, initial);
        };
        #pragma omp taskwait
    }
};
 */
    //Subproblem_Params initial = Problem_Def.GetInitialSubproblem(prob);
    //PushTask(Problem_Def, prob, initial);
}


template<typename Prob_Consts, typename Subproblem_Params>
void Solver_omp<Prob_Consts, Subproblem_Params>::PushTask(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
                                                       const Prob_Consts& prob, Subproblem_Params& subpr) {
{
    double newBound = std::get<double>(Problem_Def.GetBound(prob, subpr));
    if(((bool)goal && (newBound < BestSolution))
       || (!(bool)goal && (newBound > BestSolution))){
        //#pragma omp atomic
        {
            BestSolution = newBound;
        };
    }

    if(!Problem_Def.IsFeasible(prob, subpr))
    {
        std::vector<Subproblem_Params> result = Problem_Def.SplitSolution(prob, subpr);
        for(const auto& r : result)
        {
            #pragma omp task
            {
                PushTask(Problem_Def, prob, r);
            }
        }
    }
}




