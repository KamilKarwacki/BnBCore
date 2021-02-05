#pragma once

#include <iomanip>
#include "mpi.h"
#include "MPI_Scheduler.h"

namespace Default
{
    enum MessageType{PROB = 0, GET_SLAVES = 1, DONE = 2, IDLE = 3, FINISH = 4};
}

template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
class MPI_Scheduler_Default : public MPI_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type>
{
public:

    Subproblem_Params Execute(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
                 const Prob_Consts& prob,
                 const MPI_Message_Encoder<Subproblem_Params>& encoder,
                 const Goal goal,
                 const Domain_Type WorstBound) override;
};


/*
 * Master:
 * send initial problem
 *      while(procs not all idle)
 *      receive Message
 *      case Slave Request:
 *          master gets local bound and number of slaves needed
 *          he updated his new bound and sends the new one to the slave together with ids of the procs
 *
 *      case Processor is Idle:
 *          set as idle
 *
 *      case Processor found a solution which is not divisible anymore
 *          bound is received and checked against the current best bound
 *          overwritten if needed
 *
 * Slave:
 *      while(not terminated)
 *          receive Message
 *          case New Problem:
 *              push it to queue and update bound if needed
 *              if we found a solution which cant be expanded we send it to master
 *              but only of it also has the best LocalBound currently
 *              expand the problem and ask the master for slaves
 *
 */
//for(typename std::vector<Subproblem_Params>::reverse_iterator i = v.rbegin();
//		     i != v.rend(); ++i ) {
//	task_queue.push(*i);
//}

