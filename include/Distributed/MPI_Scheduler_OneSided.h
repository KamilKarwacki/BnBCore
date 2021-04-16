#pragma once

#include "MPI_Scheduler.h"

namespace OneSided {
    enum MessageType {
        IDLE_PROC_WANTS_WORK = 0,
        WORK_EXCHANGE = 1,
    };
}

template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
class MPI_Scheduler_OneSided : public MPI_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type> {
public:
    Subproblem_Params Execute(const Problem_Definition<Prob_Consts, Subproblem_Params, Domain_Type> &Problem_Def,
                              const Prob_Consts &prob,
                              const MPI_Message_Encoder<Subproblem_Params> &encoder,
                              const Goal goal,
                              const Domain_Type WorstBound) override;

    MPI_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type>* TermCheckFrequency(int freq){TerminationCheckFrequency = freq; return this;}
protected:
    int TerminationCheckFrequency = 100;
    float PercentageToShare = 0.1f;
};


/*
 * The main problem with the other implementations is that every n iterations there is a blocking communication with the master
 * this implementation works differently
 * the master contacts the slaves, the salves just listen using a probe (Work Stealing model)
 * furthermore, the processors exchange their best solutions using one sided communication
 */


template<typename T> int ConvertTypeToMPIType() { assert(false); }
template<> int ConvertTypeToMPIType<int>() { return MPI_INT; }
template<> int ConvertTypeToMPIType<float>() { return MPI_FLOAT; }
template<> int ConvertTypeToMPIType<double>() { return MPI_DOUBLE; }



MPI_Datatype UserDefinedType()
{
    MPI_Datatype my_type;
    MPI_Type_contiguous(2,MPI_DOUBLE, &my_type);
    MPI_Type_commit(&my_type);
    return my_type;
}


