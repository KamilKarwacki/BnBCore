#pragma once

#include "Serial_Scheduler.h"

namespace BnB {
    template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
    class Serial_Scheduler_Default : public Serial_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type> {
    public:
        Subproblem_Params Execute(const Problem_Definition <Prob_Consts, Subproblem_Params, Domain_Type> &Problem_Def,
                                  const Prob_Consts &prob,
                                  const Goal goal,
                                  const Domain_Type WorstBound) override;
    };

    template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
    Subproblem_Params Serial_Scheduler_Default<Problem_Consts, Subproblem_Params, Domain_Type>::Execute(
            const Problem_Definition <Problem_Consts, Subproblem_Params, Domain_Type> &Problem_Def,
            const Problem_Consts &prob,
            const Goal goal,
            const Domain_Type WorstBound) {

        std::deque<Subproblem_Params> TaskQueue;
        TaskQueue.push_back(Problem_Def.GetInitialSubproblem(prob));

        Domain_Type BestBound = WorstBound;
        Subproblem_Params BestSubproblem;
        int NumProblemsSolved = 0;
        int ProblemsEliminated = 0;

        while (!TaskQueue.empty()) {
            NumProblemsSolved++;

            Subproblem_Params sol = GetNextSubproblem(TaskQueue, this->mode);

            //ignore if its bound is worse than already known best sol.
            auto[LowerBound, UpperBound] = Problem_Def.GetEstimateForBounds(prob, sol);
            if (((bool) goal && LowerBound < BestBound)
                || (!(bool) goal && LowerBound > BestBound)) {
                ProblemsEliminated++;
                continue;
            }

            // try to make the bound better only if the solution lies in a feasible domain
            bool IsPotentialBestSolution = false;
            auto Feasibility = Problem_Def.IsFeasible(prob, sol);
            Domain_Type CandidateBound;
            if (Feasibility == BnB::FEASIBILITY::Full) {
                CandidateBound = (Problem_Def.GetContainedUpperBound(prob, sol));
                if (((bool) goal && CandidateBound >= BestBound)
                    || (!(bool) goal && CandidateBound <= BestBound)) {
                    BestBound = CandidateBound;
                    IsPotentialBestSolution = true;
                }
            } else if (Feasibility == BnB::FEASIBILITY::PARTIAL) {
                // use our backup for the CandidateBound
                CandidateBound = UpperBound;
            } else if (Feasibility == BnB::FEASIBILITY::NONE) // basically discard again
                continue;

            std::vector<Subproblem_Params> v;
            if (std::abs(CandidateBound - LowerBound) > this->EPS) { // epsilon criterion for convergence
                v = Problem_Def.SplitSolution(prob, sol);
                std::move(std::begin(v), std::end(v), std::back_inserter(TaskQueue));
            }
            if (IsPotentialBestSolution)
                BestSubproblem = sol;
        }

        std::cout << "I have solved " << NumProblemsSolved << " problems" << std::endl;
        std::cout << "and eliminated " << ProblemsEliminated << " problems" << std::endl;
        Problem_Def.PrintSolution(BestSubproblem);
        return BestSubproblem;
    }
}
