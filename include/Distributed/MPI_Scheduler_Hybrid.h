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
    std::stringstream sendstream("");
    sendstream << std::setprecision(15);
    std::stringstream receivstream("");
    receivstream << std::setprecision(15);
    Subproblem_Params BestSubproblem = Problem_Def.GetInitialSubproblem(prob);


    if(pid == 0){
        BestSubproblem = DefaultMasterBehavior(sendstream, receivstream, Problem_Def, prob, encoder, goal, WorstBound);
    }
    else{
        char buffer[2000];
        // local variables of slave
        std::deque<Subproblem_Params> LocalTaskQueue;
        Domain_Type LocalBestBound = WorstBound;
        int counter;

        while(true){
            MPI_Recv(buffer,2000, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
            receivstream.str(buffer);
            if(st.MPI_TAG == Default::MessageType::PROB){
                counter = 1;

                Subproblem_Params Received_Params;
                encoder.Decode_Solution(receivstream, Received_Params);
                LocalTaskQueue.push_back(Received_Params);

                Domain_Type newBoundValue;
                receivstream >> newBoundValue;
                if(((bool)goal && newBoundValue > LocalBestBound)
                   || (!(bool)goal && newBoundValue < LocalBestBound)){
                    LocalBestBound = newBoundValue;
                }
                while(!LocalTaskQueue.empty())
                {
                    #pragma omp parallel num_threads(this->OpenMPThreads) private(sendstream) shared(LocalBestBound, LocalTaskQueue)
                    {
                        while(counter % this->Communication_Frequency != 0) {
                            #pragma omp atomic
                            counter = counter + 1;

                            Subproblem_Params sol;
                            bool isEmpty = false;
                            #pragma omp critical (queuelock)
                            {
                                isEmpty = LocalTaskQueue.empty();
                                if (!isEmpty) {
                                    if(this->mode == TraversalMode::DFS){
                                        sol = LocalTaskQueue.back(); LocalTaskQueue.pop_back();
                                    }else if( this->mode == TraversalMode::BFS){
                                        sol = LocalTaskQueue.front(); LocalTaskQueue.pop_front();
                                    }
                                }
                            }
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
                            std::vector<Subproblem_Params> v;
                            if (std::abs(CandidateBound - LowerBound) > this->eps) {
                                v = Problem_Def.SplitSolution(prob, sol);
                                for (auto &&el : v) {
                                    #pragma omp critical (queuelock)
                                    {
                                        LocalTaskQueue.push_back(el);
                                    }
                                }
                            }
                        }
                    }

                    counter = 1; // start new omp session after this

                    if(!LocalTaskQueue.empty()) // after omp region we are gonna send request for slaves and also our best bound
                    {
                        sendstream.str("");
                        sendstream << LocalBestBound << " ";
                        sendstream << (int)LocalTaskQueue.size() << " ";
                        MPI_Send(&sendstream.str()[0],strlen(sendstream.str().c_str()),MPI_CHAR,0,Default::MessageType::GET_SLAVES,MPI_COMM_WORLD);
                        //get master's response
                        MPI_Recv(buffer,200, MPI_CHAR, 0, Default::MessageType::GET_SLAVES, MPI_COMM_WORLD, &st);
                        receivstream.str(buffer);
                        int slaves_avbl;
                        receivstream >> LocalBestBound;
                        receivstream >> slaves_avbl;

                        for(int i=0; i<slaves_avbl; i++){
                            //give a problem to each slave
                            Subproblem_Params SubProb;
                            if(this->mode == TraversalMode::DFS){
                                SubProb = LocalTaskQueue.back(); LocalTaskQueue.pop_back();
                            }else if( this->mode == TraversalMode::BFS){
                                SubProb = LocalTaskQueue.front(); LocalTaskQueue.pop_front();
                            }
                            int sl_no;
                            receivstream >> sl_no;
                            sendstream.str("");
                            encoder.Encode_Solution(sendstream, SubProb);
                            sendstream << LocalBestBound << " ";
                            //send it to idle processor
                            // TODO maybe non blocking
                            MPI_Send(&sendstream.str()[0],strlen(sendstream.str().c_str()), MPI_CHAR, sl_no, Default::MessageType::PROB, MPI_COMM_WORLD);
                        }
                    }
                }

                //This slave has now become idle (its queue is empty). Inform master.
                sendstream.str("");
                MPI_Send(&sendstream.str()[0],0,MPI_CHAR,0,Default::MessageType::IDLE, MPI_COMM_WORLD);
            }
            else if(st.MPI_TAG == Default::MessageType::FINISH){
                break;
            }
        }
    }
    // ------------------------ ALL procs have a best solution now master has to gather it
    return ExtractBestSolution<Prob_Consts,Subproblem_Params, Domain_Type>(sendstream, receivstream,
                                                                           BestSubproblem,
                                                                           Problem_Def,
                                                                           prob,
                                                                           encoder,
                                                                           goal);
}
