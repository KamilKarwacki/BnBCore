#pragma once

#include "Base.h"
#include "BnB_Solver.h"
#include "Serial_Scheduler.h"
#include "Serial_Scheduler_Default.h"

namespace BnB {
    // main solver class has to be initiated by the user
    // Problem_Consts    -- should be an std::tuple holding constants of the problem
    // Subproblem_Params -- should be an std::tuple holding values that describe the problem
    // Domain_Type       -- one of the following : double, float, int
    template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
    class Solver_Serial : public Solver<Problem_Consts, Subproblem_Params, Domain_Type> {
    public:
        // maximizes a problem defines by the user
        Subproblem_Params
        Maximize(const Problem_Definition <Problem_Consts, Subproblem_Params, Domain_Type> &, const Problem_Consts &);
        // minimizes a problem defined by the user
        Subproblem_Params
        Minimize(const Problem_Definition <Problem_Consts, Subproblem_Params, Domain_Type> &, const Problem_Consts &);
    private:
        std::unique_ptr<Serial_Scheduler<Problem_Consts, Subproblem_Params, Domain_Type>> scheduler =
                std::make_unique<Serial_Scheduler_Default<Problem_Consts, Subproblem_Params, Domain_Type>>();

    };

    template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
    Subproblem_Params Solver_Serial<Problem_Consts, Subproblem_Params, Domain_Type>::Maximize(
            const Problem_Definition <Problem_Consts, Subproblem_Params, Domain_Type> &Problem_Def,
            const Problem_Consts &prob) {
        Goal goal = Goal::MAX;
        Domain_Type WorstSolution = std::numeric_limits<Domain_Type>::lowest();
        return scheduler->Execute(Problem_Def, prob, goal, WorstSolution);
    }

    template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
    Subproblem_Params Solver_Serial<Problem_Consts, Subproblem_Params, Domain_Type>::Minimize(
            const Problem_Definition <Problem_Consts, Subproblem_Params, Domain_Type> &Problem_Def,
            const Problem_Consts &prob) {
        Goal goal = Goal::MIN;
        Domain_Type WorstSolution = std::numeric_limits<Domain_Type>::max();
        return scheduler->Execute(Problem_Def, prob, goal, WorstSolution);
    }
}
