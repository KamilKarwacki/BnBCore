#pragma once
#include "MPI_Scheduler.h"

namespace BnB {
    template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
    class MPI_Scheduler_Hybrid : public MPI_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type> {
    public:


        Subproblem_Params Execute(const Problem_Definition <Prob_Consts, Subproblem_Params, Domain_Type> &Problem_Def,
                                  const Prob_Consts &prob,
                                  const MPI_Message_Encoder <Subproblem_Params> &encoder,
                                  const Goal goal,
                                  const Domain_Type WorstBound) override;

        MPI_Scheduler_Hybrid<Prob_Consts, Subproblem_Params, Domain_Type> *Threads(size_t num) {OpenMPThreads = num; return this;}

    private:
        int OpenMPThreads = 1;
    };


    template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
    Subproblem_Params MPI_Scheduler_Hybrid<Prob_Consts, Subproblem_Params, Domain_Type>::Execute(
            const Problem_Definition <Prob_Consts, Subproblem_Params, Domain_Type> &Problem_Def,
            const Prob_Consts &prob,
            const MPI_Message_Encoder <Subproblem_Params> &encoder,
            const Goal goal,
            const Domain_Type WorstBound) {
        int pid, num;
        MPI_Comm_rank(MPI_COMM_WORLD, &pid);
        MPI_Comm_size(MPI_COMM_WORLD, &num);
        assert(num >= 2 && "this implementation needs at least 3 cores");
        MPI_Status st;
        MPI_Request SlaveReq;
        std::vector<MPI_Request> req(num);
        std::vector<bool> OpenRequests(num, false);
        bool RequestOngoing = false;
        int TasksDone = 0;

        omp_lock_t QueueLock;
        omp_init_lock(&QueueLock);

        std::vector<std::stringstream> sendstreams(num);
        for (auto &stream : sendstreams)
            stream << std::setprecision(15);
        std::stringstream receivstream("");
        receivstream << std::setprecision(15);
        Subproblem_Params BestSubproblem = Problem_Def.GetInitialSubproblem(prob);


        if (pid == 0) {
            printProc("threads: " << this->OpenMPThreads)
            ///BestSubproblem = SplittingMasterBehavior(sendstreams, receivstream, Problem_Def, prob, encoder, goal, WorstBound);
            BestSubproblem = DefaultMasterBehavior(sendstreams, receivstream, Problem_Def, prob, encoder, goal, WorstBound);
        } else { // Worker
            char buffer[20000];
            // local variables of
            std::deque<Subproblem_Params> LocalTaskQueue;
            Domain_Type LocalBestBound = WorstBound;
            int counter = 0;

            while (true) {
                MPI_Recv(buffer, 20000, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
                receivstream.str(buffer);
                if (st.MPI_TAG == PtoP::MessageType::PROB) {
                    int NumOfProblems;
                    receivstream >> NumOfProblems;
                    for (int i = 0; i < NumOfProblems; i++) {
                        Subproblem_Params Received_Params;
                        encoder.Decode_Solution(receivstream, Received_Params);
                        LocalTaskQueue.push_back(Received_Params);
                    }

                    Domain_Type newBoundValue;
                    receivstream >> newBoundValue;
                    // the new work package comes with a bound, check if the bound is better
                    if (((bool) goal && newBoundValue >= LocalBestBound)
                        || (!(bool) goal && newBoundValue <= LocalBestBound)) {
                        LocalBestBound = newBoundValue;
                    }

                    while (!LocalTaskQueue.empty()) {
                        counter = 1;
                        int dynamicThreads = std::min(this->OpenMPThreads, (int) LocalTaskQueue.size());
#pragma omp parallel num_threads(dynamicThreads)  shared(LocalBestBound, LocalTaskQueue, BestSubproblem, counter)
                        {
                            while (counter % this->Communication_Frequency != 0) {
                                #pragma omp atomic
                                counter = counter + 1;
                                #pragma omp atomic
                                TasksDone = TasksDone + 1;

                                Subproblem_Params sol;
                                bool isEmpty = false;
                                omp_set_lock(&QueueLock);
                                isEmpty = LocalTaskQueue.empty();
                                if (!isEmpty) {
                                    sol = GetNextSubproblem(LocalTaskQueue, this->mode);
                                }
                                omp_unset_lock(&QueueLock);
                                if (isEmpty)
                                    break;

                                //ignore if its bound is worse than already known best sol.
                                auto[LowerBound, UpperBound] = Problem_Def.GetEstimateForBounds(prob, sol);
                                if (((bool) goal && LowerBound < LocalBestBound)
                                    || (!(bool) goal && LowerBound > LocalBestBound)) {
                                    continue;
                                }
                                // try to make the bound better
                                auto Feasibility = Problem_Def.IsFeasible(prob, sol);
                                Domain_Type CandidateBound;
                                if (Feasibility == BnB::FEASIBILITY::Full) {
                                    CandidateBound = Problem_Def.GetContainedUpperBound(prob, sol);
#pragma omp critical
                                    {
                                        if (((bool) goal && CandidateBound >= LocalBestBound)
                                            || (!(bool) goal && CandidateBound <= LocalBestBound)) {
                                            LocalBestBound = CandidateBound;
                                            BestSubproblem = sol;
                                        }
                                    }
                                } else if (Feasibility == BnB::FEASIBILITY::PARTIAL) {
                                    CandidateBound = UpperBound;
                                } else if (Feasibility == BnB::FEASIBILITY::NONE)
                                    continue;


                                // check if we cant divide further
                                if (std::abs(CandidateBound - LowerBound) > this->eps) {
                                    std::vector<Subproblem_Params> v;
                                    v = Problem_Def.SplitSolution(prob, sol);
                                    for (auto &&el : v) {
                                        auto[Lower, Upper] = Problem_Def.GetEstimateForBounds(prob, el);
                                        if (((bool) goal && Lower < LocalBestBound)
                                            || (!(bool) goal && Lower > LocalBestBound)) {
                                            continue;
                                        }
                                        omp_set_lock(&QueueLock);
                                        LocalTaskQueue.push_back(el);
                                        omp_unset_lock(&QueueLock);
                                    }
                                }
                            }
#pragma omp barrier
                        }

                        if (!RequestOngoing && !LocalTaskQueue.empty()) {
                            sendstreams[0].str("");
                            sendstreams[0] << LocalBestBound << " ";
                            sendstreams[0] << (int) LocalTaskQueue.size() << " ";
                            MPI_Isend(&sendstreams[0].str()[0], strlen(sendstreams[0].str().c_str()), MPI_CHAR, 0,
                                      PtoP::MessageType::GET_WORKERS, MPI_COMM_WORLD, &SlaveReq);
                            RequestOngoing = true;
                        }

                        if (RequestOngoing) {
                            int flag = 0;
                            MPI_Test(&SlaveReq, &flag, MPI_STATUS_IGNORE);
                            if (flag == 1) {
                                RequestOngoing = false;
                                //get master's response
                                MPI_Recv(buffer, 20000, MPI_CHAR, 0, PtoP::MessageType::GET_WORKERS, MPI_COMM_WORLD,
                                         &st);
                                receivstream.str(buffer);
                                int slaves_avbl;
                                Domain_Type MastersBound;
                                receivstream >> MastersBound;

                                if (((bool) goal && MastersBound >= LocalBestBound)
                                    || (!(bool) goal && MastersBound <= LocalBestBound)) {
                                    LocalBestBound = MastersBound;
                                }

                                receivstream >> slaves_avbl;

                                for (int i = 0; i < slaves_avbl; i++) {
                                    //give a problem to each slave
                                    int sl_no;
                                    receivstream >> sl_no;
                                    int RestSize = std::min(this->MaxPackageSize, (int) LocalTaskQueue.size());
                                    std::vector<Subproblem_Params> SubproblemsToSend;
                                    while (!LocalTaskQueue.empty() and SubproblemsToSend.size() != RestSize) {
                                        Subproblem_Params subprb = LocalTaskQueue.front();
                                        LocalTaskQueue.pop_front();
                                        auto[Lower, Upper] = Problem_Def.GetEstimateForBounds(prob, subprb);
                                        if (((bool) goal && Lower < LocalBestBound)
                                            || (!(bool) goal && Lower > LocalBestBound)) {
                                            continue;
                                        }
                                        SubproblemsToSend.push_back(subprb);
                                    }

                                    sendstreams[sl_no].str("");
                                    sendstreams[sl_no] << static_cast<int>(SubproblemsToSend.size()) << " ";
                                    for (auto &&subproblem : SubproblemsToSend)
                                        encoder.Encode_Solution(sendstreams[sl_no], subproblem);
                                    sendstreams[sl_no] << LocalBestBound << " ";
                                    //send it to idle processor
                                    if (OpenRequests[sl_no]) MPI_Wait(&req[sl_no], MPI_STATUS_IGNORE);
                                    MPI_Isend(&sendstreams[sl_no].str()[0], strlen(sendstreams[sl_no].str().c_str()),
                                              MPI_CHAR, sl_no,
                                              PtoP::MessageType::PROB, MPI_COMM_WORLD, &req[sl_no]);
                                    OpenRequests[sl_no] = true;
                                }
                            }
                        }

                    }

                    //This slave has now become idle (its queue is empty). Inform master.
                    sendstreams[0].str("");
                    MPI_Ssend(&sendstreams[0].str()[0], 0, MPI_CHAR, 0, PtoP::MessageType::IDLE, MPI_COMM_WORLD);
                } else if (st.MPI_TAG == PtoP::MessageType::FINISH) {
                    break;
                } else if (st.MPI_TAG == PtoP::MessageType::GET_WORKERS) {
                    RequestOngoing = false;
                    //get master's response
                    int slaves_avbl;
                    Domain_Type MastersBound;
                    receivstream >> MastersBound;

                    if (((bool) goal && MastersBound >= LocalBestBound)
                        || (!(bool) goal && MastersBound <= LocalBestBound)) {
                        LocalBestBound = MastersBound;
                    }

                    receivstream >> slaves_avbl;

                    for (int i = 0; i < slaves_avbl; i++) {
                        //give a problem to each slave
                        int sl_no;
                        receivstream >> sl_no;

                        sendstreams[sl_no].str("");
                        sendstreams[sl_no] << 0 << " ";
                        //send it to idle processor
                        sendstreams[sl_no] << LocalBestBound << " ";
                        if (OpenRequests[sl_no]) MPI_Wait(&req[sl_no], MPI_STATUS_IGNORE);
                        MPI_Isend(&sendstreams[sl_no].str()[0], strlen(sendstreams[sl_no].str().c_str()), MPI_CHAR,
                                  sl_no,
                                  PtoP::MessageType::PROB, MPI_COMM_WORLD, &req[sl_no]);
                        OpenRequests[sl_no] = true;
                    }
                }
            }
        }

        omp_destroy_lock(&QueueLock);


        printProc("I have done " << TasksDone)

        if (pid != 0) {
            if (RequestOngoing)
                MPI_Request_free(&SlaveReq);
            for (int i = 1; i < num; i++)
                if (OpenRequests[i]) MPI_Request_free(&req[i]);
        }
        // ------------------------ ALL procs have a best solution now master has to gather it
        return ExtractBestSolution<Prob_Consts, Subproblem_Params, Domain_Type>(sendstreams[0], receivstream,
                                                                                BestSubproblem,
                                                                                Problem_Def,
                                                                                prob,
                                                                                encoder,
                                                                                goal);
    }


}