template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params MPI_Scheduler_OneSided<Prob_Consts, Subproblem_Params, Domain_Type>::Execute(
        const Problem_Definition<Prob_Consts, Subproblem_Params, Domain_Type> &Problem_Def,
        const Prob_Consts &prob,
        const MPI_Message_Encoder<Subproblem_Params> &encoder,
        const Goal goal,
        const Domain_Type WorstBound) {

    MPI_Datatype UserType = UserDefinedType();

    int pid, num;
    MPI_Comm_rank(MPI_COMM_WORLD, &pid);
    MPI_Comm_size(MPI_COMM_WORLD, &num);
    assert(num >= 2 && "this implementation needs at least 2 cores");
    MPI_Status st;
    MPI_Status throwAway;
    MPI_Request workReq, req2, terminationReq, boundexchangeReq;
    std::vector<std::stringstream> sendstreams(num);
    for(auto& stream : sendstreams)
        stream << std::setprecision(15);
    std::stringstream receivstream("");
    receivstream << std::setprecision(15);

    auto *BestBoundArray = new Domain_Type[num];
    auto *IdleStatusArray = new int[num];

    int BoundToTerminationCheckRatio = this->TerminationCheckFrequency / this->Communication_Frequency;
    assert(BoundToTerminationCheckRatio > 1);

    char buffer[20000];
    int NumMessages = 0;
    int NumProblemsSolved = 0;

    std::deque<Subproblem_Params> LocalTaskQueue;
    Domain_Type LocalBestBound = WorstBound;
    Domain_Type LocalBoundToShare;
    Subproblem_Params BestSubproblem;
    int ImIdle;
    int ProcWhomISend;
    bool RequestSent = false;
    bool IallgatherOngoing = false;
    bool BoundComm = true;
    bool IsTerminationAllgatherGoingOn = false;

    MPI_Datatype type = ConvertTypeToMPIType<Domain_Type>();
    BestSubproblem = Problem_Def.GetInitialSubproblem(prob);

    if (pid == 0) {
        LocalTaskQueue.push_back(BestSubproblem);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    int counter = 0;
    int counter2 = 0;
    int IdleProcAsksForWork = 0;
    while (true) {
        if (!LocalTaskQueue.empty()) {
            counter++;
    	    NumProblemsSolved++;

            //take out one element from queue, expand it
            Subproblem_Params sol;
            if(this->mode == TraversalMode::DFS){
                sol = LocalTaskQueue.back(); LocalTaskQueue.pop_back();
            }else if(this->mode == TraversalMode::BFS){
                sol = LocalTaskQueue.front(); LocalTaskQueue.pop_front();
            }

            //ignore if its bound is worse than already known best sol.
            auto [LowerBound, UpperBound] = Problem_Def.GetEstimateForBounds(prob, sol);
            if (((bool) goal && LowerBound < LocalBestBound)
                || (!(bool) goal && LowerBound > LocalBestBound)) {
                continue;
            }

            // try to make the bound better only if the solution lies in a feasible domain
            auto Feasibility = Problem_Def.IsFeasible(prob, sol);
            Domain_Type CandidateBound;
            if (Feasibility == BnB::FEASIBILITY::Full) {
                CandidateBound = (Problem_Def.GetContainedUpperBound(prob, sol));
                if (((bool) goal && CandidateBound >= LocalBestBound)
                    || (!(bool) goal && CandidateBound <= LocalBestBound)) {
                    LocalBestBound = CandidateBound;
                    BestSubproblem = sol;
                }
            } else if(Feasibility == BnB::FEASIBILITY::PARTIAL){
                // use our backup for the CandidateBound
                CandidateBound = UpperBound;
            }
            else if (Feasibility == BnB::FEASIBILITY::NONE) // basically discard again
                continue;

            std::vector<Subproblem_Params> v;
            if (std::abs(CandidateBound - LowerBound) > this->eps) { // epsilon criterion for convergence
                v = Problem_Def.SplitSolution(prob, sol);
                for (auto &&el : v)
                    LocalTaskQueue.push_back(el);
	        }
        }
        // communication phase -----------------------------------------------------------------------------------------
        // test if someone needs work
        MPI_Iprobe(MPI_ANY_SOURCE, OneSided::MessageType::IDLE_PROC_WANTS_WORK, MPI_COMM_WORLD,
                   &IdleProcAsksForWork, &st); // test if another processor has sent me a request
        if (IdleProcAsksForWork == 1) {
            NumMessages++;
            int target = st.MPI_SOURCE;
            MPI_Recv(buffer, 1, MPI_CHAR, target, OneSided::MessageType::IDLE_PROC_WANTS_WORK,
                     MPI_COMM_WORLD, &st);
            int queueSize = LocalTaskQueue.size();
            int shareSize = std::min(static_cast<int>(queueSize * PercentageToShare), 1);
            sendstreams[target].str("");
            sendstreams[target] << LocalBestBound << " ";
            sendstreams[target] << shareSize << " ";
            ///std::vector<interval_MPI> packed;
            for (int i = 0; i < shareSize; i++) {
                Subproblem_Params subprb;
                ///if(this->mode == TraversalMode::DFS){
                ///    subprb = LocalTaskQueue.back(); LocalTaskQueue.pop_back();
                ///}else if( this->mode == TraversalMode::BFS){
                    subprb = LocalTaskQueue.front(); LocalTaskQueue.pop_front();
                ///}
                encoder.Encode_Solution(sendstreams[target], subprb);
                ///Problem_Def.PackData(subprb, packed);
            }
            assert(strlen(sendstreams[target].str().c_str()) < 20000);

            MPI_Issend(sendstreams[target].str().c_str(), strlen(sendstreams[target].str().c_str()), MPI_CHAR, target,
                       OneSided::MessageType::WORK_EXCHANGE, MPI_COMM_WORLD, &req2);
            ///MPI_Issend(&packed[0], shareSize*6, UserType, target, OneSided::MessageType::WORK_EXCHANGE, MPI_COMM_WORLD, &req2);
            IdleProcAsksForWork = 0;
        }


        // only send request if u are idle
        if(LocalTaskQueue.empty())
        {
            if (!RequestSent){
                char anything;
                ProcWhomISend = (rand() % static_cast<int>( num ));
                if (ProcWhomISend == pid) ProcWhomISend = (ProcWhomISend + 1) % num;
                MPI_Issend(&anything, 1, MPI_CHAR, ProcWhomISend, OneSided::MessageType::IDLE_PROC_WANTS_WORK,
                           MPI_COMM_WORLD, &workReq);
                RequestSent = true;
            } else {
                int requestReceived = 0;
                MPI_Test(&workReq, &requestReceived, MPI_STATUS_IGNORE);
                ///MPI_Iprobe(MPI_ANY_SOURCE, OneSided::MessageType::WORK_EXCHANGE, MPI_COMM_WORLD,
                ///           &requestReceived, &st); // test if another processor has sent me a request
                if (requestReceived == 1) {
                    int size;
                    ///std::vector<interval_MPI> receive_buf(1000);
                    ///MPI_Recv(receive_buf.data(), 1000, UserType, ProcWhomISend, OneSided::MessageType::WORK_EXCHANGE,
                    ///        MPI_COMM_WORLD, &throwAway);
                    MPI_Recv(buffer, 100000, MPI_CHAR, ProcWhomISend, OneSided::MessageType::WORK_EXCHANGE,
                             MPI_COMM_WORLD, &throwAway);
                    ///MPI_Get_count(&throwAway, UserType, &size);
                    receivstream.str(buffer);

                    Domain_Type CandidateBound;
                    receivstream >> CandidateBound;
                    if (((bool) goal && CandidateBound > LocalBestBound)
                        || (!(bool) goal && CandidateBound < LocalBestBound)) {
                        LocalBestBound = CandidateBound;
                    }
                    int numProblems;
                    receivstream >> numProblems;
                    ///Problem_Def.UnpackData(LocalTaskQueue, receive_buf, size);
                    for(int i = 0; i < numProblems; i++)
                    {
                        Subproblem_Params subprb;
                        encoder.Decode_Solution(receivstream, subprb);
                        LocalTaskQueue.push_back(subprb);
                    }

                    RequestSent = false;
                }
            }

        }


        counter++;
        if(BoundComm)
        {
            if ((counter % this->Communication_Frequency == 0) && !IallgatherOngoing) {
                LocalBoundToShare = LocalBestBound;
                MPI_Iallgather(&LocalBoundToShare, 1, MPI_DOUBLE,
                               BestBoundArray, 1, MPI_DOUBLE, MPI_COMM_WORLD, &boundexchangeReq); // make it a vector
                IallgatherOngoing = true;
            }
            else if (IallgatherOngoing) {
                int flag = 0;
                MPI_Test(&boundexchangeReq, &flag, MPI_STATUS_IGNORE);
                if (flag == 1) {
                    for (int i = 0; i < num; i++){
                        if (((bool) goal && BestBoundArray[i] > LocalBestBound)
                            || (!(bool) goal && BestBoundArray[i] < LocalBestBound)) {
                            LocalBestBound = BestBoundArray[i];
                        }
                    }
                    IallgatherOngoing = false;
                    counter = 0;
                    counter2++;
                    if(counter2 % BoundToTerminationCheckRatio == 0)
                    {
                        BoundComm = false;
                        counter2 = 0;
                    }
                }
            }
        }else
        {
            if ((counter % this->Communication_Frequency == 0) && !IsTerminationAllgatherGoingOn) {
                ImIdle = static_cast<int>(LocalTaskQueue.empty());
                MPI_Iallgather(&ImIdle, 1, MPI_INT,
                               IdleStatusArray, 1, MPI_INT, MPI_COMM_WORLD, &terminationReq); // make it a vector
                IsTerminationAllgatherGoingOn = true;
            }
            else if (IsTerminationAllgatherGoingOn) {
                int flag = 0;
                MPI_Test(&terminationReq, &flag, MPI_STATUS_IGNORE);
                if (flag == 1) {
                    int NumIdleProcs = 0;
                    for (int i = 0; i < num; i++) {
                        if (IdleStatusArray[i] == 1)
                            NumIdleProcs++;
                    }
                    if (NumIdleProcs == num) break;
                    IsTerminationAllgatherGoingOn = false;
                    counter = 0;
                    BoundComm = true;
                }
            }
        }
    }
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Type_free(&UserType);

    ///                       Clean UP                           ///
    IdleProcAsksForWork = 0;
    MPI_Iprobe(MPI_ANY_SOURCE, OneSided::MessageType::IDLE_PROC_WANTS_WORK, MPI_COMM_WORLD,
               &IdleProcAsksForWork, &st); // test if another processor has sent me a request
    if (IdleProcAsksForWork == 1){
        MPI_Recv(buffer, 1, MPI_CHAR, st.MPI_SOURCE, OneSided::MessageType::IDLE_PROC_WANTS_WORK,
                 MPI_COMM_WORLD, &st);
    }


    int requestReceived = 0;
    MPI_Iprobe(MPI_ANY_SOURCE, OneSided::MessageType::WORK_EXCHANGE, MPI_COMM_WORLD,
               &requestReceived, &st); // test if another processor has sent me a request
    if (requestReceived == 1)
    {
        MPI_Recv(buffer, 100000, MPI_CHAR, st.MPI_SOURCE, OneSided::MessageType::WORK_EXCHANGE,
                 MPI_COMM_WORLD, &throwAway);
    }


    MPI_Barrier(MPI_COMM_WORLD);

    // receive possible messages
    if(req2 !=  MPI_REQUEST_NULL){
        MPI_Request_free(&req2);
    }
    if(workReq !=  MPI_REQUEST_NULL){
        MPI_Request_free(&workReq);
    }
    if(boundexchangeReq !=  MPI_REQUEST_NULL){
        std::cout << "I SHOULD NOT BE HERE " << __LINE__ << std::endl;
        MPI_Cancel(&boundexchangeReq);
        MPI_Request_free(&boundexchangeReq);
    }if(terminationReq!=  MPI_REQUEST_NULL){
        std::cout << "I SHOULD NOT BE HERE " << __LINE__ << std::endl;
        MPI_Cancel(&terminationReq);
        MPI_Request_free(&terminationReq);
    }


    printProc("I have sent " <<  NumMessages << " messages and solved " << NumProblemsSolved << " problems" );


    // ------------------------ ALL procs have a best solution now master has to gather it
    return ExtractBestSolution<Prob_Consts, Subproblem_Params, Domain_Type>(sendstreams[pid], receivstream,
                                                                            BestSubproblem,
                                                                            Problem_Def,
                                                                            prob,
                                                                            encoder,
                                                                            goal);
}
