#pragma once

#include "Base.h"

namespace BnB{
    // strategy pattern that holds the branch-and-bound procedure
    template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
    class Serial_Scheduler {
    public:
        virtual Subproblem_Params
        Execute(const Problem_Definition <Prob_Consts, Subproblem_Params, Domain_Type> &Problem_Def,
                const Prob_Consts &prob,
                const Goal goal,
                const Domain_Type WorstBound) = 0;

        Serial_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type> *Eps(Domain_Type e) {EPS = e; return this;}
        Serial_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type> *TraversMode(TraversalMode m) {mode = m; return this;};

    protected:
        double EPS = 0.001;
        TraversalMode mode = TraversalMode::DFS;
    };
}