#pragma once

#include "Base.h"
#include "MPI_Message_Encoder.h"

// strategy pattern that holds the actual MPI algorithm to schedule the work
namespace Default {enum MessageType{PROB = 0, GET_SLAVES = 1, DONE = 2, IDLE = 3, FINISH = 4};}

template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
class MPI_Scheduler
{
public:
    // TODO has to be private!!!!!!!! we need some friend mechanism to let the solver use it
    virtual Subproblem_Params Execute(const Problem_Definition<Prob_Consts, Subproblem_Params, Domain_Type>& Problem_Def,
                         const Prob_Consts& prob,
                         const MPI_Message_Encoder<Subproblem_Params>& encoder,
                         const Goal goal,
                         const Domain_Type WorstBound) = 0;


    MPI_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type>* CommFrequency(int num){Communication_Frequency = num; return this;}
    MPI_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type>* Threads(size_t num) {OpenMPThreads = num; return this;}
    MPI_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type>* Eps(Domain_Type e) {eps = e; return this;}
    MPI_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type>* TraversMode(TraversalMode m) {mode = m; return this;};

protected:
    int Communication_Frequency = 1;
    Domain_Type eps;
    TraversalMode mode = TraversalMode::DFS;



    // only for Hybrid
    int OpenMPThreads = 1;
};


template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params DefaultMasterBehavior(std::stringstream& sendstream,
                                        std::stringstream& receivstream,
                                        const Problem_Definition<Prob_Consts, Subproblem_Params, Domain_Type>& Problem_Def,
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

    Domain_Type GlobalBestBound = WorstBound;
    //master processor
    Subproblem_Params BestSubproblem = Problem_Def.GetInitialSubproblem(prob);

    // stores idleProcIds;
    std::vector<int> idleProcIds;
    for(int i=2;i<num;i++){
        idleProcIds.push_back(i);
    }
    // encode initial problem and empty sol into sendstream
    sendstream << 1 << " ";
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
                                      const Problem_Definition<Prob_Consts, Subproblem_Params, Domain_Type>& Problem_Def,
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
        Domain_Type GlobalBound = Problem_Def.GetContainedUpperBound(prob, GlobalSolution);
        for(int i = 1; i < num; i++)
        {
            MPI_Recv(buf, 1000, MPI_CHAR, i, 100, MPI_COMM_WORLD, &st);
            receivstream.str(buf);
            Subproblem_Params Subproblem;
            encoder.Decode_Solution(receivstream, Subproblem);
            Domain_Type temp = Problem_Def.GetContainedUpperBound(prob, Subproblem);
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
