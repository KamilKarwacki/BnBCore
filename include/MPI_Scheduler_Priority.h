#pragma once
#include "Base.h"
#include "MPI_Scheduler.h"

template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
class MPI_Scheduler_Priority: public MPI_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type>
{
public:
    enum MessageType{PROB = 0, GET_SLAVES = 1, DONE = 2, IDLE = 3, FINISH = 4};

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

    char buffer[2000];

    if(pid == 0){
        int NumMessages = 0;
        // local Variables of master
        Subproblem_Params BestSubproblem;
        Domain_Type GlobalBestBound = WorstBound;
        // -------------

        Subproblem_Params initialProblem = Problem_Def.GetInitialSubproblem(prob);
        // stores idleProcIds;
        std::vector<int> idleProcIds;
        for(int i=2;i<num;i++){
            idleProcIds.push_back(i);
        }
        // encode initial problem and empty sol into sendstream
        encoder.Encode_Solution(sendstream, initialProblem);
        sendstream << GlobalBestBound << " ";
        // send it to idle processor no. 1
        MPI_Send(sendstream.str().c_str(),strlen(sendstream.str().c_str()), MPI_CHAR, 1, MessageType::PROB, MPI_COMM_WORLD);
        while(idleProcIds.size() != num - 1){
            MPI_Recv(buffer, 2000,MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
            NumMessages++;
            receivstream.str(buffer);
            if(st.MPI_TAG == MessageType::GET_SLAVES){
                int sl_needed;
                int r = st.MPI_SOURCE;

                Domain_Type CandidateBound;
                receivstream >> CandidateBound;

                if(((bool)goal && CandidateBound > GlobalBestBound) ||
                   (!(bool)goal && CandidateBound < GlobalBestBound) ){ // optimize with template specialization
                    GlobalBestBound = CandidateBound;
                }

                receivstream >> sl_needed;
                int sl_given = std::min(sl_needed, (int)idleProcIds.size());;
                sendstream.str("");
                sendstream << GlobalBestBound << " ";
                sendstream << sl_given << " ";

                std::for_each(idleProcIds.begin(), idleProcIds.begin() + sl_given,
                              [&sendstream](const int& el){sendstream << el << " ";});

                MPI_Send(&sendstream.str()[0],strlen(sendstream.str().c_str())+1,MPI_CHAR,r,MessageType::GET_SLAVES,MPI_COMM_WORLD);

                idleProcIds.erase(idleProcIds.begin(), idleProcIds.begin() + sl_given);

            }
            else if(st.MPI_TAG == MessageType::IDLE){
                //slave has become idle
                idleProcIds.push_back(st.MPI_SOURCE);
            }
            else if(st.MPI_TAG == MessageType::DONE){
                //slave has sent a feasible solution
                Subproblem_Params Received_Solution;
                encoder.Decode_Solution(receivstream, Received_Solution);
                Domain_Type CandidateBound;
                receivstream >> CandidateBound;
               // printProc("received solution with bound: " << CandidateBound << " while best Bound is " << GlobalBestBound);

                if(((bool)goal && CandidateBound >= GlobalBestBound) ||
                   (!(bool)goal && CandidateBound <= GlobalBestBound) ){ // optimize with template specialization
                    BestSubproblem  = Received_Solution;
                    GlobalBestBound = CandidateBound;
                }
            }
        }

        for(int i=1;i<num;i++){
            MPI_Send(buffer, 1, MPI_CHAR, i, MessageType::FINISH, MPI_COMM_WORLD);
        }
        printProc("number of messages needed " << NumMessages);
        Problem_Def.PrintSolution(BestSubproblem);
        return BestSubproblem;
    }
    else{
        // local variables of slave
        std::priority_queue<Subproblem_Params, std::vector<Subproblem_Params>, comparator> LocalTaskQueue;
        Domain_Type LocalBestBound = WorstBound;

        int counter = 0;

        while(true){
            MPI_Recv(buffer,2000, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
            receivstream.str(buffer);
            if(st.MPI_TAG == MessageType::PROB){ // is 0 if equal
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
                    if(Problem_Def.Discard(prob, sol, LocalBestBound))
                        continue;

                    // try to make the bound better only if the solution lies in a feasible domain
                    bool SendToMaster = false;
                    auto Feasibility = Problem_Def.IsFeasible(prob, sol);
                    if(Feasibility == BnB::FEASIBILITY::FULL)
                    {
                        auto CandidateBound = std::get<Domain_Type>(Problem_Def.GetBound(prob, sol));
                        if(((bool)goal && CandidateBound >= LocalBestBound)
                           || (!(bool)goal && CandidateBound <= LocalBestBound)){
                            LocalBestBound = CandidateBound;
                            SendToMaster = true;
                        }
                    }else if(Feasibility == BnB::FEASIBILITY::NONE) // basically discard again
                        continue;

                    std::vector<Subproblem_Params> v;
                    if(Problem_Def.IsBranchable(prob, sol)){ // if we can branch we do it
                        v = Problem_Def.SplitSolution(prob, sol);
                        for(const auto& el : v)
                            LocalTaskQueue.push(el);
                    }
                    else if (SendToMaster) // if we cant and we have a good solution we send it to master
                    {
                        sendstream.str("");
                        encoder.Encode_Solution(sendstream, sol);
                        sendstream << LocalBestBound << " ";
                        //tell master that feasible solution is reached
                        MPI_Send(&sendstream.str()[0],strlen(sendstream.str().c_str())+1, MPI_CHAR,0,MessageType::DONE, MPI_COMM_WORLD);
                        continue;
                    }

                    // request master for slaves
                    if(counter % this->Communication_Frequency == 0)
                    {
                        counter = 0;
                        sendstream.str("");
                        sendstream << LocalBestBound << " ";
                        sendstream << (int)LocalTaskQueue.size() << " ";
                        MPI_Send(&sendstream.str()[0],strlen(sendstream.str().c_str()),MPI_CHAR,0,MessageType::GET_SLAVES,MPI_COMM_WORLD);

                        //get master's response
                        MPI_Recv(buffer,200, MPI_CHAR, 0, MessageType::GET_SLAVES, MPI_COMM_WORLD, &st);
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
                            MPI_Send(&sendstream.str()[0],strlen(sendstream.str().c_str()),MPI_CHAR,sl_no,MessageType::PROB,MPI_COMM_WORLD);
                        }
                    }
                    counter++;
                }
                //This slave has now become idle (its queue is empty). Inform master.
                sendstream.str("");
                MPI_Send(&sendstream.str()[0],10,MPI_CHAR,0,MessageType::IDLE, MPI_COMM_WORLD);
            }
            else if(st.MPI_TAG == MessageType::FINISH){
                assert(st.MPI_SOURCE==0); // only master can tell it to finish
                return Subproblem_Params(); // empty solution only master will return a meaningful solution
                break; //from the while(1) loop
            }
        }
    }
}
