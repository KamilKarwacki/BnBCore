#pragma once

#include "MPI_Scheduler.h"
#include <iomanip>


template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
class MPI_Scheduler_Priority: public MPI_Scheduler<Prob_Consts, Subproblem_Params, Domain_Type>
{
public:
    Subproblem_Params Execute(const Problem_Definition<Prob_Consts, Subproblem_Params, Domain_Type>& Problem_Def,
                 const Prob_Consts& prob,
                 const MPI_Message_Encoder<Subproblem_Params>& encoder,
                 const Goal goal,
                 const Domain_Type WorstBound) override;
};

// TODO compare to normal if statements
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
Subproblem_Params MPI_Scheduler_Priority<Prob_Consts, Subproblem_Params, Domain_Type>::Execute(const Problem_Definition<Prob_Consts, Subproblem_Params, Domain_Type>& Problem_Def,
                                                                                 const Prob_Consts& prob,
                                                                                 const MPI_Message_Encoder<Subproblem_Params>& encoder,
                                                                                 const Goal goal,
                                                                                 const Domain_Type WorstBound)
{
    int PackageSize = 1;
    bool RequestOngoing = false;
    int pid, num;
    MPI_Comm_rank(MPI_COMM_WORLD, &pid);
    MPI_Comm_size(MPI_COMM_WORLD, &num);
    assert(num >= 3 && "this implementation needs at least 3 cores");
    MPI_Status st;
    MPI_Request SlaveReq;
    std::vector<MPI_Request> req(num);
    std::vector<std::stringstream> sendstreams(num);
    for(auto& stream : sendstreams)
        stream << std::setprecision(15);
    std::stringstream receivstream("");
    receivstream << std::setprecision(15);

    Subproblem_Params BestSubproblem = Problem_Def.GetInitialSubproblem(prob);
    int NumProblemsSolved = 0;
    int ProblemsEliminated = 0;

    if(pid == 0){
        BestSubproblem = DefaultMasterBehavior(sendstreams[0], receivstream, Problem_Def, prob, encoder, goal, WorstBound);
    }
    else{
        char buffer[20000];
        // local variables of slave
        std::deque<Subproblem_Params> LocalTaskQueue;
        Domain_Type LocalBestBound = WorstBound;

        int counter = 0;

        while(true){
            MPI_Recv(buffer,20000, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
            receivstream.str(buffer);
            if(st.MPI_TAG == Default::MessageType::PROB){ // is 0 if equal
                //slave has been given a partially solved problem to expand
                int NumOfProblems;
                receivstream >> NumOfProblems;
                for(int i = 0; i < NumOfProblems; i++){
                    Subproblem_Params Received_Params;
                    encoder.Decode_Solution(receivstream, Received_Params);
                    LocalTaskQueue.push_back(Received_Params);
                }

                Domain_Type newBoundValue;
                receivstream >> newBoundValue;
                // for debugging it should not be the case that anyone sends a worse bound then what we already have
                if(((bool)goal && newBoundValue >= LocalBestBound)
                   || (!(bool)goal && newBoundValue <= LocalBestBound)){
                    LocalBestBound = newBoundValue;
                }

                while(!LocalTaskQueue.empty()){
	                NumProblemsSolved++;
                    //take out one element from queue, expand it
                    Subproblem_Params sol = GetNextSubproblem(LocalTaskQueue, this->mode);

                    //ignore if its bound is worse than already known best sol.
                    auto [LowerBound, UpperBound] = Problem_Def.GetEstimateForBounds(prob, sol);
                    if (((bool) goal && LowerBound < LocalBestBound)
                        || (!(bool) goal && LowerBound > LocalBestBound)) {
                        ProblemsEliminated++;
                        continue;
                    }

                    // try to make the bound better only if the solution lies in a feasible domain
                    auto Feasibility = Problem_Def.IsFeasible(prob, sol);
                    Domain_Type CandidateBound;
                    bool IsPotentialCandidate = false;
                    if (Feasibility == BnB::FEASIBILITY::Full) {
                        CandidateBound = (Problem_Def.GetContainedUpperBound(prob, sol));
                        if (((bool) goal && CandidateBound >= LocalBestBound)
                            || (!(bool) goal && CandidateBound <= LocalBestBound)) {
                            IsPotentialCandidate = true;
                            LocalBestBound = CandidateBound;
                        }
                    } else if(Feasibility == BnB::FEASIBILITY::PARTIAL){
                        // use our backup for the CandidateBound
                        CandidateBound = UpperBound;
                    }
                    else if (Feasibility == BnB::FEASIBILITY::NONE) // basically discard again
                        continue;

                    //printProc("CandidateBound = " << CandidateBound << " LowerBound = " << LowerBound);
                    std::vector<Subproblem_Params> v;
                    if(std::abs(CandidateBound - LowerBound) > this->eps){ // epsilon criterion for convergence
                        v = Problem_Def.SplitSolution(prob, sol);
                        for(auto&& el : v) {
                            LocalTaskQueue.push_back(el);

                        }
		            }else if(IsPotentialCandidate){
                        BestSubproblem = sol;
                    }

                    // request master for slaves
                    if(counter % this->Communication_Frequency == 0 and !RequestOngoing) {
                        counter = 0;
                        sendstreams[0].str("");
                        sendstreams[0] << LocalBestBound << " ";
                        sendstreams[0] << (int) LocalTaskQueue.size() << " ";
                        MPI_Isend(&sendstreams[0].str()[0], strlen(sendstreams[0].str().c_str()), MPI_CHAR, 0,
                                  Default::MessageType::GET_SLAVES, MPI_COMM_WORLD, &SlaveReq);
                        RequestOngoing = true;
                    }


                    if(RequestOngoing) {
                        int flag = 0;
                        MPI_Test(&SlaveReq, &flag, MPI_STATUS_IGNORE);
                        if(flag == 1) {
                            RequestOngoing = false;
                            //get master's response
                            MPI_Recv(buffer,200, MPI_CHAR, 0, Default::MessageType::GET_SLAVES, MPI_COMM_WORLD, &st);
                            receivstream.str(buffer);
                            int slaves_avbl;
                            receivstream >> LocalBestBound;
                            receivstream >> slaves_avbl;

                            for(int i=0; i<slaves_avbl; i++){
                                //give a problem to each slave
                                int sl_no;
                                receivstream >> sl_no;
                                sendstreams[sl_no].str("");
                                int RestSize = std::min(PackageSize, (int)LocalTaskQueue.size());
                                if(RestSize == 0) continue;
                                sendstreams[sl_no] << RestSize << " ";
                                int ProblemsInBuffer = 0;
                                while(ProblemsInBuffer != RestSize){
                                    if(LocalTaskQueue.empty()){
                                        sendstreams[sl_no].str("");
                                        sendstreams[sl_no] << 0 << " ";
                                        break;
                                    }
                                    Subproblem_Params SubProb;

                                    ///if(this->mode == TraversalMode::DFS){
                                    ///    SubProb = LocalTaskQueue.back(); LocalTaskQueue.pop_back();
                                    ///}else if( this->mode == TraversalMode::BFS){
                                    SubProb = LocalTaskQueue.front(); LocalTaskQueue.pop_front();
                                    ///}

                                    auto[lower, upper] = Problem_Def.GetEstimateForBounds(prob, SubProb);
                                    if (((bool) goal && lower < LocalBestBound)
                                        || (!(bool) goal && lower > LocalBestBound)){
                                        continue;
                                    }
                                    encoder.Encode_Solution(sendstreams[sl_no], SubProb);
                                    ProblemsInBuffer++;

                                }
                                sendstreams[sl_no] << LocalBestBound<< " ";
                                //send it to idle processor
                                MPI_Isend(&sendstreams[sl_no].str()[0],strlen(sendstreams[sl_no].str().c_str()), MPI_CHAR, sl_no,
                                          Default::MessageType::PROB, MPI_COMM_WORLD, &req[sl_no]);
                            }
                        }
                    }

                    counter++;
                }
                //This slave has now become idle (its queue is empty). Inform master.
                sendstreams[num-1].str("");
                MPI_Send(&sendstreams[num - 1].str()[0],10,MPI_CHAR,0,Default::MessageType::IDLE, MPI_COMM_WORLD);
            }
            else if(st.MPI_TAG == Default::MessageType::FINISH){
		        printProc("I have solved " << NumProblemsSolved << " problems and eliminated" << ProblemsEliminated);
                break; // empty solution only master will return a meaningful solution
            }
        }
    }

    // cleanup

    /*for(int i = 1; i < num;i++)
        if(req[i] !=  MPI_REQUEST_NULL)
            MPI_Cancel(&req[i]);*/
/*    for(auto& request : req){
        if(request!=  MPI_REQUEST_NULL)
            MPI_Cancel(&request);
    }*/

    // ------------------------ ALL procs have a best solution now master has to gather it
    return ExtractBestSolution<Prob_Consts,Subproblem_Params, Domain_Type>(sendstreams[0], receivstream,
                               BestSubproblem,
                               Problem_Def,
                               prob,
                               encoder,
                               goal);
}