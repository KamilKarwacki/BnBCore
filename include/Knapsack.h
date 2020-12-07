#pragma once
#include "Base.h"

// factory function that returns a toy problem
// used for testing solvers, and as an example of how to use the library

namespace BnB::Knapsack
{
    namespace Detail{
        struct item{
            int wt, cost;
            double ratio;
        };

        bool comp(item a, item b){
            return a.ratio > b.ratio;
        }

        std::vector<int> GenerateWeights()
        {
            return {2,2,2,2,8};
        }

        std::vector<int> GenerateCosts()
        {
            return {5,5,5,5,100};
        }

    }

    using Consts = Problem_Constants<
            std::vector<int>, // Weights
            std::vector<int>, // Costs
            int>;             // Capacity of the Bag

    using Params = Subproblem_Parameters<
            std::vector<int>, // Taken objects
            int,              // weight of config
            int>;             // cost of config


    Consts GenerateProblemConstants()
    {
        // NOTE: Params are not needed to be constructed now
        // The initial subproblem will be constructed by Problem_Definition.GetInitialSubproblem()
        // constants will be initialized before solver is run
        Consts consts;
        std::get<0>(consts) = Detail::GenerateWeights();
        std::get<1>(consts) = Detail::GenerateCosts();
        std::get<2>(consts) = 1; // max weight

        return consts;
    }


    Problem_Definition<Consts, Params> GenerateToyProblem()
    {
        using namespace Detail;
        // object that will hold the functions called by the solver
        Problem_Definition<Consts, Params> KnapsackProblem;

        // you can assume that the subproblem is not feasible yet so it can be split
        KnapsackProblem.SplitSolution = [](const Consts& consts, const Params & params){
            int ObjectIndex = std::get<0>(params).size();
            Params s1 = params;
            Params s2 = params;
            std::get<0>(s1).push_back(0);
            std::vector<Params> ret;
            ret.push_back(s1); // without taking object at index pos
            if(std::get<0>(consts)[ObjectIndex] + std::get<1>(params) <= std::get<2>(consts) ){
                std::get<0>(s2).push_back(1);
                std::get<1>(s2) += std::get<0>(consts)[ObjectIndex];
                std::get<2>(s2) += std::get<1>(consts)[ObjectIndex];
                ret.push_back(s2); // taking object at index pos
            }
            return ret;
        };

        KnapsackProblem.Discard = [](const Consts& consts, const Params& params, const std::variant<int, float, double> CurrentBound){
            int w = std::get<1>(params);
            int cost = std::get<2>(params);
            std::vector<int> weights = std::get<0>(consts);
            std::vector<int> costs = std::get<1>(consts);
            std::vector<item> rem; // remaining items

            for(int i=std::get<0>(params).size(); i < weights.size(); i++){
                item it;
                it.wt = weights[i];
                it.cost = costs[i];
                it.ratio = static_cast<double>(it.cost) / static_cast<double>(it.wt);
                rem.push_back(it);
            }

            int bound = cost;
            std::sort(rem.begin(),rem.end(),comp); // decreasing order of ratio of cost to weight;
            for(int j=0;j<rem.size() && w + rem[j].wt <= std::get<2>(consts);j++){
                //taking items till weight is exceeded
                bound += rem[j].cost;
                w += rem[j].wt;
            }

            return bound < std::get<int>(CurrentBound);
        };

        KnapsackProblem.GetBound = [](const Consts& consts, const Params& params){
            int w = std::get<1>(params);
            int cost = std::get<2>(params);
            std::vector<int> weights = std::get<0>(consts);
            std::vector<int> costs = std::get<1>(consts);
            std::vector<item> rem; // remaining items

            for(int i=std::get<0>(params).size(); i < weights.size(); i++){
                item it;
                it.wt = weights[i];
                it.cost = costs[i];
                it.ratio = static_cast<double>(it.cost) / static_cast<double>(it.wt);
                rem.push_back(it);
            }

            int bound = cost;
            std::sort(rem.begin(),rem.end(),comp); // decreasing order of ratio of cost to weight;
            for(int j=0;j<rem.size() && w + rem[j].wt <= std::get<2>(consts);j++){
                //taking items till weight is exceeded
                bound += rem[j].cost;
                w += rem[j].wt;
            }
            return bound;
        };

        KnapsackProblem.IsFeasible = [](const Consts& consts, const Params& params){
            return std::get<0>(consts).size() == std::get<0>(params).size();
        };

        KnapsackProblem.PrintSolution = [](const Params& params){
            sleep(2);
            std::cout << "The optimal solution has cost: " << std::get<2>(params) << std::endl;
            for(const auto& el : std::get<0>(params))
            {
                std::cout << "item nr: " << el << std::endl;
            }
        };

        KnapsackProblem.GetInitialSubproblem = [](const Consts& prob){
           Params initial;
           // vector will be empty
           // cost needs to be set to 0
           std::get<1>(initial) = 0;
           std::get<2>(initial) = 0;
           return initial;
        };

        return KnapsackProblem;
    }
}