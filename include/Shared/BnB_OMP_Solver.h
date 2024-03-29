#pragma once
#include <omp.h>
#include "Base.h"
#include "BnB_Solver.h"
#include "OMP_Scheduler_Tasking.h"
#include "OMP_Scheduler_Queue.h"

namespace BnB {


    // types of schedulers that can be loaded at runtime
    enum class OMP_Scheduler_Type {
        TASKING, QUEUE
    };

    template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
    class Solver_OMP : public Solver<Problem_Consts, Subproblem_Params, Domain_Type> {
    public:
        void SetScheduler(OMP_Scheduler_Type);

        void SetNumThreads(int num) { numThreads = num; }

        // returns a reference to the scheduler who exposes its parameters
        auto SetSchedulerParameters() { return scheduler.get(); }


        Subproblem_Params
        Maximize(const Problem_Definition <Problem_Consts, Subproblem_Params, Domain_Type> &Problem_Def,
                 const Problem_Consts &prob);

        Subproblem_Params
        Minimize(const Problem_Definition <Problem_Consts, Subproblem_Params, Domain_Type> &Problem_Def,
                 const Problem_Consts &prob);

    private:
        // number of threads that will be used for execution
        size_t numThreads = 1;

        std::unique_ptr<OMP_Scheduler<Problem_Consts, Subproblem_Params, Domain_Type>> scheduler =
                std::make_unique<OMP_Scheduler_Tasking<Problem_Consts, Subproblem_Params, Domain_Type>>();
    };


    template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
    Subproblem_Params Solver_OMP<Problem_Consts, Subproblem_Params, Domain_Type>::Maximize(
            const Problem_Definition <Problem_Consts, Subproblem_Params, Domain_Type> &Problem_Def,
            const Problem_Consts &prob) {
        Domain_Type WorstSolution = std::numeric_limits<Domain_Type>::lowest();
        omp_set_dynamic(0);
        omp_set_num_threads(numThreads);
        Subproblem_Params result = scheduler->Execute(Problem_Def, prob, Goal::MAX, WorstSolution);
        std::cout << "threads are set to " << numThreads << std::endl;
        return result;
    }

    template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
    Subproblem_Params Solver_OMP<Problem_Consts, Subproblem_Params, Domain_Type>::Minimize(
            const Problem_Definition <Problem_Consts, Subproblem_Params, Domain_Type> &Problem_Def,
            const Problem_Consts &prob) {
        Domain_Type WorstSolution = std::numeric_limits<Domain_Type>::max();
        omp_set_dynamic(0);
        omp_set_num_threads(numThreads);
        Subproblem_Params result = scheduler->Execute(Problem_Def, prob, Goal::MIN, WorstSolution);
        std::cout << "threads are set to " << numThreads << std::endl;
        return result;
    }

    template<typename Problem_Consts, typename Subproblem_Params, typename Domain_Type>
    void Solver_OMP<Problem_Consts, Subproblem_Params, Domain_Type>::SetScheduler(OMP_Scheduler_Type type) {
        switch (type) {
            case OMP_Scheduler_Type::TASKING:
                scheduler = std::make_unique<OMP_Scheduler_Tasking<Problem_Consts, Subproblem_Params, Domain_Type>>();
                break;
            case OMP_Scheduler_Type::QUEUE:
                scheduler = std::make_unique<OMP_Scheduler_Queue<Problem_Consts, Subproblem_Params, Domain_Type>>();
                break;
        }
    }

}
