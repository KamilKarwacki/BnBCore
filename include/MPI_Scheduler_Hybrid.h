#pragma once
#include "Base.h"
#include "MPI_Scheduler.h"
#include <unistd.h>

template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
class MPI_Scheduler_Hybrid: public MPI_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type>
{
public:
    enum MessageType{PROB = 0, GET_SLAVES = 1, DONE = 2, IDLE = 3, FINISH = 4};

    struct comparator{
        bool operator()(const Subproblem_Params& p1, const Subproblem_Params& p2){
            return std::get<0>(p1) < std::get<0>(p2);
        }
    };


    void Execute(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
                 const Prob_Consts& prob,
                 const MPI_Message_Encoder<Subproblem_Params>& encoder,
                 const Goal goal,
                 const Domain_Type WorstBound) override;
};


template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
void MPI_Scheduler_Hybrid<Prob_Consts, Subproblem_Params, Domain_Type>::Execute(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
                                                                                  const Prob_Consts& prob,
                                                                                  const MPI_Message_Encoder<Subproblem_Params>& encoder,
                                                                                  const Goal goal,
                                                                                  const Domain_Type WorstBound)
{
    int provided;
    MPI_Init_thread(0,NULL, MPI_THREAD_SERIALIZED, &provided);
    int pid, num;
    MPI_Comm_rank(MPI_COMM_WORLD, &pid);
    MPI_Comm_size(MPI_COMM_WORLD, &num);
    if(pid == 0)
        printProc("provided thread support = " << provided << " and we requested " << MPI_THREAD_SERIALIZED);
    assert(num >= 3 && "this implementation needs at least 3 cores");
    MPI_Status st;
    std::stringstream sendstream("");
    sendstream << std::setprecision(15);
    std::stringstream receivstream("");
    receivstream << std::setprecision(15);

    char buffer[2000];

    if(pid == 0){
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
            receivstream.str(buffer);
            if(st.MPI_TAG == MessageType::GET_SLAVES){
                int sl_needed;
                int r = st.MPI_SOURCE;
                receivstream >> sl_needed;
                int sl_given = std::min(sl_needed, (int)idleProcIds.size());;
                sendstream.str("");
                sendstream << sl_given << " ";
                printProc("sending so many slaves to the proc: " << sl_given);

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
                printProc("received solution with bound: " << CandidateBound << " while best Bound is " << GlobalBestBound);

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
    }
    else{
        // local variables of slave
        std::priority_queue<Subproblem_Params, std::vector<Subproblem_Params>, comparator> LocalTaskQueue;
        Domain_Type LocalBestBound = WorstBound;
        Subproblem_Params LocalBestProb;
        char req[2000];
        int counter;

        // maybe ICOllectiveSynchro

        while(true){
            MPI_Recv(buffer,2000, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
            receivstream.str(buffer);
            if(st.MPI_TAG == MessageType::PROB){
                counter = 1;

                Subproblem_Params Received_Params;
                encoder.Decode_Solution(receivstream, Received_Params);
                LocalTaskQueue.push(Received_Params);

                // ----------------- DEBUG CODE -----------
                Domain_Type newBoundValue;
                receivstream >> newBoundValue;
                // for debugging it should not be the case that anyone sends a worse bound then what we already have
                if(((bool)goal && newBoundValue > LocalBestBound)
                   || (!(bool)goal && newBoundValue < LocalBestBound)){
                    LocalBestBound = newBoundValue;
                }
                // ----------------------------------
                while(!LocalTaskQueue.empty())
                {
                    #pragma omp parallel num_threads(2) private(sendstream) shared(LocalBestBound, LocalTaskQueue)
                    {
                        while(counter % this->Communication_Frequency != 0) {
                            #pragma omp atomic
                            counter = counter + 1;
                            printProc("counter: " << counter);

                            Subproblem_Params sol;
                            bool isEmpty = false;
                            #pragma omp critical (queuelock)
                            {
                                isEmpty = LocalTaskQueue.empty();
                                if(!isEmpty)
                                {
                                    sol = LocalTaskQueue.top(); LocalTaskQueue.pop();
                                    printProc("reached here");
                                }
                            }
                            if(isEmpty)
                                continue;

                            //ignore if its bound is worse than already known best sol.
                            if(Problem_Def.Discard(prob, sol, LocalBestBound))
                                continue;

                            // try to make the bound better
                            auto CandidateBound = std::get<Domain_Type>(Problem_Def.GetBound(prob, sol));
                            printProc("candidate bound " << CandidateBound);
                            if(((bool)goal && CandidateBound > LocalBestBound)
                               || (!(bool)goal && CandidateBound < LocalBestBound)){
                                #pragma omp critical
                                {
                                    LocalBestBound = CandidateBound;
                                }
                            }

                            // check if we cant divide further
                            if(Problem_Def.IsFeasible(prob, sol)){
                                sendstream.str("");
                                encoder.Encode_Solution(sendstream, sol);
                                sendstream << LocalBestBound << " ";
                                //tell master that feasible solution is reached
                                printProc("sending local bound " << LocalBestBound);
                                #pragma omp critical
                                {
                                    MPI_Send(&sendstream.str()[0],strlen(sendstream.str().c_str())+1, MPI_CHAR,0,MessageType::DONE, MPI_COMM_WORLD);
                                }
                                continue;
                            }

                            std::vector<Subproblem_Params> v = Problem_Def.SplitSolution(prob, sol);

                            for(const auto& el : v){
                                #pragma omp critical (queuelock)
                                {
                                    LocalTaskQueue.push(el);
                                }}
                        }
                    }

                    printProc("LEFT OMP");
                    counter++; // start new omp session after this

                    if(!LocalTaskQueue.empty()) // after omp region we are gonna send request for slaves and also our best bound
                    {
                        sendstream.str("");
                        sendstream << (int)LocalTaskQueue.size() << " ";
                        printProc("sending request for " << LocalTaskQueue.size() << " tasks");
                        MPI_Send(&sendstream.str()[0],strlen(sendstream.str().c_str()),MPI_CHAR,0,MessageType::GET_SLAVES,MPI_COMM_WORLD);
                        //get master's response
                        MPI_Recv(buffer,200, MPI_CHAR, 0, MessageType::GET_SLAVES, MPI_COMM_WORLD, &st);
                        receivstream.str(buffer);
                        int slaves_avbl;
                        receivstream >> slaves_avbl;
                        printProc("distributing onto so many slaves: " << slaves_avbl);

                        for(int i=0; i<slaves_avbl; i++){
                            //give a problem to each slave
                            Subproblem_Params SubProb;
                            SubProb = LocalTaskQueue.top(); LocalTaskQueue.pop();
                            int sl_no;
                            receivstream >> sl_no;
                            sendstream.str("");
                            encoder.Encode_Solution(sendstream, SubProb);
                            sendstream << LocalBestBound << " ";
                            //send it to idle processor
                            printProc("trying to send");
                            MPI_Send(&sendstream.str()[0],strlen(sendstream.str().c_str()), MPI_CHAR, sl_no, MessageType::PROB, MPI_COMM_WORLD);
                            printProc("sending completed");
                        }
                    }
                }

                //This slave has now become idle (its queue is empty). Inform master.
                sendstream.str("");
                MPI_Send(&sendstream.str()[0],10,MPI_CHAR,0,MessageType::IDLE, MPI_COMM_WORLD);
            }
            else if(st.MPI_TAG == MessageType::FINISH){
                assert(st.MPI_SOURCE==0); // only master can tell it to finish
                break; //from the while(1) loop
            }
        }
    }
    MPI_Finalize();
}
