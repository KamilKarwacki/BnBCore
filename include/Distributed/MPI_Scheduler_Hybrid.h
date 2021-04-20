#pragma once
#include "MPI_Scheduler.h"

template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
class MPI_Scheduler_Hybrid: public MPI_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type>
{
public:

    struct comparator{
        bool operator()(const Subproblem_Params& p1, const Subproblem_Params& p2){
            return std::get<0>(p1) < std::get<0>(p2);
        }
    };

    Subproblem_Params Execute(const Problem_Definition<Prob_Consts, Subproblem_Params, Domain_Type>& Problem_Def,
                 const Prob_Consts& prob,
                 const MPI_Message_Encoder<Subproblem_Params>& encoder,
                 const Goal goal,
                 const Domain_Type WorstBound) override;

};


template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params MPI_Scheduler_Hybrid<Prob_Consts, Subproblem_Params, Domain_Type>::Execute(const Problem_Definition<Prob_Consts, Subproblem_Params, Domain_Type>& Problem_Def,
                                                                                  const Prob_Consts& prob,
                                                                                  const MPI_Message_Encoder<Subproblem_Params>& encoder,
                                                                                  const Goal goal,
                                                                                  const Domain_Type WorstBound)
{
    int pid, num;
    MPI_Comm_rank(MPI_COMM_WORLD, &pid);
    MPI_Comm_size(MPI_COMM_WORLD, &num);
    assert(num >= 3 && "this implementation needs at least 3 cores");
    MPI_Status st;
    MPI_Request SlaveReq;
    std::vector<MPI_Request> req(num);
    std::vector<bool> OpenRequests(num, false);
    bool RequestOngoing = false;

    omp_lock_t QueueLock;
    omp_init_lock(&QueueLock);

    std::vector<std::stringstream> sendstreams(num);
    for(auto& stream : sendstreams)
        stream << std::setprecision(15);
    std::stringstream receivstream("");
    receivstream << std::setprecision(15);
    Subproblem_Params BestSubproblem = Problem_Def.GetInitialSubproblem(prob);


    if(pid == 0){
        BestSubproblem = DefaultMasterBehavior(sendstreams, receivstream, Problem_Def, prob, encoder, goal, WorstBound);
    }
    else{
        char buffer[20000];
        // local variables of slave
        std::deque<Subproblem_Params> LocalTaskQueue;
        Domain_Type LocalBestBound = WorstBound;
        int counter;

        while(true){
            MPI_Recv(buffer,20000, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
            receivstream.str(buffer);
            if(st.MPI_TAG == Default::MessageType::PROB){
                counter = 1;
                int NumOfProblems;
                receivstream >> NumOfProblems;
                for(int i = 0; i < NumOfProblems; i++){
                    Subproblem_Params Received_Params;
                    encoder.Decode_Solution(receivstream, Received_Params);
                    LocalTaskQueue.push_back(Received_Params);
                }

                Domain_Type newBoundValue;
                receivstream >> newBoundValue;
                // for debugging it should not be the case that anyone sends a worse bound then what we already have
                if(((bool)goal && newBoundValue >= LocalBestBound)
                   || (!(bool)goal && newBoundValue <= LocalBestBound)){
                    LocalBestBound = newBoundValue;
                }

                while(!LocalTaskQueue.empty())
                {
#pragma omp parallel num_threads(this->OpenMPThreads)  shared(LocalBestBound, LocalTaskQueue, BestSubproblem, counter)
                    {
                        while(counter % this->Communication_Frequency != 0) {
                            #pragma omp atomic
                            counter = counter + 1;

                            Subproblem_Params sol;
                            bool isEmpty = false;
                            omp_set_lock(&QueueLock);
                            isEmpty = LocalTaskQueue.empty();
                            if (!isEmpty) {
                                sol = GetNextSubproblem(LocalTaskQueue, this->mode);
                            }
                            omp_unset_lock(&QueueLock);
                            if (isEmpty)
                                continue;

                            //ignore if its bound is worse than already known best sol.
                            auto [LowerBound, UpperBound] = Problem_Def.GetEstimateForBounds(prob, sol);
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
                            }else if(Feasibility == BnB::FEASIBILITY::PARTIAL){
                                CandidateBound = UpperBound;
                            }
                            else if (Feasibility == BnB::FEASIBILITY::NONE)
                                continue;


                            // check if we cant divide further
                            if (std::abs(CandidateBound - LowerBound) > this->eps) {
                                std::vector<Subproblem_Params> v;
                                v = Problem_Def.SplitSolution(prob, sol);
                                for (auto &&el : v) {
                                    auto [Lower, Upper] = Problem_Def.GetEstimateForBounds(prob, el);
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



                    counter = 1;

                        if(!RequestOngoing) {
                            sendstreams[0].str("");
                            sendstreams[0] << LocalBestBound << " ";
                            sendstreams[0] << (int) LocalTaskQueue.size() << " ";
                            MPI_Isend(&sendstreams[0].str()[0], strlen(sendstreams[0].str().c_str()), MPI_CHAR, 0,
                                       Default::MessageType::GET_SLAVES, MPI_COMM_WORLD, &SlaveReq);
                            RequestOngoing = true;
                        }

                        if(RequestOngoing){
                            int flag = 0;
                            MPI_Test(&SlaveReq, &flag, MPI_STATUS_IGNORE);
                            if(flag == 1) {
                                RequestOngoing = false;
                                //get master's response
                                MPI_Recv(buffer,200, MPI_CHAR, 0, Default::MessageType::GET_SLAVES, MPI_COMM_WORLD, &st);
                                receivstream.str(buffer);
                                int slaves_avbl;
                                receivstream >> LocalBestBound;
                                receivstream >> slaves_avbl;

                                for(int i=0; i<slaves_avbl; i++){
                                    //give a problem to each slave
                                    int sl_no;
                                    receivstream >> sl_no;
                                    int RestSize = std::min(this->MaxPackageSize, (int)LocalTaskQueue.size());
                                    std::vector<Subproblem_Params> SubproblemsToSend;
                                    while(!LocalTaskQueue.empty() and SubproblemsToSend.size() != RestSize) {
                                        Subproblem_Params subprb = LocalTaskQueue.front(); LocalTaskQueue.pop_front();
                                        SubproblemsToSend.push_back(subprb);
                                    }

                                    sendstreams[sl_no].str("");
                                    sendstreams[sl_no] << static_cast<int>(SubproblemsToSend.size()) << " ";
                                    for(auto&& subproblem : SubproblemsToSend)
                                        encoder.Encode_Solution(sendstreams[sl_no], subproblem);
                                    sendstreams[sl_no] << LocalBestBound << " ";
                                    //send it to idle processor
                                    if(OpenRequests[sl_no]) MPI_Wait(&req[sl_no], MPI_STATUS_IGNORE);
                                    MPI_Isend(&sendstreams[sl_no].str()[0],strlen(sendstreams[sl_no].str().c_str()), MPI_CHAR, sl_no,
                                              Default::MessageType::PROB, MPI_COMM_WORLD, &req[sl_no]);
                                    OpenRequests[sl_no] = true;
                                }
                            }
                        }
                }

                //This slave has now become idle (its queue is empty). Inform master.
                sendstreams[0].str("");
                MPI_Send(&sendstreams[0].str()[0],0,MPI_CHAR,0,Default::MessageType::IDLE, MPI_COMM_WORLD);
            }
            else if(st.MPI_TAG == Default::MessageType::FINISH){
                break;
            }
        }
    }

    omp_destroy_lock(&QueueLock);

    if(pid != 0){
        if(RequestOngoing)
            MPI_Request_free(&SlaveReq);
        for(int i = 1; i < num; i++)
            if(OpenRequests[i]) MPI_Request_free(&req[i]);
    }
    // ------------------------ ALL procs have a best solution now master has to gather it
    return ExtractBestSolution<Prob_Consts,Subproblem_Params, Domain_Type>(sendstreams[0], receivstream,
                                                                           BestSubproblem,
                                                                           Problem_Def,
                                                                           prob,
                                                                           encoder,
                                                                           goal);
}
