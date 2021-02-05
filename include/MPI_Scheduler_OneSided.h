#pragma once

#include "Base.h"
#include "MPI_Scheduler_Default.h"

namespace OneSided {
    enum MessageType {
        IDLE_PROC_WANTS_WORK = 0,
        WORK_EXCHANGE = 1,
    };
}

template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
class MPI_Scheduler_OneSided : public MPI_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type> {
public:
    struct comparator {
        bool operator()(const Subproblem_Params &p1, const Subproblem_Params &p2) {
            return std::get<0>(p1) < std::get<0>(p2);
        }
    };
    Subproblem_Params Execute(const Problem_Definition<Prob_Consts, Subproblem_Params> &Problem_Def,
                              const Prob_Consts &prob,
                              const MPI_Message_Encoder<Subproblem_Params> &encoder,
                              const Goal goal,
                              const Domain_Type WorstBound) override;
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


template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
Subproblem_Params MPI_Scheduler_OneSided<Prob_Consts, Subproblem_Params, Domain_Type>::Execute(
        const Problem_Definition<Prob_Consts, Subproblem_Params> &Problem_Def,
        const Prob_Consts &prob,
        const MPI_Message_Encoder<Subproblem_Params> &encoder,
        const Goal goal,
        const Domain_Type WorstBound) {

    int pid, num;
    MPI_Comm_rank(MPI_COMM_WORLD, &pid);
    MPI_Comm_size(MPI_COMM_WORLD, &num);
    assert(num >= 2 && "this implementation needs at least 2 cores");
    MPI_Status st;
    MPI_Status throwAway;
    MPI_Request req, req2;
    std::stringstream sendstream("");
    sendstream << std::setprecision(15);
    std::stringstream receivstream("");
    receivstream << std::setprecision(15);

    char buffer[20000];
    int NumMessages = 0;

    std::priority_queue<Subproblem_Params, std::vector<Subproblem_Params>, comparator> LocalTaskQueue;
    Domain_Type LocalBestBound = 100000.0;//WorstBound;
    Subproblem_Params BestSubproblem;
    int ImIdle = false;
    bool RequestSent = false;
    int ProcWhomISend;
    MPI_Win boundWindow;
    MPI_Win busyWindow;
    MPI_Win_create(&LocalBestBound, sizeof(Domain_Type), sizeof(Domain_Type), MPI_INFO_NULL, MPI_COMM_WORLD,
                   &boundWindow);
    MPI_Win_create(&ImIdle, sizeof(int), sizeof(int), MPI_INFO_NULL, MPI_COMM_WORLD, &busyWindow);
    MPI_Win_fence(MPI_MODE_NOPUT, boundWindow);
    MPI_Win_fence(MPI_MODE_NOPUT, busyWindow);
    MPI_Datatype type = ConvertTypeToMPIType<Domain_Type>();
    BestSubproblem = Problem_Def.GetInitialSubproblem(prob);

    if (pid == 0) {
        LocalTaskQueue.push(BestSubproblem);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    int counter = 0;
    int IdleProcAsksForWork = 0;
    float PercentShare = 0.1f;
    while (true) {
        while (!LocalTaskQueue.empty()) {
            ImIdle = 0;
            MPI_Iprobe(MPI_ANY_SOURCE, OneSided::MessageType::IDLE_PROC_WANTS_WORK, MPI_COMM_WORLD,
                       &IdleProcAsksForWork, &st); // test if another processor has sent me a request
            if (IdleProcAsksForWork == 1) {
                MPI_Recv(buffer, 1, MPI_CHAR, st.MPI_SOURCE, OneSided::MessageType::IDLE_PROC_WANTS_WORK,
                         MPI_COMM_WORLD, &st);
                int queueSize = LocalTaskQueue.size();
                int shareSize = std::min(static_cast<int>(queueSize * PercentShare), 100);
                printProc("im sending " << shareSize << " tasks")
                sendstream.str("");
                sendstream <<  shareSize << " ";
                for (int i = 0; i < shareSize; i++) {
                    Subproblem_Params subprb = LocalTaskQueue.top();
                    LocalTaskQueue.pop();
                    encoder.Encode_Solution(sendstream, subprb);
                }
                assert(strlen(sendstream.str().c_str()) < 20000);
                MPI_Issend(&sendstream.str()[0], strlen(sendstream.str().c_str()), MPI_CHAR, st.MPI_SOURCE,
                         OneSided::MessageType::WORK_EXCHANGE, MPI_COMM_WORLD, &req2);
                IdleProcAsksForWork = 0;
            }

            counter++;
            //printProc(LocalTaskQueue.size())

            //take out one element from queue, expand it
            Subproblem_Params sol = LocalTaskQueue.top();
            LocalTaskQueue.pop();

            //ignore if its bound is worse than already known best sol.
            Domain_Type LowerBound = std::get<Domain_Type>(Problem_Def.GetLowerBound(prob, sol));
            if (((bool) goal && LowerBound < LocalBestBound)
                || (!(bool) goal && LowerBound > LocalBestBound)) {
                continue;
            }

            // try to make the bound better only if the solution lies in a feasible domain
            bool IsPotentialBestSolution = false;
            auto Feasibility = Problem_Def.IsFeasible(prob, sol);
            Domain_Type CandidateBound;
            if (Feasibility == BnB::FEASIBILITY::FULL) {
                CandidateBound = std::get<Domain_Type>(Problem_Def.GetUpperBound(prob, sol));
                if (((bool) goal && CandidateBound >= LocalBestBound)
                    || (!(bool) goal && CandidateBound <= LocalBestBound)) {
                    LocalBestBound = CandidateBound;
                    IsPotentialBestSolution = true;
                }
            } else if (Feasibility == BnB::FEASIBILITY::NONE) // basically discard again
                continue;

            std::vector<Subproblem_Params> v;
            //printProc("CandidateBound = " << CandidateBound << " LowerBound = " << LowerBound);
            if (std::abs(CandidateBound - LowerBound) > this->eps) { // epsilon criterion for convergence
                v = Problem_Def.SplitSolution(prob, sol);
                for (const auto &el : v)
                    LocalTaskQueue.push(el);
            } else if (IsPotentialBestSolution) // if we cant and we have a good solution we send it to master
            {
                BestSubproblem = sol;
            }

            // synchronize bounds
            if (counter % this->Communication_Frequency == 0) {
                for (int i = 0; i < num; i++) {
                    if (pid == i)
                        continue;
                    double OtherBound;
                    MPI_Get(&OtherBound, 1, MPI_DOUBLE, i, 0, 1, MPI_DOUBLE, boundWindow);
                    printProc("the new bound is " << OtherBound << " and old bound is " << LocalBestBound)
                    if (((bool) goal && OtherBound > LocalBestBound)
                        || (!(bool) goal && OtherBound < LocalBestBound)) {
                        printProc("found better bound")
                        LocalBestBound = OtherBound;
                    }
                }
                counter = 0;
            }
        } // queue empty ------------------------------------------
        //This slave has now become idle
        ImIdle = true;


        if (!RequestSent) {
            for (int i = 0; i < num; i++) {
                int isProcIdle;
                if (pid == i)
                    continue;

                MPI_Get(&isProcIdle, 1, MPI_INT, i, 0, 1, MPI_INT, busyWindow);
                if (isProcIdle == 0) {
                    char anything;
                    MPI_Issend(&anything, 1, MPI_CHAR, i, OneSided::MessageType::IDLE_PROC_WANTS_WORK,
                            MPI_COMM_WORLD, &req);
                    RequestSent = true;
                    ProcWhomISend = i;
                }
            }
        } else {
            int requestReceived;
            MPI_Test(&req, &requestReceived, MPI_STATUS_IGNORE);
            if (requestReceived == 1) {
                MPI_Recv(buffer, 20000, MPI_CHAR, ProcWhomISend, OneSided::MessageType::WORK_EXCHANGE,
                        MPI_COMM_WORLD, &throwAway);
                receivstream.str(buffer);
                int numProblems;
                receivstream >> numProblems;
                //printProc("processor responded with " << numProblems << " so many problems");
                for (int p = 0; p < numProblems; p++) {
                    Subproblem_Params subprb;
                    encoder.Decode_Solution(receivstream, subprb);
                    LocalTaskQueue.push(subprb);
                }
                RequestSent = false;
            }
        }

        if(pid == 0) {
            int NumOfIdleProcs = 0;
            for (int i = 0; i < num; i++) {
                if (pid != i) {
                    int isProcIdle;
                    MPI_Get(&isProcIdle, 1, MPI_INT, i, 0, 1, MPI_INT, busyWindow);
                    if (isProcIdle == 1)
                        NumOfIdleProcs++;
                }
            }
            if (NumOfIdleProcs == num - 1){
                for(int i=1;i<num;i++)
                    MPI_Isend(buffer, 0, MPI_CHAR, i, Default::MessageType::FINISH, MPI_COMM_WORLD, &req);
                break; // from while loop
            }
        }

        int flag;
        MPI_Iprobe(0, Default::MessageType::FINISH, MPI_COMM_WORLD,
                   &flag, &st);
        if (flag == 1)
        {
            printProc("got termination message");
            break;
        }
    }
    printProc("leaving the loop");
    MPI_Win_fence(MPI_MODE_NOSUCCEED, boundWindow);
    MPI_Win_fence(MPI_MODE_NOSUCCEED, busyWindow);
    MPI_Barrier(MPI_COMM_WORLD);

    // receive pending messages
    for(int i = 0; i < num; i++)
    {
        MPI_Iprobe(MPI_ANY_SOURCE, OneSided::MessageType::IDLE_PROC_WANTS_WORK, MPI_COMM_WORLD,
                   &IdleProcAsksForWork, &st); // test if master has sent me a request
        if (IdleProcAsksForWork == 1)
            MPI_Recv(buffer, 1, MPI_CHAR, st.MPI_SOURCE, OneSided::MessageType::IDLE_PROC_WANTS_WORK,
                     MPI_COMM_WORLD, &st);
    }

        // ------------------------ ALL procs have a best solution now master has to gather it
    return ExtractBestSolution<Prob_Consts,Subproblem_Params, Domain_Type>(sendstream, receivstream,
                                                                           BestSubproblem,
                                                                           Problem_Def,
                                                                           prob,
                                                                           encoder,
                                                                           goal);
}
