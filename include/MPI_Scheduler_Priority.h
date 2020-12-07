#include <set>
#include "Base.h"
#include "MPI_Scheduler.h"

template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
class MPI_Scheduler_Priority: public MPI_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type>
{
public:
    enum MessageType{PROB = 0, BOUNDEXCHANGE = 1, DONE = 2, IDLE = 3, FINISH = 4, NONEWBOUND = 5};

    void Execute(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
                 const Prob_Consts& prob,
                 const MPI_Message_Encoder<Subproblem_Params>& encoder,
                 const Goal goal) override;
};



template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
void MPI_Scheduler_Priority<Prob_Consts, Subproblem_Params, Domain_Type>::Execute(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
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

    char buffer[2000];
    Subproblem_Params BestSubproblem;
    Domain_Type BestSolution;

    if(pid == 0){
        std::queue<Subproblem_Params> master_problem_queue;
        std::vector<int> IdleProcs;
        std::set<int> BusyProcs;
        //master processor
        master_problem_queue.push(Problem_Def.GetInitialSubproblem(prob));

        // generate problems until we have enough for every sub problem
        while(master_problem_queue.size() < num)
        {
            auto prob_to_be_split = master_problem_queue.back();
            auto temp = Problem_Def.SplitSolution(prob, prob_to_be_split);
            std::for_each(temp.begin(), temp.end(),
                    [&master_problem_queue](const auto& el){master_problem_queue.push(el);});

        }

        // now send them to procs
        for(int i = 1; i < num; i++)
        {
            sendstream.str("");
            encoder.Encode_Solution(sendstream, master_problem_queue.front());
            master_problem_queue.pop();
            sendstream << BestSolution << " ";
            MPI_Send(sendstream.str().c_str(),strlen(sendstream.str().c_str()), MPI_CHAR, i, MessageType::PROB, MPI_COMM_WORLD);
            BusyProcs.insert(i);
        }


        while(IdleProcs.size() != num - 1){
            MPI_Recv(buffer, 2000, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
            receivstream.str(buffer);
            if(st.MPI_TAG == MessageType::DONE){ // ----------------------------------------------------------------------------------
                // slave doesnt have any more tasks
                Subproblem_Params Received_Solution;
                encoder.Decode_Solution(receivstream, Received_Solution);

                Domain_Type slavesBound;
                receivstream >> slavesBound;

                if(((bool)goal && slavesBound > BestSolution) ||
                  (!(bool)goal && slavesBound < BestSolution) ){ // optimize with template specialization
                    BestSubproblem = Received_Solution;
                    BestSolution = slavesBound;
                }

                IdleProcs.push_back(st.MPI_SOURCE);
                BusyProcs.erase(st.MPI_SOURCE);

                if(!BusyProcs.empty())
                {
                    int ProcThatHasWork = *BusyProcs.begin();

                    sendstream.str("");
                    sendstream << " ";
//                    MPI_Send(buffer, strlen(sendstream.str().c_str()), MPI_CHAR, ProcThatHasWork, MessageType::SENDWORK, MPI_COMM_WORLD);
                }



            }
            else if(st.MPI_TAG == MessageType::BOUNDEXCHANGE) // -----------------------------------------------------------------------
            {
                // master and slave communicate the best bound
                Domain_Type slavesBound;
                receivstream >> slavesBound;
                sendstream.str("");

                if(((bool)goal && slavesBound > BestSolution) ||
                   (!(bool)goal && slavesBound < BestSolution) ){
                    BestSolution = slavesBound;
                    MPI_Send(sendstream.str().c_str(),strlen(sendstream.str().c_str()), MPI_CHAR, st.MPI_SOURCE, MessageType::NONEWBOUND, MPI_COMM_WORLD);
                } else{
                    sendstream << BestSolution;
                    MPI_Send(sendstream.str().c_str(),strlen(sendstream.str().c_str()), MPI_CHAR, st.MPI_SOURCE, MessageType::BOUNDEXCHANGE, MPI_COMM_WORLD);
                }
            } // ------------------------------------------------------------------------------------------------------------------------
        }

        for(int i=1;i<num;i++){
            printProc("sending termination message")
            MPI_Send(buffer,1,MPI_CHAR,i,MessageType::FINISH, MPI_COMM_WORLD);
        }
        Problem_Def.PrintSolution(BestSubproblem);
    }
    else {
        //slave processor
        std::queue<Subproblem_Params> local_task_queue;
        // receiv initial problem
        MPI_Recv(buffer, 2000, MPI_CHAR, 0, (int)MessageType::PROB, MPI_COMM_WORLD, &st);
        receivstream.str(buffer);
        char req[2000];
        int askForBound = 10;
        int counter = 0;

        Subproblem_Params Received_Params;
        encoder.Decode_Solution(receivstream, Received_Params);
        local_task_queue.push(Received_Params);
        receivstream >> BestSolution;


        while (!local_task_queue.empty()) {
            counter++;
            if (askForBound % counter == 0) {
                sendstream.str("");
                encoder.Encode_Solution(sendstream, BestSubproblem);
                sendstream << BestSolution << " ";
                MPI_Send(&sendstream.str()[0], strlen(sendstream.str().c_str()) + 1, MPI_CHAR, 0,
                         MessageType::BOUNDEXCHANGE, MPI_COMM_WORLD);
                MPI_Recv(buffer, 2000, MPI_CHAR, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
                if (st.MPI_TAG == MessageType::BOUNDEXCHANGE) {
                    receivstream.str(buffer);
                    receivstream >> BestSolution;
                }

            }

            //take out one element from queue, expand it
            Subproblem_Params sol = local_task_queue.front();
            local_task_queue.pop();
            //ignore if its bound is worse than already known best sol.
            if (((bool) goal && (std::get<Domain_Type>(Problem_Def.GetBound(prob, sol)) < BestSolution))
                || (!(bool) goal && (std::get<Domain_Type>(Problem_Def.GetBound(prob, sol)) > BestSolution))) {
                continue;
            }

            if (Problem_Def.IsFeasible(prob, sol)) {
                continue;
            }

            std::vector<Subproblem_Params> v = Problem_Def.SplitSolution(prob, sol);

            for (int i = 0; i < v.size(); i++)
                local_task_queue.push(v[i]);
            //This slave has now become idle (its queue is empty). Inform master.
            printProc("im Idle sending notification to master")
            sendstream.str("");
            MPI_Send(&sendstream.str()[0], 10, MPI_CHAR, 0, MessageType::IDLE, MPI_COMM_WORLD);

        }
    }
    MPI_Finalize();
}
