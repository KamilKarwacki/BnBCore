#pragma once

#include <MPI_Scheduler.h>
#include "OMP_Scheduler.h"


template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
class OMP_Scheduler_Queue : public OMP_Scheduler<Problem_Consts, Subproblem_Params, Domain_Type> {
public:
    Subproblem_Params Execute(
            const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type> &Problem_Def,
            const Problem_Consts &prob,
            const Goal goal,
            Domain_Type WorstBound) override;

private:
    void ThreadWork(
            const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type> &Problem_Def,
            const Problem_Consts &prob,
            const Subproblem_Params &task,
            Subproblem_Params &CurrentBestProblem,
            Domain_Type &BestBound,
            std::deque<Subproblem_Params>& GlobalTaskQueue,
            const Goal goal);
    int TasksWorkedOn = 0;
};


template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params OMP_Scheduler_Queue<Problem_Consts, Subproblem_Params, Domain_Type>::Execute(
        const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type> &Problem_Def,
        const Problem_Consts &prob,
        const Goal goal,
        Domain_Type WorstBound) {
    Subproblem_Params BestSubproblem;
    Domain_Type CurrentBestBound = WorstBound;
    std::deque<Subproblem_Params> GlobalTaskQueue;
    GlobalTaskQueue.push_back(Problem_Def.GetInitialSubproblem(prob));

    while (!GlobalTaskQueue.empty()) {
        int NumTasks = GlobalTaskQueue.size();
        int NumThreads = omp_get_max_threads();
        int NumToSpawn = std::min(NumTasks, NumThreads);
        TasksWorkedOn += NumToSpawn;

        std::vector<Subproblem_Params> WorkPack;

        std::move(GlobalTaskQueue.begin(), std::next(GlobalTaskQueue.begin(), NumToSpawn),
                  std::back_inserter(WorkPack));
        GlobalTaskQueue.erase(GlobalTaskQueue.begin(), std::next(GlobalTaskQueue.begin(), NumToSpawn));

#pragma omp parallel shared(CurrentBestBound, BestSubproblem) firstprivate(TasksWorkedOn) num_threads(NumToSpawn)
        {
            ThreadWork(Problem_Def, prob, WorkPack[omp_get_thread_num()], BestSubproblem, CurrentBestBound, GlobalTaskQueue, goal);
        }
    }
    std::cout << "There were " << TasksWorkedOn << " tasks generated by solving the problem" << std::endl;


    Problem_Def.PrintSolution(BestSubproblem);
    return BestSubproblem;
}


template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
void OMP_Scheduler_Queue<Problem_Consts, Subproblem_Params, Domain_Type>::ThreadWork(
        const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type> &Problem_Def,
        const Problem_Consts &prob,
        const Subproblem_Params &task,
        Subproblem_Params &CurrentBestProblem,
        Domain_Type &CurrentBestBound,
        std::deque<Subproblem_Params>& GlobalTaskQueue,
        const Goal goal) {
    //ignore if its bound is worse than already known best sol.
    auto[LowerBound, UpperBound] = Problem_Def.GetEstimateForBounds(prob, task);
    if (((bool) goal && LowerBound < CurrentBestBound)
        || (!(bool) goal && LowerBound > CurrentBestBound)) {
        return;
    }

    // try to make the bound better
    auto Feasibility = Problem_Def.IsFeasible(prob, task);
    Domain_Type CandidateBound;
    if (Feasibility == BnB::FEASIBILITY::FULL) {
        CandidateBound = Problem_Def.GetContainedUpperBound(prob, task);
#pragma omp critical
        {
            if (((bool) goal && CandidateBound >= CurrentBestBound)
                || (!(bool) goal && CandidateBound <= CurrentBestBound)) {
                CurrentBestBound = CandidateBound;
                CurrentBestProblem= task;
            }
        }
    } else if (Feasibility == BnB::FEASIBILITY::PARTIAL) {
        CandidateBound = UpperBound;
    } else if (Feasibility == BnB::FEASIBILITY::NONE) {
        return;
    }

    // check if we cant divide further
    std::vector<Subproblem_Params> v;
    if (std::abs(CandidateBound - LowerBound) > this->eps) {
        v = Problem_Def.SplitSolution(prob, task);
        for (const auto &el : v) {
#pragma omp critical (queuelock)
            {
                if(this->mode == TraversalMode::BFS)
                    GlobalTaskQueue.push_back(el);
                else if(this->mode ==TraversalMode::DFS)
                    GlobalTaskQueue.push_front(el);
            }
        }
    }
}



