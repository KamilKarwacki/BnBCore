#pragma once

#include "Base.h"

// strategy pattern that holds the actual MPI algorithm to schedule the work

template<typename Prob_Consts, typename Subproblem_Params, typename Domain_Type>
class MPI_Scheduler
{
public:
    virtual Subproblem_Params Execute(const Problem_Definition<Prob_Consts, Subproblem_Params>& Problem_Def,
                         const Prob_Consts& prob,
                         const MPI_Message_Encoder<Subproblem_Params>& encoder,
                         const Goal goal,
                         const Domain_Type WorstBound) = 0;

    void CommFrequency(int num){Communication_Frequency = num;}
    void Threads(size_t num) {OpenMPThreads = num;}

protected:
    int Communication_Frequency = 1;

    // only for Hybrid
    int OpenMPThreads = 1;
};

// free function for post processing the output
// all slaves send their solutions to the master and he decides which one is the best one
template<typename Subproblem_Params>
Subproblem_Params PostProcessSolution(std::vector<Subproblem_Params> SlaveSolutions)
{
    MPI_Message_Encoder<Subproblem_Params> encoder;
    char buffer [300000];
    std::stringstream stream("");
    int me;
    int size;
    MPI_Comm_rank(MPI_COMM_WORLD, &me);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Status st;
    if(me == 0)
    {
        std::vector<Subproblem_Params> Solutions;
        for(int i = 1; i < size; i++)
        {
            MPI_Recv(buffer, 300000, MPI_CHAR, i, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
            stream.str(buffer);
            int NumOfSolutions;
            stream >> NumOfSolutions;
            for(int j = 0; j < NumOfSolutions; j++)
            {
                Subproblem_Params Sol;
                encoder.Decode_Solution(stream, Sol);
                Solutions.push_back(Sol);
            }
        }
        std::cout << "there are " << Solutions.size() << " solutions" << std::endl;
        return Solutions[0];
    } else
    {
        stream << (int)SlaveSolutions.size() << " ";
        for(const auto& sol : SlaveSolutions)
            encoder.Encode_Solution(stream, sol);
        MPI_Send(&stream.str()[0],strlen(stream.str().c_str()), MPI_CHAR,0 ,0 ,MPI_COMM_WORLD);
        return Subproblem_Params(); // empty return
    }
}

