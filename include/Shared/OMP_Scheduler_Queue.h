#pragma once

#include <MPI_Scheduler.h>
#include "OMP_Scheduler.h"

int TasksWorkedOn = 0;

template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
class OMP_Scheduler_Queue: public OMP_Scheduler<Problem_Consts, Subproblem_Params, Domain_Type>
{
public:
    Subproblem_Params Execute(
            const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type>& Problem_Def,
            const Problem_Consts& prob,
            const Goal goal,
            Domain_Type WorstBound) override;
};



template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params OMP_Scheduler_Queue<Problem_Consts, Subproblem_Params, Domain_Type>::Execute(
        const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type>& Problem_Def,
        const Problem_Consts& prob,
        const Goal goal,
        Domain_Type WorstBound)
{
    /*
    Subproblem_Params BestSubproblem;
    Domain_Type CurrentBestBound = WorstBound;
    std::deque<Subproblem_Params> GlobalTaskQueue;
    GlobalTaskQueue.push_back(Problem_Def.GetInitialSubproblem(prob));
    int NumBusyThreads = 0;

        while(!GlobalTaskQueue.empty())
        {

            int NumTasks = GlobalTaskQueue.size();
            int NumThreads = omp_get_max_threads();
            int NumToSpawn = std::min(NumTasks, NumThreads);

            std::vector<Subproblem_Params> WorkPack;

            std::move(GlobalTaskQueue.begin(), std::next(GlobalTaskQueue.begin(), NumToSpawn), std::back_inserter(WorkPack));

            #pragma omp parallel shared(CurrentBestBound, BestSubproblem) firstprivate(TasksWorkedOn)
            {

            Subproblem_Params sol;
            bool isEmpty = false;
#pragma omp critical (queuelock) // deque is not thread safe
            {
                NumBusyThreads++;
                isEmpty = GlobalTaskQueue.empty();
                if (!isEmpty) {
                    if(this->mode == TraversalMode::DFS){
                        sol = GlobalTaskQueue.back(); GlobalTaskQueue.pop_back();
                    }else if( this->mode == TraversalMode::BFS){
                        sol = GlobalTaskQueue.front(); GlobalTaskQueue.pop_front();
                    }
                }
            }

            //ignore if its bound is worse than already known best sol.
            auto [LowerBound, UpperBound] = Problem_Def.GetEstimateForBounds(prob, sol);
            if (((bool) goal && LowerBound < CurrentBestBound)
                || (!(bool) goal && LowerBound > CurrentBestBound)) {
                #pragma omp atomic
                NumBusyThreads--;
                continue;
            }

            // try to make the bound better
            auto Feasibility = Problem_Def.IsFeasible(prob, sol);
            Domain_Type CandidateBound;
            if (Feasibility == BnB::FEASIBILITY::FULL) {
                CandidateBound = Problem_Def.GetContainedUpperBound(prob, sol);
#pragma omp critical
                {
                    if (((bool) goal && CandidateBound >= CurrentBestBound)
                        || (!(bool) goal && CandidateBound <= CurrentBestBound)) {
                        CurrentBestBound = CandidateBound;
                        BestSubproblem = sol;
                    }
                }
            }else if(Feasibility == BnB::FEASIBILITY::PARTIAL){
                CandidateBound = UpperBound;
            }
            else if (Feasibility == BnB::FEASIBILITY::NONE)
            {

                #pragma omp atomic
                NumBusyThreads--;
                continue;
            }


            // check if we cant divide further
            std::vector<Subproblem_Params> v;
            if (std::abs(CandidateBound - LowerBound) > this->eps) {
                v = Problem_Def.SplitSolution(prob, sol);
                for (const auto &el : v) {
#pragma omp critical (queuelock)
                    {
                        GlobalTaskQueue.push_back(el);
                    }
                }
            }
            #pragma omp atomic
            NumBusyThreads--;
        }

        std::cout << "I have worked on " << TaskWorkedOn << " Tasks" << std::endl;
    }
    Problem_Def.PrintSolution(BestSubproblem);
     */
    return {};
}





