#pragma once
#include "mpi.h"
#include "MPI_Scheduler.h"

template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
class MPI_Scheduler_Default : public MPI_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type>
{
public:
    void Execute(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
                 const Prob_Consts& prob,
                 const MPI_Message_Encoder<Subproblem_Params>& encoder,
                 const Goal goal) override;
};


enum MessageType{PROB = 0, GET_SLAVES = 1, DONE = 2, IDLE = 3, FINISH = 4};

template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
void MPI_Scheduler_Default<Prob_Consts, Subproblem_Params, Domain_Type>::Execute(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
                                                                                 const Prob_Consts& prob,
                                                                                 const MPI_Message_Encoder<Subproblem_Params>& encoder,
                                                                                 const Goal goal)
{
    MPI_Init(0,NULL);
    int pid, num;
    MPI_Comm_rank(MPI_COMM_WORLD, &pid);
    MPI_Comm_size(MPI_COMM_WORLD, &num);
    assert(num >= 3 && "this implementation needs at least 3 cores");
    MPI_Status st;
    std::stringstream sendstream("");
    std::stringstream receivstream("");
    std::queue<Subproblem_Params> task_queue;

    char buffer[2000];
    Subproblem_Params BestSubproblem;
    Domain_Type BestSolution;

    if(pid == 0){
        //master processor
        Subproblem_Params initialProblem = Problem_Def.GetInitialSubproblem(prob);

        // stores idleProcIds;
        std::vector<int> idleProcIds;
        for(int i=2;i<num;i++){
            idleProcIds.push_back(i);
        }
        //encode initial problem and empty sol into buffer
        encoder.Encode_Solution(sendstream, initialProblem);
        sendstream << BestSolution << " ";
        //send it to idle processor no. 1
        MPI_Send(sendstream.str().c_str(),strlen(sendstream.str().c_str()), MPI_CHAR, 1, MessageType::PROB, MPI_COMM_WORLD);
        while(idleProcIds.size() != num - 1){ //NumOfIdleProcs < num - 1) {  //num_idle < num - 1){
            MPI_Recv(buffer, 2000,MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
            receivstream.str(buffer);
            if(st.MPI_TAG == (int)MessageType::GET_SLAVES){
                int sl_needed;
                int r = st.MPI_SOURCE;
                receivstream >> sl_needed;
                int sl_given = std::min(sl_needed, (int)idleProcIds.size());//)num_idle);
                sendstream.str("");
                sendstream << sl_given << " ";

                std::for_each(idleProcIds.begin(), idleProcIds.begin() + sl_given,
                        [&sendstream](const int& el){sendstream << el << " ";});

                printProc("sending data to slave " << r << "message content: " << std::endl << sendstream.str())
                MPI_Send(&sendstream.str()[0],strlen(sendstream.str().c_str())+1,MPI_CHAR,r,MessageType::GET_SLAVES,MPI_COMM_WORLD);

                idleProcIds.erase(idleProcIds.begin(), idleProcIds.begin() + sl_given);
            }
            else if(st.MPI_TAG == (int)MessageType::IDLE){
                printProc("setting proc as idle")
                //slave has become idle
                idleProcIds.push_back(st.MPI_SOURCE);
            }
            else if(st.MPI_TAG == MessageType::DONE){
                printProc("got a feasible solution")
                //slave has sent a feasible solution
                Subproblem_Params Received_Solution;
                encoder.Decode_Solution(receivstream, Received_Solution);

                if(((bool)goal && std::get<Domain_Type>(Problem_Def.GetBound(prob, Received_Solution)) > BestSolution) ||
                  (!(bool)goal && std::get<Domain_Type>(Problem_Def.GetBound(prob, Received_Solution)) < BestSolution) ){ // optimize with template specialization
                    BestSubproblem = Received_Solution;
                    BestSolution = std::get<Domain_Type>(Problem_Def.GetBound(prob, BestSubproblem));
                }
            }

        }

        for(int i=1;i<num;i++){
            printProc("sending termination message")
            MPI_Send(buffer,1,MPI_CHAR,i,MessageType::FINISH, MPI_COMM_WORLD);
        }
        Problem_Def.PrintSolution(BestSubproblem);
    }
    else{
        //slave processor
        while(true){
            MPI_Recv(buffer,2000,MPI_CHAR,MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&st);
            receivstream.str(buffer);
            char req[2000];
            if(st.MPI_TAG == MessageType::PROB){ // is 0 if equal
                //slave has been given a partially solved problem to expand
                printProc("Received message with buffer " << receivstream.str())

                Subproblem_Params Received_Params;
                encoder.Decode_Solution(receivstream, Received_Params);
                task_queue.push(Received_Params);
                receivstream >> BestSolution;
                receivstream.str(""); // can be commented out
                while(!task_queue.empty()){
                    //take out one element from queue, expand it
                    Subproblem_Params sol = task_queue.front();
                    task_queue.pop();
                    //ignore if its bound is worse than already known best sol.
                    if( ((bool)goal && (std::get<Domain_Type>(Problem_Def.GetBound(prob, sol))<BestSolution))
                        || (!(bool)goal && (std::get<Domain_Type>(Problem_Def.GetBound(prob, sol))>BestSolution))){
                        continue;
                    }
                    if(Problem_Def.IsFeasible(prob, sol)){
                        sendstream.str("");
                        encoder.Encode_Solution(sendstream, sol);
                        printProc("decoded sol");
                        //tell master that feasible solution is reached
                        MPI_Send(&sendstream.str()[0],strlen(sendstream.str().c_str())+1, MPI_CHAR,0,MessageType::DONE, MPI_COMM_WORLD);
                        sendstream.str("");
                        continue;
                    }
                    std::vector<Subproblem_Params> v = Problem_Def.SplitSolution(prob, sol);

                    for(int i=0;i<v.size();i++)
                        task_queue.push(v[i]);
                    if(task_queue.empty())
                        break;
                    //request master for slaves
                    sendstream.str("");
                    sendstream << (int)task_queue.size() << " ";
                    printProc("sending the master a request for " << task_queue.size() << " slaves");
                    MPI_Send(&sendstream.str()[0],strlen(sendstream.str().c_str()),MPI_CHAR,0,MessageType::GET_SLAVES,MPI_COMM_WORLD);

                    //get master's response
                    MPI_Recv(buffer,200, MPI_CHAR,0,MessageType::GET_SLAVES, MPI_COMM_WORLD,&st);
                    receivstream.str(buffer);
                    printProc("received data from master " << receivstream.str())
                    int slaves_avbl;
                    receivstream >> slaves_avbl;
                    printProc(slaves_avbl);

                    for(int i=0; i<slaves_avbl; i++){
                        //give a problem to each slave
                        int sl_no;
                        receivstream >> sl_no;
                        printProc("will send new problem to " << sl_no)
                        sendstream.str("");
                        Subproblem_Params SubProb = task_queue.front();
                        task_queue.pop();
                        encoder.Encode_Solution(sendstream, SubProb);
                        sendstream << BestSolution << " ";
                        //send it to idle processor
                        printProc("sending subProblem to proc " << sl_no)
                        MPI_Send(&sendstream.str()[0],strlen(sendstream.str().c_str()),MPI_CHAR,sl_no,MessageType::PROB,MPI_COMM_WORLD);
                    }
                }
                //This slave has now become idle (its queue is empty). Inform master.
                printProc("im Idle sending notification to master")
                sendstream.str("");
                MPI_Send(&sendstream.str()[0],10,MPI_CHAR,0,MessageType::IDLE, MPI_COMM_WORLD);
            }
            else if(st.MPI_TAG == MessageType::FINISH){
                printProc("i got termination message from master")
                assert(st.MPI_SOURCE==0); // only master can tell it to finish
                break; //from the while(1) loop
            }
        }
    }
    MPI_Finalize();
}
