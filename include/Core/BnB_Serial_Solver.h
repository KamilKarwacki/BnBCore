#pragma once

#include "Base.h"
#include "BnB_Solver.h"

// main solver class has to be initiated by the user
// Problem_Consts    -- should be an std::tuple holding constants of the problem
// Subproblem_Params -- should be an std::tuple holding values that describe the problem
// Domain_Type       -- one of the following : double, float, int
template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
class Solver_Serial : public BnB_Solver<Problem_Consts, Subproblem_Params, Domain_Type>
{
public:
    // maximizes a problem defines by the user
    Subproblem_Params Maximize(const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type>&, const Problem_Consts&);
    // minimizes a problem defined by the user
    Subproblem_Params Minimize(const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type>&, const Problem_Consts&);
    void SetEps(double eps){Eps = eps;}
    void SetTraversalMode(TraversalMode m){mode = m;}

private:
    double Eps = 0.001;
    TraversalMode mode = TraversalMode::DFS;

    Subproblem_Params Solve(
            const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type>& Problem_Def,
            const Problem_Consts& prob,
            const Goal goal,
            const Domain_Type WorstBound);
};


template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params Solver_Serial<Problem_Consts, Subproblem_Params, Domain_Type>::Maximize(
        const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type>& Problem_Def,
        const Problem_Consts& prob)
{
    Goal goal = Goal::MAX;
    Domain_Type WorstSolution = std::numeric_limits<Domain_Type>::lowest();
    return Solve(Problem_Def, prob, goal, WorstSolution);
}

template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params Solver_Serial<Problem_Consts, Subproblem_Params, Domain_Type>::Minimize(
        const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type>& Problem_Def,
        const Problem_Consts& prob)
{
    Goal goal = Goal::MIN;
    Domain_Type WorstSolution = std::numeric_limits<Domain_Type>::max();
    return Solve(Problem_Def, prob, goal, WorstSolution);
}


template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params Solver_Serial<Problem_Consts, Subproblem_Params, Domain_Type>::Solve(
        const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type>& Problem_Def,
        const Problem_Consts& prob,
        const Goal goal,
        const Domain_Type WorstBound)
{
    std::deque<Subproblem_Params> TaskQueue;
    TaskQueue.push_back(Problem_Def.GetInitialSubproblem(prob));

    Domain_Type BestBound = WorstBound;
    Subproblem_Params BestSubproblem;
    int NumProblemsSolved = 0;
    int ProblemsEliminated = 0;

    while(!TaskQueue.empty())
    {
	    NumProblemsSolved++;
        Subproblem_Params sol = GetNextSubproblem(TaskQueue, this->mode);

        //ignore if its bound is worse than already known best sol.
        auto [LowerBound, UpperBound] = Problem_Def.GetEstimateForBounds(prob, sol);
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
        } else if(Feasibility == BnB::FEASIBILITY::PARTIAL){
            // use our backup for the CandidateBound
            CandidateBound = UpperBound;
        }
        else if (Feasibility == BnB::FEASIBILITY::NONE) // basically discard again
            continue;

        std::vector<Subproblem_Params> v;
        // TODO try to pass the bound difference to split func to calculate the priority
        if(std::abs(CandidateBound - LowerBound) > this->Eps){ // epsilon criterion for convergence
            v = Problem_Def.SplitSolution(prob, sol);
            for(auto&& el : v)
                TaskQueue.push_back(el);
        }if(IsPotentialBestSolution)
            BestSubproblem = sol;
    }
    std::cout << "I have solved " << NumProblemsSolved << " problems" << std::endl;
    std::cout << "and eliminated " << ProblemsEliminated << " problems" << std::endl;
    Problem_Def.PrintSolution(BestSubproblem);
    return BestSubproblem;
}
