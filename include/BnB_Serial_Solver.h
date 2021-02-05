#pragma once

#include "Base.h"

// main solver class has to be initiated by the user
// Problem_Consts    -- should be an std::tuple holding constants of the problem
// Subproblem_Params -- should be an std::tuple holding values that describe the problem
// Domain_Type       -- one of the following : double, float, int
template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
class Solver_Serial
{
public:
    // maximizes a problem defines by the user
    Subproblem_Params Maximize(const Problem_Definition<Problem_Consts, Subproblem_Params>&, const Problem_Consts&);
    // minimizes a problem defined by the user
    Subproblem_Params Minimize(const Problem_Definition<Problem_Consts, Subproblem_Params>&, const Problem_Consts&);
    void SetEps(double eps){Eps = eps;}

private:
    double Eps = 0.001;

    Subproblem_Params Solve(
            const Problem_Definition<Problem_Consts, Subproblem_Params>& Problem_Def,
            const Problem_Consts& prob,
            const Goal goal,
            const Domain_Type WorstBound);
};


template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params Solver_Serial<Problem_Consts, Subproblem_Params, Domain_Type>::Maximize(
        const Problem_Definition<Problem_Consts, Subproblem_Params>& Problem_Def,
        const Problem_Consts& prob)
{
    Goal goal = Goal::MAX;
    Domain_Type WorstSolution = std::numeric_limits<Domain_Type>::lowest();
    std::cout << "epsilon is set to " << Eps << std::endl;
    return Solve(Problem_Def, prob, goal, WorstSolution);
}

template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params Solver_Serial<Problem_Consts, Subproblem_Params, Domain_Type>::Minimize(
        const Problem_Definition<Problem_Consts, Subproblem_Params>& Problem_Def,
        const Problem_Consts& prob)
{
    Goal goal = Goal::MIN;
    Domain_Type WorstSolution = std::numeric_limits<Domain_Type>::max();
    std::cout << "epsilon is set to " << Eps << std::endl;
    return Solve(Problem_Def, prob, goal, WorstSolution);
}


template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params Solver_Serial<Problem_Consts, Subproblem_Params, Domain_Type>::Solve(
        const Problem_Definition<Problem_Consts, Subproblem_Params>& Problem_Def,
        const Problem_Consts& prob,
        const Goal goal,
        const Domain_Type WorstBound)
{
    struct comparator{
        bool operator()(const Subproblem_Params& p1, const Subproblem_Params& p2){
            return std::get<0>(p1) < std::get<0>(p2);
        }
    };

    std::priority_queue<Subproblem_Params, std::vector<Subproblem_Params>, comparator> TaskQueue;
    TaskQueue.push(Problem_Def.GetInitialSubproblem(prob));
    Domain_Type BestBound = WorstBound;
    Subproblem_Params BestSubproblem;

    while(!TaskQueue.empty())
    {
        Subproblem_Params sol = TaskQueue.top(); TaskQueue.pop();

        //ignore if its bound is worse than already known best sol.
        Domain_Type LowerBound = std::get<Domain_Type>(Problem_Def.GetLowerBound(prob, sol));
        if(((bool)goal && LowerBound < BestBound)
           || (!(bool)goal && LowerBound > BestBound)){
            continue;
        }

        // try to make the bound better only if the solution lies in a feasible domain
        auto Feasibility = Problem_Def.IsFeasible(prob, sol);
        Domain_Type CandidateBound;
        if(Feasibility == BnB::FEASIBILITY::FULL)
        {
            CandidateBound = std::get<Domain_Type>(Problem_Def.GetUpperBound(prob, sol));
            if(((bool)goal && CandidateBound >= BestBound)
               || (!(bool)goal && CandidateBound <= BestBound)){
                BestBound = CandidateBound;
                BestSubproblem = sol;
            }
        }else if(Feasibility == BnB::FEASIBILITY::NONE) // basically discard again
            continue;

        std::vector<Subproblem_Params> v;
        // TODO try to pass the bound idfference to split func to calcualte the priority
        if(std::abs(CandidateBound - LowerBound) > this->Eps){ // epsilon criterion for convergence
            v = Problem_Def.SplitSolution(prob, sol);
            for(const auto& el : v)
                TaskQueue.push(el);
        }
    }
    Problem_Def.PrintSolution(BestSubproblem);
    return BestSubproblem;
}