template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params DefaultMasterBehavior(std::stringstream& sendstream,
                                        std::stringstream& receivstream,
                                        const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
                                        const Prob_Consts& prob,
                                        const MPI_Message_Encoder<Subproblem_Params>& encoder,
                                        const Goal goal,
                                        const Domain_Type WorstBound)
{
    char buffer[2000];
    int pid, num;
    MPI_Comm_rank(MPI_COMM_WORLD, &pid);
    MPI_Comm_size(MPI_COMM_WORLD, &num);
    MPI_Status st;

    int NumMessages = 0;
    printProc("the domain is " << std::get<0>(prob).size() << " dimensional and the domain is " << std::get<0>(prob)[0]);

    Domain_Type GlobalBestBound = WorstBound;
    //master processor
    Subproblem_Params BestSubproblem = Problem_Def.GetInitialSubproblem(prob);

    // stores idleProcIds;
    std::vector<int> idleProcIds;
    for(int i=2;i<num;i++){
        idleProcIds.push_back(i);
    }
    // encode initial problem and empty sol into sendstream
    encoder.Encode_Solution(sendstream, BestSubproblem);
    sendstream << GlobalBestBound << " ";
    // send it to idle processor no. 1
    MPI_Send(sendstream.str().c_str(),strlen(sendstream.str().c_str()), MPI_CHAR, 1, Default::MessageType::PROB, MPI_COMM_WORLD);
    while(idleProcIds.size() != num - 1){
        MPI_Recv(buffer, 2000,MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
        NumMessages++;
        receivstream.str(buffer);
        if(st.MPI_TAG == Default::MessageType::GET_SLAVES){
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

            MPI_Send(&sendstream.str()[0],strlen(sendstream.str().c_str())+1,MPI_CHAR,r,Default::MessageType::GET_SLAVES,MPI_COMM_WORLD);

            idleProcIds.erase(idleProcIds.begin(), idleProcIds.begin() + sl_given);
        }
        else if(st.MPI_TAG == Default::MessageType::IDLE){
            //slave has become idle
            idleProcIds.push_back(st.MPI_SOURCE);
        }
    }

    for(int i=1;i<num;i++){
        MPI_Send(buffer, 1, MPI_CHAR, i, Default::MessageType::FINISH, MPI_COMM_WORLD);
    }
    printProc("the master received a total of " << NumMessages << " messages");
    return BestSubproblem;
}


template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params ExtractBestSolution(std::stringstream& sendstream, std::stringstream& receivstream,
                                      Subproblem_Params BestSubproblem,
                                      const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
                                      const Prob_Consts& prob,
                                      const MPI_Message_Encoder<Subproblem_Params>& encoder,
                                      const Goal goal)
{
    int pid, num;
    MPI_Comm_rank(MPI_COMM_WORLD, &pid);
    MPI_Comm_size(MPI_COMM_WORLD, &num);
    MPI_Status st;
    if(pid == 0)
    {
        char buf[1000];
        Subproblem_Params GlobalSolution = BestSubproblem;
        Domain_Type GlobalBound = std::get<Domain_Type>(Problem_Def.GetUpperBound(prob, GlobalSolution));
        for(int i = 1; i < num; i++)
        {
            MPI_Recv(buf, 1000, MPI_CHAR, i, 100, MPI_COMM_WORLD, &st);
            receivstream.str(buf);
            Subproblem_Params Subproblem;
            encoder.Decode_Solution(receivstream, Subproblem);
            Domain_Type temp = std::get<Domain_Type>(Problem_Def.GetUpperBound(prob, Subproblem));
            if(((bool)goal && temp >= GlobalBound)
               || (!(bool)goal && temp <= GlobalBound)){
                GlobalSolution= Subproblem;
                GlobalBound = temp;
            }
        }
        Problem_Def.PrintSolution(GlobalSolution);
        return GlobalSolution;
    }else{
        sendstream.str("");
        encoder.Encode_Solution(sendstream, BestSubproblem);
        //send it to idle processor
        MPI_Send(&sendstream.str()[0],strlen(sendstream.str().c_str()), MPI_CHAR, 0,
                 100, MPI_COMM_WORLD);
        return BestSubproblem;
    }
}



template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params MPI_Scheduler_Default<Prob_Consts, Subproblem_Params, Domain_Type>::Execute(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
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
        //slave processor
        std::queue<Subproblem_Params> LocalTaskQueue;
        Domain_Type LocalBestBound = WorstBound;

        int counter = 0;
        while(true){
            MPI_Recv(buffer,2000, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD,&st);
            receivstream.str(buffer);
            if(st.MPI_TAG == Default::MessageType::PROB){ // is 0 if equal
                //slave has been given a partially solved problem to expand

                Subproblem_Params Received_Params;
                encoder.Decode_Solution(receivstream, Received_Params);
                LocalTaskQueue.push(Received_Params);

                Domain_Type newBoundValue;
                receivstream >> newBoundValue;
                if(((bool)goal && newBoundValue > LocalBestBound)
                   || (!(bool)goal && newBoundValue < LocalBestBound)){
                    LocalBestBound = newBoundValue;
                }

                while(!LocalTaskQueue.empty()){
                    //take out one element from queue, expand it
                    Subproblem_Params sol = LocalTaskQueue.front(); LocalTaskQueue.pop();

                    //ignore if its bound is worse than already known best sol.
                    // make it return two bounds if we are not feasible these two bounds are used for branching criterion
                    Domain_Type LowerBound = std::get<Domain_Type>(Problem_Def.GetLowerBound(prob, sol));
                    if(((bool)goal && LowerBound < LocalBestBound)
                       || (!(bool)goal && LowerBound > LocalBestBound)){
                        continue;
                    }

                    // try to make the bound better only if the solution lies in a feasible domain
                    auto Feasibility = Problem_Def.IsFeasible(prob, sol);
                    Domain_Type CandidateBound;
                    bool IsCandidateSolution = false;
                    if(Feasibility == BnB::FEASIBILITY::FULL)
                    {
                        CandidateBound = std::get<Domain_Type>(Problem_Def.GetUpperBound(prob, sol));
                        if(((bool)goal && CandidateBound >= LocalBestBound)
                           || (!(bool)goal && CandidateBound <= LocalBestBound)){
                            LocalBestBound = CandidateBound;
                            IsCandidateSolution = true;
                        }
                    }else if(Feasibility == BnB::FEASIBILITY::NONE) // basically discard again
                        continue;

                    std::vector<Subproblem_Params> v;
                    //printProc("upper bound is " << CandidateBound << " and lower is " << LowerBound);
                    if(std::abs(CandidateBound - LowerBound) > this->eps){ // epsilon criterion for convergence
                        v = Problem_Def.SplitSolution(prob, sol);
                        for(const auto& el : v)
                            LocalTaskQueue.push(el);
                    }else if(IsCandidateSolution)
                        BestSubproblem = sol;

                    //request master for slaves and exchange bound
                    if(counter % this->Communication_Frequency == 0 and !LocalTaskQueue.empty()) // maybe set a variable
                    {
                        counter = 0;
                        sendstream.str("");
                        sendstream << LocalBestBound << " ";
                        sendstream << (int)LocalTaskQueue.size() << " ";
                        MPI_Send(&sendstream.str()[0],strlen(sendstream.str().c_str()),MPI_CHAR,0,Default::MessageType::GET_SLAVES,MPI_COMM_WORLD);

                        //get master's response
                        MPI_Recv(buffer,200, MPI_CHAR, 0, Default::MessageType::GET_SLAVES, MPI_COMM_WORLD, &st);
                        receivstream.str(buffer);

                        receivstream >> LocalBestBound;
                        int slaves_avbl;
                        receivstream >> slaves_avbl;

                        for(int i=0; i<slaves_avbl; i++){
                            //give a problem to each slave
                            int sl_no;
                            receivstream >> sl_no;
                            sendstream.str("");
                            Subproblem_Params SubProb = LocalTaskQueue.front();
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
