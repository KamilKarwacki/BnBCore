#pragma once
#include "MPI_Message_Encoder.h"
#include "MPI_Scheduler_Default.h"
#include "MPI_Scheduler_Priority.h"
#include "Base.h"

enum class Scheduler_Type
{
    DEFAULT,
    PRIORITY,
    SOMEOTHERSTUFF,
};


template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
class Solver
{
public:
    // maximizes a problem defines by the user
    void Maximize(const Problem_Definition<Problem_Consts, Subproblem_Params>& Problem_Def, const Problem_Consts& prob);
    // minimizes a problem defined by the user
    void Minimize(const Problem_Definition<Problem_Consts, Subproblem_Params>& Problem_Def, const Problem_Consts& prob);
    // if not called default scheduler will be used
    void SetScheduler(Scheduler_Type);

private:

    MPI_Message_Encoder<Subproblem_Params> encoder;
    Domain_Type BestSolution;

    std::unique_ptr<MPI_Scheduler<Problem_Consts, Subproblem_Params, Domain_Type>> scheduler =
            std::make_unique<MPI_Scheduler_Default<Problem_Consts, Subproblem_Params, Domain_Type>>();
};


template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
void Solver<Problem_Consts, Subproblem_Params, Domain_Type>::Maximize(
        const Problem_Definition<Problem_Consts, Subproblem_Params>& Problem_Def,
        const Problem_Consts& prob)
{
    Goal goal = Goal::MAX;
    BestSolution = std::numeric_limits<Domain_Type>::lowest()/2.0;
    scheduler->Execute(Problem_Def, prob, encoder, goal);
}

template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
void Solver<Problem_Consts, Subproblem_Params, Domain_Type>::Minimize(
        const Problem_Definition<Problem_Consts, Subproblem_Params>& Problem_Def,
        const Problem_Consts& prob)
{
    Goal goal = Goal::MIN;
    BestSolution = std::numeric_limits<Domain_Type>::max()/2.0;
    scheduler->Execute(Problem_Def, prob, encoder, goal);
}

template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
void Solver<Problem_Consts, Subproblem_Params, Domain_Type>::SetScheduler(Scheduler_Type type)
{
    if(type == Scheduler_Type::DEFAULT)
    {
        scheduler = std::make_unique<MPI_Scheduler_Default<Problem_Consts, Subproblem_Params, Domain_Type>>();
    }
    else if(type == Scheduler_Type::PRIORITY)
    {
        scheduler = std::make_unique<MPI_Scheduler_Priority<Problem_Consts, Subproblem_Params, Domain_Type>>();
    }
}
