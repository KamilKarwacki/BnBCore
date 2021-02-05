#pragma once
#include "Base.h"
#include "MPI_Scheduler_Default.h"


template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
class MPI_Scheduler_Priority: public MPI_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type>
{
public:

    struct comparator{
       bool operator()(const Subproblem_Params& p1, const Subproblem_Params& p2){
           return std::get<0>(p1) < std::get<0>(p2);
       }
    };


    Subproblem_Params Execute(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
                 const Prob_Consts& prob,
                 const MPI_Message_Encoder<Subproblem_Params>& encoder,
                 const Goal goal,
                 const Domain_Type WorstBound) override;
};


/*
 * Same idea as Default implementation but it holds a priority queue of tasks and also a communication frequency
 * the communication frequency might also be added to the default impl
 *
 */


template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params MPI_Scheduler_Priority<Prob_Consts, Subproblem_Params, Domain_Type>::Execute(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
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
    Subproblem_Params BestSubproblem;

    if(pid == 0){
        DefaultMasterBehavior(sendstream, receivstream, Problem_Def, prob, encoder, goal, WorstBound);
    }
    else{
        char buffer[2000];
        // local variables of slave
        std::priority_queue<Subproblem_Params, std::vector<Subproblem_Params>, comparator> LocalTaskQueue;
        Domain_Type LocalBestBound = WorstBound;

        int counter = 0;

        while(true){
            MPI_Recv(buffer,2000, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
            receivstream.str(buffer);
            if(st.MPI_TAG == Default::MessageType::PROB){ // is 0 if equal
                //slave has been given a partially solved problem to expand

                Subproblem_Params Received_Params;
                encoder.Decode_Solution(receivstream, Received_Params);
                LocalTaskQueue.push(Received_Params);

                Domain_Type newBoundValue;
                receivstream >> newBoundValue;
                // for debugging it should not be the case that anyone sends a worse bound then what we already have
                if(((bool)goal && newBoundValue >= LocalBestBound)
                   || (!(bool)goal && newBoundValue <= LocalBestBound)){
                    LocalBestBound = newBoundValue;
                }

                while(!LocalTaskQueue.empty()){
                    //take out one element from queue, expand it
                    Subproblem_Params sol = LocalTaskQueue.top(); LocalTaskQueue.pop();
                    //ignore if its bound is worse than already known best sol.
                    Domain_Type LowerBound = std::get<Domain_Type>(Problem_Def.GetLowerBound(prob, sol));
                    if(((bool)goal && LowerBound < LocalBestBound)
                       || (!(bool)goal && LowerBound > LocalBestBound)){
                        continue;
                    }

                    // try to make the bound better only if the solution lies in a feasible domain

                    auto Feasibility = Problem_Def.IsFeasible(prob, sol);
                    Domain_Type CandidateBound;
                    if(Feasibility == BnB::FEASIBILITY::FULL)
                    {
                        CandidateBound = std::get<Domain_Type>(Problem_Def.GetUpperBound(prob, sol));
                        if(((bool)goal && CandidateBound >= LocalBestBound)
                           || (!(bool)goal && CandidateBound <= LocalBestBound)){
                            LocalBestBound = CandidateBound;
                            BestSubproblem = sol;
                        }
                    }else if(Feasibility == BnB::FEASIBILITY::NONE) // basically discard again
                        continue;

                   // printProc("CandidateBound = " << CandidateBound << " LowerBound = " << LowerBound);
                    std::vector<Subproblem_Params> v;
                    if(std::abs(CandidateBound - LowerBound) > this->eps){ // epsilon criterion for convergence
                        v = Problem_Def.SplitSolution(prob, sol);
                        for(const auto& el : v)
                            LocalTaskQueue.push(el);
                    }

                    // request master for slaves
                    if(counter % this->Communication_Frequency == 0)
                    {
                        counter = 0;
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
                            int sl_no;
                            receivstream >> sl_no;
                            sendstream.str("");
                            Subproblem_Params SubProb = LocalTaskQueue.top();
                            LocalTaskQueue.pop();
                            encoder.Encode_Solution(sendstream, SubProb);
                            sendstream << LocalBestBound<< " ";
                            //send it to idle processor
                            MPI_Send(&sendstream.str()[0],strlen(sendstream.str().c_str()),MPI_CHAR,sl_no,Default::MessageType::PROB,MPI_COMM_WORLD);
                        }
                    }
                    counter++;
                }
                //This slave has now become idle (its queue is empty). Inform master.
                sendstream.str("");
                MPI_Send(&sendstream.str()[0],10,MPI_CHAR,0,Default::MessageType::IDLE, MPI_COMM_WORLD);
            }
            else if(st.MPI_TAG == Default::MessageType::FINISH){
                break; // empty solution only master will return a meaningful solution
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
