#pragma once
#include "MPI_Message_Encoder.h"
#include "MPI_Scheduler_Priority.h"
#include "MPI_Scheduler_Hybrid.h"
#include "MPI_Scheduler_OneSided.h"
#include "BnB_Solver.h"
#include "Base.h"

enum class MPI_Scheduler_Type{PRIORITY, HYBRID, ONESIDED,};

// main solver class has to be initiated by the user
// Problem_Consts    -- should be an std::tuple holding constants of the problem
// Subproblem_Params -- should be an std::tuple holding values that describe the problem
// Domain_Type       -- one of the following : double, float, int
template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
class Solver_MPI : public BnB_Solver<Problem_Consts, Subproblem_Params, Domain_Type>
{
public:

    // maximizes a problem defines by the user
    Subproblem_Params Maximize(const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type>&, const Problem_Consts&);
    // minimizes a problem defined by the user
    Subproblem_Params Minimize(const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type>&, const Problem_Consts&);
    // if not called default scheduler will be used
    void SetScheduler(MPI_Scheduler_Type Scheduler);
    // set frequency of worker to master communication
    auto SetSchedulerParameters() {return scheduler.get();}

private:
    // can encode the Subproblem_Params into a string that can be send via MPI
    MPI_Message_Encoder<Subproblem_Params> encoder;

    std::unique_ptr<MPI_Scheduler<Problem_Consts, Subproblem_Params, Domain_Type>> scheduler =
            std::make_unique<MPI_Scheduler_Priority<Problem_Consts, Subproblem_Params, Domain_Type>>();
};


template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params Solver_MPI<Problem_Consts, Subproblem_Params, Domain_Type>::Maximize(
        const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type>& Problem_Def,
        const Problem_Consts& prob)
{
    Goal goal = Goal::MAX;
    Domain_Type WorstSolution = std::numeric_limits<Domain_Type>::lowest()/2.0;
    return scheduler->Execute(Problem_Def, prob, encoder, goal, WorstSolution);
}

template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params Solver_MPI<Problem_Consts, Subproblem_Params, Domain_Type>::Minimize(
        const Problem_Definition<Problem_Consts, Subproblem_Params, Domain_Type>& Problem_Def,
        const Problem_Consts& prob)
{
    Goal goal = Goal::MIN;
    Domain_Type WorstSolution = std::numeric_limits<Domain_Type>::max()/2.0;
    return scheduler->Execute(Problem_Def, prob, encoder, goal, WorstSolution);
}

template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
void Solver_MPI<Problem_Consts, Subproblem_Params, Domain_Type>::SetScheduler(MPI_Scheduler_Type type)
{
    switch(type){
        case MPI_Scheduler_Type::PRIORITY:
            scheduler = std::make_unique<MPI_Scheduler_Priority<Problem_Consts, Subproblem_Params, Domain_Type>>();
            break;
        case MPI_Scheduler_Type::HYBRID:
            scheduler = std::make_unique<MPI_Scheduler_Hybrid<Problem_Consts, Subproblem_Params, Domain_Type>>();
            break;
        case MPI_Scheduler_Type::ONESIDED:
            scheduler = std::make_unique<MPI_Scheduler_OneSided<Problem_Consts, Subproblem_Params, Domain_Type>>();
            break;
    }
}

