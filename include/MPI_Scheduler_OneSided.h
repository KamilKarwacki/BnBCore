#pragma once
#include "Base.h"
#include "MPI_Scheduler.h"

template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
class MPI_Scheduler_OneSided: public MPI_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type>
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
 * The main problem with the other implementations is that every n iterations there is a blocking communication with the master
 * this implementation works differently
 * the master contacts the slaves, the salves just listen using a probe (Work Stealing model)
 * furthermore, the processors exchange their best solutions using one sided communication
 */



/*
 *
 *
 *
 *
 *
 */


template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params MPI_Scheduler_OneSided<Prob_Consts, Subproblem_Params, Domain_Type>::Execute(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
                                                                                  const Prob_Consts& prob,
                                                                                  const MPI_Message_Encoder<Subproblem_Params>& encoder,
                                                                                  const Goal goal,
                                                                                  const Domain_Type WorstBound)
{
    int pid, num;
    MPI_Comm Slaves_Comm;
    MPI_Comm_rank(MPI_COMM_WORLD, &pid);
    MPI_Comm_size(MPI_COMM_WORLD, &num);
    MPI_Comm_split(MPI_COMM_WORLD, pid > 0, pid, &Slaves_Comm);
    int slaveID;
    MPI_Comm_rank(Slaves_Comm, &slaveID);
    assert(num >= 3 && "this implementation needs at least 3 cores");
    MPI_Status st;
    std::stringstream sendstream("");
    sendstream << std::setprecision(15);
    std::stringstream receivstream("");
    receivstream << std::setprecision(15);

    char buffer[2000];
    int NumMessages = 0;

    if(pid == 0){
        // local Variables of master
        Subproblem_Params BestSubproblem;
        Domain_Type GlobalBestBound = WorstBound;
        // -------------

        Subproblem_Params initialProblem = Problem_Def.GetInitialSubproblem(prob);
        // stores idleProcIds;
        std::vector<int> workingProcs;
        workingProcs.push_back(1);
        // encode initial problem and empty sol into sendstream
        encoder.Encode_Solution(sendstream, initialProblem);
        sendstream << GlobalBestBound << " ";
        // send it to idle processor no. 1
        MPI_Send(sendstream.str().c_str(), strlen(sendstream.str().c_str()), MPI_CHAR, 1, MessageType::PROB, MPI_COMM_WORLD);
        while(!workingProcs.empty()){
            MPI_Recv(buffer, 2000, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
            NumMessages++;
            receivstream.str(buffer);
            if(st.MPI_TAG == MessageType::IDLE){ // if a proc is idle we find a proc that has work and send him a request
                printProc("received idle message")
                workingProcs.erase(std::remove(workingProcs.begin(), workingProcs.end(), (int)st.MPI_SOURCE), workingProcs.end());
                if(workingProcs.size() != num){ // maybe randomize
                    printProc("sending a working process a request")
                    sendstream.str("");
                    sendstream << (int)st.MPI_SOURCE << std::endl;
                    MPI_Send(&sendstream.str()[0], strlen(sendstream.str().c_str()), MPI_CHAR,
                            workingProcs[0], MessageType::GET_SLAVES, MPI_COMM_WORLD);
                }
            }
            else if(st.MPI_TAG == MessageType::DONE){
                //slave has sent a feasible solution
                Subproblem_Params Received_Solution;
                encoder.Decode_Solution(receivstream, Received_Solution);
                Domain_Type CandidateBound;
                receivstream >> CandidateBound;

                if(((bool)goal && CandidateBound > GlobalBestBound) ||
                   (!(bool)goal && CandidateBound < GlobalBestBound) ){ // optimize with template specialization
                    BestSubproblem  = Received_Solution;
                    GlobalBestBound = CandidateBound;
                }
            }
        }

        for(int i=1;i<num;i++){
            MPI_Send(buffer, 1, MPI_CHAR, i, MessageType::FINISH, MPI_COMM_WORLD);
        }
        Problem_Def.PrintSolution(BestSubproblem);
        printProc("we achieved the goal after " << NumMessages << " messages")
        return BestSubproblem;
    }
    else{
        // local variables of slave
        std::priority_queue<Subproblem_Params, std::vector<Subproblem_Params>, comparator> LocalTaskQueue;
        Domain_Type LocalBestBound = WorstBound;
        MPI_Win boundWindow;
        MPI_Win_create(&LocalBestBound, sizeof(Domain_Type), sizeof(Domain_Type), MPI_INFO_NULL, Slaves_Comm, &boundWindow);
        MPI_Win_fence(0, boundWindow);

        int counter = 0;
        while(true){
            MPI_Recv(buffer,2000, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
            receivstream.str(buffer);
            if(st.MPI_TAG == MessageType::PROB){
                //slave has been given a partially solved problem to expand
                printProc("got a task from proc " << st.MPI_SOURCE)

                Subproblem_Params Received_Params;
                encoder.Decode_Solution(receivstream, Received_Params);
                LocalTaskQueue.push(Received_Params);

                Domain_Type newBoundValue;
                receivstream >> newBoundValue;
                // for debugging it should not be the case that anyone sends a worse bound then what we already have
                if(((bool)goal && newBoundValue > LocalBestBound)
                   || (!(bool)goal && newBoundValue < LocalBestBound)){
                    LocalBestBound = newBoundValue;
                }

                int masterAsksForWork = false;
                while(!LocalTaskQueue.empty()){
                    MPI_Iprobe(0, MessageType::GET_SLAVES, MPI_COMM_WORLD, &masterAsksForWork, &st); // test if master has sent me a request
                    if(masterAsksForWork)
                    {
                        printProc("sending an idle proc some work");
                        // get masters request for work
                        MPI_Recv(buffer, 200, MPI_CHAR, 0, MessageType::GET_SLAVES, MPI_COMM_WORLD, &st);
                        receivstream.str(buffer);
                        int sl_no; receivstream >> sl_no;
                        sendstream.str("");
                        Subproblem_Params SubProb = LocalTaskQueue.top();
                        LocalTaskQueue.pop();
                        encoder.Encode_Solution(sendstream, SubProb);
                        sendstream << LocalBestBound<< " ";
                        //send it to idle processor
                        MPI_Send(&sendstream.str()[0],strlen(sendstream.str().c_str()),MPI_CHAR,sl_no,MessageType::PROB,MPI_COMM_WORLD);
                    }

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

                    // synchronize bounds
                    if(counter % this->Communication_Frequency == 0)
                    {
                        for(int i = 0; i < num - 1; i++)
                        {
                            Domain_Type CandidateBound;
                            MPI_Datatype type;
                            if(slaveID == i)
                                continue;
                            if(typeid(Domain_Type) == typeid(int))
                                type = MPI_INT;
                            else if(typeid(Domain_Type) == typeid(float))
                                type = MPI_FLOAT;
                            else if(typeid(Domain_Type) == typeid(double))
                                type = MPI_INT;
                            else
                                assert(false);
                            MPI_Get(&CandidateBound, 1, type, i, 0, 1, type, boundWindow);
                            if(((bool)goal && CandidateBound > LocalBestBound)
                               || (!(bool)goal && CandidateBound < LocalBestBound)){
                                LocalBestBound = CandidateBound;
                            }
                        }
                        counter = 0;
                    }
                    counter++;
                }
                //This slave has now become idle (its queue is empty). Inform master.
                sendstream.str("");
                MPI_Send(&sendstream.str()[0],10,MPI_CHAR,0,MessageType::IDLE, MPI_COMM_WORLD);
            }
            else if(st.MPI_TAG == MessageType::FINISH)
            {
                MPI_Win_fence(0, boundWindow);
                MPI_Win_free(&boundWindow);
                return Subproblem_Params();
            }

        }
    }

}
