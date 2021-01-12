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

            std::vector<Params> subProblems;
            auto objects = std::get<0>(params);
            auto weights = std::get<0>(consts);
            auto costs = std::get<1>(consts);
            for(int i = 0; i < objects.size(); i++)
            {
                Params subProb;
                std::vector<int> indices;
                int w = 0;
                int c = 0;
                for(int j = 0; j < objects.size(); j++)
                {
                    if(i != j){
                        w += weights[objects[i]];
                        c += costs[objects[i]];
                        indices.push_back(objects[i]);
                    }
                }
                std::get<0>(subProb) = indices;
                std::get<1>(subProb) = w;
                std::get<2>(subProb) = c;
                subProblems.push_back(subProb);
            }
            return subProblems;
        };

        KnapsackProblem.Discard = [](const Consts& consts, const Params& params, const std::variant<int, float, double> CurrentBound){
           std::vector<int> objects = std::get<0>(params);
           std::vector<int> weights = std::get<0>(consts);

           int sumW = 0;
           for(const auto& object : objects)
               sumW += weights[object];

           return sumW > std::get<2>(consts);
        };

        KnapsackProblem.GetBound = [](const Consts& consts, const Params& params){
            int w = std::get<1>(params);
            int cost = std::get<2>(params);
            std::vector<int> weights = std::get<0>(consts);
            std::vector<int> costs = std::get<1>(consts);
            std::vector<int> objects = std::get<0>(params);
            for(const auto&  object : objects){
                if(weights[object] < std::get<2>(consts))
                    return weights[object];
            }

/*
            for(size_t i=std::get<0>(params).size(); i < weights.size(); i++){
                item it;
                it.wt = weights[i];
                it.cost = costs[i];
                it.ratio = static_cast<double>(it.cost) / static_cast<double>(it.wt);
                rem.push_back(it);
            }

            int bound = cost;
            std::sort(rem.begin(),rem.end(),comp); // decreasing order of ratio of cost to weight;
            for(size_t j=0;j<rem.size() && w + rem[j].wt <= std::get<2>(consts);j++){
                //taking items till weight is exceeded
                bound += rem[j].cost;
                w += rem[j].wt;
            }
            return bound;*/
        };

        KnapsackProblem.IsFeasible = [](const Consts& consts, const Params& params){
            return std::get<1>(params) <= std::get<2>(consts);
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
           std::get<0>(initial) = {0,1,2,3,4};
           std::get<1>(initial) = 0;
           std::get<2>(initial) = 0;
           return initial;
        };

        return KnapsackProblem;
    }
}