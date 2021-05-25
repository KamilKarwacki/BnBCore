#pragma once

#include "Base.h"
#include "MPI_Message_Encoder.h"

namespace BnB {
    namespace PtoP { // messages used in Point to Point based schedulers (MasterWorker and Hybrid
        enum MessageType {
            PROB = 0, GET_WORKERS = 1, IDLE = 2, FINISH = 3, INITIAL = 4,
        };
    }

    // strategy pattern that holds the actual MPI algorithm to schedule the work
    template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
    class MPI_Scheduler {
    public:
        virtual Subproblem_Params
        Execute(const Problem_Definition <Prob_Consts, Subproblem_Params, Domain_Type> &Problem_Def,
                const Prob_Consts &prob,
                const MPI_Message_Encoder <Subproblem_Params> &encoder,
                const Goal goal,
                const Domain_Type WorstBound) = 0;


        MPI_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type> *CommFrequency(int num) {Communication_Frequency = num; return this;}
        MPI_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type> *Eps(Domain_Type e) {eps = e; return this;}
        MPI_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type> *TraversMode(TraversalMode m) {mode = m; return this;};
        MPI_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type> *MaximalPackageSize(int size) {MaxPackageSize = size; return this;}

    protected:
        int Communication_Frequency = 1;
        Domain_Type eps;
        TraversalMode mode = TraversalMode::DFS;
        int MaxPackageSize = 1;

    };

    // get next id if you are the last proc then send to 0
    void Next(int &id, int num) {
        id = (id + 1) % num;
        if (id == 0) id = 1;
    }


