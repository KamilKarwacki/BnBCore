#pragma once
#include "MPI_Message_Encoder.h"
#include "MPI_Scheduler_Default.h"
#include "MPI_Scheduler_Priority.h"
#include "MPI_Scheduler_Hybrid.h"
#include "Base.h"

enum class Scheduler_Type
{
    DEFAULT,
    PRIORITY,
    HYBRID,
};


template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
class Solver_MPI
{
public:
    // maximizes a problem defines by the user
    void Maximize(const Problem_Definition<Problem_Consts, Subproblem_Params>&, const Problem_Consts&);
    // minimizes a problem defined by the user
    void Minimize(const Problem_Definition<Problem_Consts, Subproblem_Params>&, const Problem_Consts&);
    // if not called default scheduler will be used
    void SetScheduler(Scheduler_Type);
    // set frequency of worker to master communication
    auto SetSchedulerParameters() {return scheduler.get();}

private:

    MPI_Message_Encoder<Subproblem_Params> encoder;

    std::unique_ptr<MPI_Scheduler<Problem_Consts, Subproblem_Params, Domain_Type>> scheduler =
            std::make_unique<MPI_Scheduler_Default<Problem_Consts, Subproblem_Params, Domain_Type>>();
};


template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
void Solver_MPI<Problem_Consts, Subproblem_Params, Domain_Type>::Maximize(
        const Problem_Definition<Problem_Consts, Subproblem_Params>& Problem_Def,
        const Problem_Consts& prob)
{
    Goal goal = Goal::MAX;
    Domain_Type WorstSolution = std::numeric_limits<Domain_Type>::lowest()/2.0;
    scheduler->Execute(Problem_Def, prob, encoder, goal, WorstSolution);
}

template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
void Solver_MPI<Problem_Consts, Subproblem_Params, Domain_Type>::Minimize(
        const Problem_Definition<Problem_Consts, Subproblem_Params>& Problem_Def,
        const Problem_Consts& prob)
{
    Goal goal = Goal::MIN;
    Domain_Type WorstSolution = std::numeric_limits<Domain_Type>::max()/2.0;
    scheduler->Execute(Problem_Def, prob, encoder, goal, WorstSolution);
}

template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
void Solver_MPI<Problem_Consts, Subproblem_Params, Domain_Type>::SetScheduler(Scheduler_Type type)
{
    switch(type){
        case Scheduler_Type::DEFAULT:
            scheduler = std::make_unique<MPI_Scheduler_Default<Problem_Consts, Subproblem_Params, Domain_Type>>();
            break;
        case Scheduler_Type::PRIORITY:
            scheduler = std::make_unique<MPI_Scheduler_Priority<Problem_Consts, Subproblem_Params, Domain_Type>>();
            break;
        case Scheduler_Type::HYBRID:
            scheduler = std::make_unique<MPI_Scheduler_Hybrid<Problem_Consts, Subproblem_Params, Domain_Type>>();
            break;
    }
}
