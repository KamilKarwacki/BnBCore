#pragma once
#include "MPI_Message_Encoder.h"
#include <tuple>
#include <functional>
#include <iostream>

template<typename... Args>
using Problem_Constants = std::tuple<Args...>;
template<typename... Args>
using Solution_Parameters = std::tuple<Args...>;


template<typename Prob_Consts, typename Sol_Params>
class Problem_Definition
{
public:
    //virtual Sol GetInitialSolution(const Prob& prob) = 0;
    std::function<Sol_Params(const Prob_Consts& prob)> GetInitialSolution = nullptr;
    std::function<std::vector<Sol_Params>(const Prob_Consts& consts, const Sol_Params& params)> SplitSolution = nullptr;
    std::function<bool(const Sol_Params&)> IsFeasible = nullptr;





    // ... other functions ... //
};


template<typename Prob_Consts, typename Sol_Params>
class Solver
{
    MPI_Message_Encoder<Sol_Params> encoder;
    // bool dir;
    // void Maximize(prob){ dir = 1; Solve(prob)};
public:
    void Solve(const Problem_Definition<Prob_Consts, Sol_Params>& Problem_Def, Prob_Consts& prob)
    {
        Sol_Params s = Problem_Def.GetInitialSolution(prob);
        std::stringstream ss("");
        encoder.Encode_Solution(ss, s);
        std::cout << ss.str() << std::endl;

        // imagine im another processor
        Sol_Params s2;
        encoder.Decode_Solution(ss,s2);

        std::apply([](auto&&... args) {((std::cout << args << std::endl), ...);}, s2); // applies encode parameter on all elements

    }
};