    // Master used by MasterWorker and Hybrid that does a initial split of the domain, besides that it does the same as the normal master
    template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
    Subproblem_Params SplittingMasterBehavior(std::vector<std::stringstream> &sendstream,
                                              std::stringstream &receivstream,
                                              const Problem_Definition <Prob_Consts, Subproblem_Params, Domain_Type> &Problem_Def,
                                              const Prob_Consts &prob,
                                              const MPI_Message_Encoder <Subproblem_Params> &encoder,
                                              const Goal goal,
                                              const Domain_Type WorstBound) {
        char buffer[20000];
        int pid, num;
        MPI_Comm_rank(MPI_COMM_WORLD, &pid);
        MPI_Comm_size(MPI_COMM_WORLD, &num);
        MPI_Status st;
        std::vector<MPI_Request> req(num);
        std::vector<bool> OpenRequests(num, false);

        int NumMessages = 0;

        Domain_Type GlobalBestBound = WorstBound;
        Subproblem_Params BestSubproblem = Problem_Def.GetInitialSubproblem(prob);


        std::vector<Subproblem_Params> subproblems = Problem_Def.SplitSolution(prob, BestSubproblem);
        printProc("there are " << subproblems.size() << " problems that i need to sent")
        int ProblemsToDivide = subproblems.size();
        std::vector<bool> sentToID(num + 2, false);
        // if there are more workers than tasks
        if (num - 1 > subproblems.size()) {
            int size = subproblems.size();
            for (int i = 1; i <= size; i++) {
                Subproblem_Params currentSub = subproblems.back();
                subproblems.pop_back();
                int Numpacks = 1;
                sendstream[i] << Numpacks << " ";
                MPI_Send(sendstream[i].str().c_str(), strlen(sendstream[i].str().c_str()), MPI_CHAR, i,
                         PtoP::MessageType::INITIAL, MPI_COMM_WORLD);
                sendstream[i].str("");
                sendstream[i] << 1 << " ";
                encoder.Encode_Solution(sendstream[i], currentSub);
                MPI_Send(sendstream[i].str().c_str(), strlen(sendstream[i].str().c_str()), MPI_CHAR, i,
                         PtoP::MessageType::INITIAL, MPI_COMM_WORLD);
                sendstream[i].str("");
                sentToID[i] = true;
            }
        } else { // if the number of tasks is bigger than the number of workers
            const int MaxPackSize = 5;
            int i = 0;
            std::vector<int> PackSizes;
            while (true) {
                if ((PackSizes.size() + 1) * MaxPackSize <= ProblemsToDivide)
                    PackSizes.push_back(MaxPackSize);
                else {
                    PackSizes.push_back(subproblems.size() - PackSizes.size() * MaxPackSize);
                    break;
                }
            }
            int PacksNeedToSend = std::ceil(PackSizes.size() / ((float) (num - 1.0)));
            printProc("Packs of work sent to each proc " << PacksNeedToSend)


            std::for_each(PackSizes.begin(), PackSizes.end(), [](int i) { std::cout << i << " "; });
            std::cout << std::endl;

            while (!subproblems.empty()) {
                Next(i, num);
                sentToID[i] = true;

                if (i == num - 1) PacksNeedToSend = PackSizes.size();
                sendstream[i].str("");
                sendstream[i] << PacksNeedToSend << " ";
                MPI_Send(sendstream[i].str().c_str(), strlen(sendstream[i].str().c_str()), MPI_CHAR, i,
                         PtoP::MessageType::INITIAL, MPI_COMM_WORLD);
                sendstream[i].str("");

                for (int j = 0; j < PacksNeedToSend; j++) {
                    int PackSize = PackSizes.front();
                    PackSizes.erase(PackSizes.begin());
                    sendstream[i] << PackSize << " ";
                    for (int p = 0; p < PackSize; p++) {
                        Subproblem_Params currentSub = subproblems.back();
                        subproblems.pop_back();
                        encoder.Encode_Solution(sendstream[i], currentSub);
                    }
                    MPI_Send(sendstream[i].str().c_str(), strlen(sendstream[i].str().c_str()), MPI_CHAR, i,
                             PtoP::MessageType::INITIAL, MPI_COMM_WORLD);
                    sendstream[i].str("");
                }

            }
        }

        // stores idleProcIds;
        std::vector<int> idleProcIds;
        for (int i = 1; i < num; i++) {
            if (sentToID[i] == false)
                idleProcIds.push_back(i);
        }
        std::for_each(idleProcIds.begin(), idleProcIds.end(), [](bool i) { std::cout << i << " "; });
        // TODO this is the same as for default master, refactor into one core function
        while (idleProcIds.size() != num - 1) {
            MPI_Recv(buffer, 2000, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
            NumMessages++;
            receivstream.str(buffer);
            if (st.MPI_TAG == PtoP::MessageType::GET_WORKERS) {
                int sl_needed;
                int r = st.MPI_SOURCE;

                Domain_Type CandidateBound;
                receivstream >> CandidateBound;

                if (((bool) goal && CandidateBound > GlobalBestBound) ||
                    (!(bool) goal && CandidateBound < GlobalBestBound)) {
                    GlobalBestBound = CandidateBound;
                }

                receivstream >> sl_needed;
                int sl_given = std::min(sl_needed, (int) idleProcIds.size());;
                sendstream[r].str("");
                sendstream[r] << GlobalBestBound << " ";
                sendstream[r] << sl_given << " ";

                std::for_each(idleProcIds.begin(), idleProcIds.begin() + sl_given,
                              [&sendstream, &r](const int &el) { sendstream[r] << el << " "; });
                if (OpenRequests[r]) MPI_Wait(&req[r], MPI_STATUS_IGNORE);
                MPI_Isend(&sendstream[r].str()[0], strlen(sendstream[r].str().c_str()), MPI_CHAR, r,
                          PtoP::MessageType::GET_WORKERS, MPI_COMM_WORLD, &req[r]);
                OpenRequests[r] = true;
                idleProcIds.erase(idleProcIds.begin(), idleProcIds.begin() + sl_given);
            } else if (st.MPI_TAG == PtoP::MessageType::IDLE) {
                //slave has become idle
                idleProcIds.push_back(st.MPI_SOURCE);
            }
        }

        for (int i = 1; i < num; i++) {
            MPI_Send(buffer, 1, MPI_CHAR, i, PtoP::MessageType::FINISH, MPI_COMM_WORLD);
        }

        for (int i = 1; i < num; i++)
            if (OpenRequests[i]) MPI_Request_free(&req[i]);
        printProc("the master received a total of " << NumMessages << " messages");
        return BestSubproblem;
    }


    // the Master code that is used by MasterWorker and Hybrif
    template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
    Subproblem_Params DefaultMasterBehavior(std::vector<std::stringstream> &sendstream,
                                            std::stringstream &receivstream,
                                            const Problem_Definition <Prob_Consts, Subproblem_Params, Domain_Type> &Problem_Def,
                                            const Prob_Consts &prob,
                                            const MPI_Message_Encoder <Subproblem_Params> &encoder,
                                            const Goal goal,
                                            const Domain_Type WorstBound) {
        char buffer[2000];
        int pid, num;
        MPI_Comm_rank(MPI_COMM_WORLD, &pid);
        MPI_Comm_size(MPI_COMM_WORLD, &num);
        MPI_Status st;
        std::vector<MPI_Request> req(num);
        std::vector<bool> OpenRequests(num, false);

        int NumMessages = 0;

        Domain_Type GlobalBestBound = WorstBound;
        //master processor
        Subproblem_Params BestSubproblem = Problem_Def.GetInitialSubproblem(prob);

        // stores idleProcIds;
        std::vector<int> idleProcIds;
        for (int i = 2; i < num; i++) {
            idleProcIds.push_back(i);
        }
        // encode initial problem and empty sol into sendstream
        sendstream[1] << 1 << " ";
        encoder.Encode_Solution(sendstream[1], BestSubproblem);
        sendstream[1] << GlobalBestBound << " ";
        // send it to idle processor no. 1
        MPI_Send(sendstream[1].str().c_str(), strlen(sendstream[1].str().c_str()), MPI_CHAR, 1,
                 PtoP::MessageType::PROB, MPI_COMM_WORLD);
        while (idleProcIds.size() != num - 1) {
            MPI_Recv(buffer, 2000, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
            NumMessages++;
            receivstream.str(buffer);
            if (st.MPI_TAG == PtoP::MessageType::GET_WORKERS) {
                int sl_needed;
                int r = st.MPI_SOURCE;

                Domain_Type CandidateBound;
                receivstream >> CandidateBound;

                if (((bool) goal && CandidateBound > GlobalBestBound) ||
                    (!(bool) goal && CandidateBound < GlobalBestBound)) {
                    GlobalBestBound = CandidateBound;
                }

                receivstream >> sl_needed;
                int sl_given = std::min(sl_needed, (int) idleProcIds.size());;
                sendstream[r].str("");
                sendstream[r] << GlobalBestBound << " ";
                sendstream[r] << sl_given << " ";

                std::for_each(idleProcIds.begin(), idleProcIds.begin() + sl_given,
                              [&sendstream, &r](const int &el) { sendstream[r] << el << " "; });
                if (OpenRequests[r]) MPI_Wait(&req[r], MPI_STATUS_IGNORE);
                MPI_Isend(&sendstream[r].str()[0], strlen(sendstream[r].str().c_str()), MPI_CHAR, r,
                          PtoP::MessageType::GET_WORKERS, MPI_COMM_WORLD, &req[r]);
                OpenRequests[r] = true;
                idleProcIds.erase(idleProcIds.begin(), idleProcIds.begin() + sl_given);
            } else if (st.MPI_TAG == PtoP::MessageType::IDLE) {
                //slave has become idle
                idleProcIds.push_back(st.MPI_SOURCE);
            }
        }

        for (int i = 1; i < num; i++) {
            MPI_Send(buffer, 1, MPI_CHAR, i, PtoP::MessageType::FINISH, MPI_COMM_WORLD);
        }

        for (int i = 1; i < num; i++)
            if (OpenRequests[i]) MPI_Request_free(&req[i]);
        printProc("the master received a total of " << NumMessages << " messages");
        return BestSubproblem;
    }


    // after the algorithm has finished every processor has its local optimum, the global optimum has to be found
    template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
    Subproblem_Params ExtractBestSolution(std::stringstream &sendstream, std::stringstream &receivstream,
                                          Subproblem_Params BestSubproblem,
                                          const Problem_Definition <Prob_Consts, Subproblem_Params, Domain_Type> &Problem_Def,
                                          const Prob_Consts &prob,
                                          const MPI_Message_Encoder <Subproblem_Params> &encoder,
                                          const Goal goal) {
        int pid, num;
        MPI_Comm_rank(MPI_COMM_WORLD, &pid);
        MPI_Comm_size(MPI_COMM_WORLD, &num);
        MPI_Status st;
        if (pid == 0) { // master receives all solutions and compares them
            char buf[10000];
            Subproblem_Params GlobalSolution = BestSubproblem;
            Domain_Type GlobalBound = Problem_Def.GetContainedUpperBound(prob, GlobalSolution);
            for (int i = 1; i < num; i++) {
                MPI_Recv(buf, 10000, MPI_CHAR, i, 999, MPI_COMM_WORLD, &st);
                receivstream.str(buf);
                Subproblem_Params Subproblem;
                encoder.Decode_Solution(receivstream, Subproblem);
                Domain_Type temp = Problem_Def.GetContainedUpperBound(prob, Subproblem);
                if (((bool) goal && temp >= GlobalBound)
                    || (!(bool) goal && temp <= GlobalBound)) {
                    GlobalSolution = Subproblem;
                    GlobalBound = temp;
                }
            }
            Problem_Def.PrintSolution(GlobalSolution);
            return GlobalSolution;
        } else { // worker sends his solution
            sendstream.str("");
            encoder.Encode_Solution(sendstream, BestSubproblem);
            MPI_Send(&sendstream.str()[0], strlen(sendstream.str().c_str()), MPI_CHAR, 0,
                     999, MPI_COMM_WORLD);
            return BestSubproblem;
        }
    }
}
