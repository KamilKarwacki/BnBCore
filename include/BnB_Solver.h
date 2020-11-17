#pragma once
#include "MPI_Message_Encoder.h"
#include "mpi.h"
#include <tuple>
#include <functional>
#include <iostream>
#include <sstream>
#include <any>
#include <variant>
#include <queue>
#include <cstring>

#define printProc(x)  {int __id; \
                     MPI_Comm_rank(MPI_COMM_WORLD, &__id); \
                     std::cout << "proc "<< __id <<": " << x << std::endl;}\




namespace myLib
{
    template<typename... Args>
    using Problem_Constants = std::tuple<Args...>;
    template<typename... Args>
    using Subproblem_Parameters = std::tuple<Args...>; //subproblem parameters
}


// how to store a function that returns any type and the user will specify which type it returns
// use enum??  which will be set to define the domain
// look at type_traits maybe it will help



template<typename Prob_Consts, typename Subproblem_Params>
class Problem_Definition
{
public:
    // get the initial sub-problem, this depends on your problem constant
    std::function<Subproblem_Params               (const Prob_Consts& prob)> GetInitialSubproblem = nullptr;
    std::function<std::vector<Subproblem_Params>  (const Prob_Consts& consts, const Subproblem_Params& params)> SplitSolution = nullptr;
    std::function<bool                            (const Prob_Consts& consts, const Subproblem_Params& params)> IsFeasible = nullptr;
    std::function<std::variant<int, float, double>(const Prob_Consts& consts, const Subproblem_Params& params)> GetBound = nullptr;
    std::function<void                            (const Subproblem_Params& params)> PrintSolution = nullptr;

private:

    // ... other functions ... //
};



template<typename Prob_Consts, typename Subproblem_Params>
class Solver
{
private:
    enum class Goal : bool {
       MAX = true,
       MIN = false
    };

    MPI_Message_Encoder<Subproblem_Params> encoder;
    Goal goal;
    double BestSolution; // should this hold different types

    void Solve(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def, const Prob_Consts& prob);
public:
    void Maximize(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def, const Prob_Consts& prob)
    {
        goal = Goal::MAX;
        BestSolution = std::numeric_limits<double>::lowest()/2.0;
        Solve(Problem_Def, prob);
    }

    void Minimize(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def, const Prob_Consts& prob)
    {
        goal = Goal::MIN;
        BestSolution = std::numeric_limits<double>::max()/2.0;
        Solve(Problem_Def, prob);
    }

};


enum MessageType{ PROB = 0, GET_SLAVES = 1, DONE = 2, IDLE = 3, FINISH = 4};

template<typename Prob_Consts, typename Subproblem_Params>
void Solver<Prob_Consts, Subproblem_Params>::Solve(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
                                                   const Prob_Consts& prob)
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
    Subproblem_Params BestSubpoblem;

    if(pid == 0){
        //master processor
        Subproblem_Params initialProblem = Problem_Def.GetInitialSubproblem(prob);

        bool is_idle[num+1];
        int num_idle;
        for(int i=1;i<num;i++){
            is_idle[i] = true; // mark all as idle
        }
        num_idle = num - 1;
        //encode initial problem and empty sol into buffer
        encoder.Encode_Solution(sendstream, initialProblem);
        sendstream << BestSolution << " ";
        //send it to idle processor no. 1
        MPI_Send(sendstream.str().c_str(),strlen(sendstream.str().c_str()), MPI_CHAR, 1, MessageType::PROB, MPI_COMM_WORLD);
        sendstream.flush();
        is_idle[1] = false;
        num_idle--;
        while(num_idle < num - 1){
            MPI_Recv(buffer, 2000,MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD,&st);
            receivstream.str(buffer);
            if(st.MPI_TAG == (int)MessageType::GET_SLAVES){
                int sl_needed;
                int r = st.MPI_SOURCE;
                receivstream >> sl_needed;
                int sl_given = std::min(sl_needed, num_idle);
                sendstream.str("");
                sendstream << sl_given << " ";
                int i = 1;
                while(sl_given){
                    if(!is_idle[i])
                        i++;
                    else{
                        sendstream << i << " ";
                        is_idle[i] = false;
                        num_idle--;
                        sl_given--;
                    }
                }
                printProc("sending data to slave " << r << "message content: " << std::endl << sendstream.str())
                MPI_Send(&sendstream.str()[0],strlen(sendstream.str().c_str())+1,MPI_CHAR,r,MessageType::GET_SLAVES,MPI_COMM_WORLD);
                sendstream.str("");
            }
            else if(st.MPI_TAG == (int)MessageType::IDLE){
                printProc("setting proc as idle")
                //slave has become idle
                num_idle++;
                is_idle[st.MPI_SOURCE] = true;
            }
            else if(st.MPI_TAG == MessageType::DONE){
                printProc("got a feasible solution")
                //slave has sent a feasible solution
                Subproblem_Params Received_Solution;
                encoder.Decode_Solution(receivstream, Received_Solution);

                if(((bool)goal  && std::get<double>(Problem_Def.GetBound(prob, Received_Solution)) > BestSolution) ||
                   (!(bool)goal && std::get<double>(Problem_Def.GetBound(prob, Received_Solution)) < BestSolution) ){ // optimize with template specialization
                    BestSubpoblem = Received_Solution;
                    BestSolution = std::get<double>(Problem_Def.GetBound(prob, BestSubpoblem));
                }
            }

        }

        for(int i=1;i<num;i++){
            printProc("sending termination message")
            MPI_Send(buffer,1,MPI_CHAR,i,MessageType::FINISH, MPI_COMM_WORLD);
        }
        Problem_Def.PrintSolution(BestSubpoblem);
    }
    else{
        //slave processor
        while(true){
            MPI_Recv(buffer,2000,MPI_CHAR,MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&st);
            receivstream.str(buffer);
            char req[2000];
            if(st.MPI_TAG == MessageType::PROB){ // is 0 if equal
                //slave has been given a partially solved problem to expand
                printProc(receivstream.str())

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
                    if( ((bool)goal && (std::get<double>(Problem_Def.GetBound(prob, sol))<BestSolution))
                        || (!(bool)goal && (std::get<double>(Problem_Def.GetBound(prob, sol))>BestSolution))){
                        continue;
                    }
                    if(Problem_Def.IsFeasible(prob, sol)){
                        printProc("found a feasible solution")
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
                    //master sends num_avbl_slaves <space> best_cost <space> list of slaves
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
                        assert(sl_no != 0);
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
