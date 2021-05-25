#pragma once

#include <climits>
#include "Base.h"

// factory function that returns a TSP problem definition
// used for testing solvers, and as an example of how to use the library

namespace BnB::TSP {
    using Consts = BnB::Problem_Constants<
            std::vector<std::vector<int>> // connectivity Matrix
            >;

    using Params = BnB::Subproblem_Parameters<
            std::vector<std::pair<int,int>>, // list of traveled nodes
            std::vector<std::vector<int>>,
            int, // lower bound
            int, // cost
            int,// vertex
            int // upper bound
    >;

    const int INF = 1e8;

    int minKey(int key[], bool mstSet[], int V) {
        // Initialize min value
        int min = INT_MAX, min_index;

        for (int v = 0; v < V; v++)
            if (mstSet[v] == false && key[v] < min)
                min = key[v], min_index = v;

        return min_index;
    }


    std::vector<std::vector<int>> GetReducedGraph(const Params& params){
        std::vector<int> nodesLeft;
        std::vector<int> nodesTaken;

        nodesTaken.push_back(0);
        for(const auto& con : std::get<0>(params))
            nodesTaken.push_back(con.second);

        for(int i = 0; i < std::get<1>(params).size(); i++)
            nodesLeft.push_back(i);

        for(auto el : nodesTaken)
            nodesLeft.erase(std::remove(nodesLeft.begin(), nodesLeft.end(), el), nodesLeft.end());

        int newSize = nodesLeft.size();

        std::vector<std::vector<int>> newGraph(newSize);
        for(int i = 0; i < newSize; i++){
            newGraph[i].resize(newSize);
            for(int j = 0; j < newSize; j++)
            {
                if(i == j)
                    newGraph[i][j] = INF;
                else
                    newGraph[i][j] = std::get<1>(params)[nodesLeft[i]][nodesLeft[j]];
            }
        }

        return newGraph;
    }

    // found on the internet
    int primMST(std::vector<std::vector<int>> graph)
    {
        int V = graph.size();
        // Array to store constructed MST
        int parent[V];

        // Key values used to pick minimum weight edge in cut
        int key[V];

        // To represent set of vertices included in MST
        bool mstSet[V];

        // Initialize all keys as INFINITE
        for (int i = 0; i < V; i++)
            key[i] = INT_MAX, mstSet[i] = false;

        // Always include first 1st vertex in MST.
        // Make key 0 so that this vertex is picked as first vertex.
        key[0] = 0;
        parent[0] = -1; // First node is always root of MST

        // The MST will have V vertices
        for (int count = 0; count < V - 1; count++)
        {
            // Pick the minimum key vertex from the
            // set of vertices not yet included in MST
            int u = minKey(key, mstSet,V);

            // Add the picked vertex to the MST Set
            mstSet[u] = true;

            // Update key value and parent index of
            // the adjacent vertices of the picked vertex.
            // Consider only those vertices which are not
            // yet included in MST
            for (int v = 0; v < V; v++)

                // graph[u][v] is non zero only for adjacent vertices of m
                // mstSet[v] is false for vertices not yet included in MST
                // Update the key only if graph[u][v] is smaller than key[v]
                if (graph[u][v] != INF && mstSet[v] == false && graph[u][v] < key[v])
                    parent[v] = u, key[v] = graph[u][v];
        }


        int lowerBound = 0;
        for (int i = 1; i < V; i++)
            lowerBound += graph[i][parent[i]];
        return lowerBound;
    }



    void NullifyZero(std::vector<std::vector<int>>& matrix){
        for(auto& row : matrix){
            row[0] = INF;
        }
    }

    void NullifyEntry(std::vector<std::vector<int>>& matrix, int i, int j){

        for(int I = 0; I < matrix.size(); I++)
            matrix[I][j] = INF;

        for(int J = 0; J < matrix.size(); J++){
            matrix[i][J] = INF;
        }
    }

    int GreedyUpper(const Consts& consts, const Params& params){

        int Upper = std::get<3>(params);

        std::vector<std::vector<int>> tempCityMap = std::get<1>(params);
        int from = std::get<4>(params);
        for(int i = 0; i < std::get<0>(consts).size() - std::get<0>(params).size()-1; i++){
            int bestVertex = -1;
            int SmallestRoute = INF - 1;
            for(int j = 0; j < std::get<0>(consts).size(); j++){
                if(i == std::get<0>(consts).size() - std::get<0>(params).size() - 2){
                    if(tempCityMap[from][j] + std::get<0>(consts)[j][0]  < SmallestRoute){
                        bestVertex = j;
                        SmallestRoute = tempCityMap[from][j] + std::get<0>(consts)[j][0];
                    }
                }else
                if(tempCityMap[from][j] < SmallestRoute){
                    bestVertex = j;
                    SmallestRoute = tempCityMap[from][j];
                }
            }
            NullifyEntry(tempCityMap,from, bestVertex);
            from = bestVertex;
            Upper+= SmallestRoute;
            assert(bestVertex != -1);
        }

        return Upper;
    }


    Problem_Definition<Consts, Params, int> GenerateToyProblem() {
        // object that will hold the functions called by the solver
        Problem_Definition<Consts, Params, int> TSPProblem;

        // you can assume that the subproblem is not feasible yet so it can be split
        TSPProblem.SplitSolution = [](const Consts &consts, const Params &params) {
            std::vector<Params> ret;


            int from = std::get<4>(params);

            for(int to = 0; to < std::get<0>(consts).size(); to++){
                if(std::get<1>(params)[from][to] != INF) {
                    Params subproblem = params;
                    std::get<0>(subproblem).push_back({from, to});
                    NullifyEntry(std::get<1>(subproblem),from,to);
                    std::get<4>(subproblem)++;

                    std::get<3>(subproblem) += std::get<0>(consts)[from][to];
                    if(std::get<0>(subproblem).size() == std::get<0>(consts).size() - 1)
                        std::get<3>(subproblem) += std::get<0>(consts)[to][0];
                    assert(std::get<3>(subproblem) < INF);
                    std::get<4>(subproblem) = to;
                    std::get<5>(subproblem) = GreedyUpper(consts, subproblem);
                    ret.push_back(subproblem);
                }
            }

            return ret;
        };


        TSPProblem.GetEstimateForBounds = [](const Consts &consts, const Params &params) {
            int lower = std::get<3>(params) + primMST(GetReducedGraph(params));
            return std::tuple<int, int>(lower, 0);
        };


        TSPProblem.GetContainedUpperBound = [](const Consts &consts, const Params &params) {
            return std::get<5>(params);
        };


        TSPProblem.IsFeasible = [](const Consts &consts, const Params &params) {
            return BnB::FEASIBILITY::Full;
        };


        TSPProblem.PrintSolution = [](const Params &params) {
            for(const auto& el : std::get<0>(params))
                std::cout << el.first << "->" << el.second << " ";
            std::cout << std::endl;
            std::cout <<  std::get<3>(params)<< std::endl;
        };


        TSPProblem.GetInitialSubproblem = [](const Consts &prob) {
            Params initial;
            std::get<1>(initial) = std::get<0>(prob);
            NullifyZero(std::get<1>(initial));

            std::get<5>(initial) = GreedyUpper(prob, initial);

            return initial;
        };

        return TSPProblem;
    }


    /*                     Problem generators and pawrsing tools                                      */

    std::vector<std::vector<int>> GenerateSmallMatrix(){
        std::vector<std::vector<int>> system=
    /*            {{ INF, 10, 8, 9, 7 },
        { 10, INF, 10, 5, 6 },
        { 8, 10, INF, 8, 9 },
        { 9, 5, 8, INF, 6 },
        { 7, 6, 9, 6, INF }};*/
                /*{{INF, 3, 1, 5, 8},
        {3, INF, 6, 7, 9},
        {1, 6, INF, 4, 2},
        {5, 7, 4, INF, 3},
        {8, 9, 2, 3, INF}};*/
                {{INF,1,2,3},
                 {1,INF,2,1},
                 {2,2,INF,5},
                 {3,1,5,INF}};

       return system;
    }


    std::vector<std::vector<int>> GenerateMatrixFromFile(std::string filename, int n){
        std::vector<std::vector<int>> system(n);
        std::fstream file;
        file.open(filename);
        for(int i = 0; i < n;i++){
            for(int j = 0; j < n; j++){
                int val;
                file >> val;
                if(i == j){
                    system[i].push_back(INF);
                }else{
                   system[i].push_back(val);
                }
            }
        }
        return system;
    }



    std::vector<std::vector<int>> GenerateRandomMatrix(int n, std::pair<int,int> range, int seed){
        std::vector<std::vector<int>> system(n);
        for(int i = 0; i < n; i++){
            system[i].resize(n);
        }

        static std::mt19937 mt(seed);
        std::uniform_int_distribution<int> dist(range.first, range.second);
        for(int i = 0; i < n ;i++){
            for(int j = 0; j < n; j++){
                if(i == j)
                    system[i][j] = INF;
                else{
                    int val = dist(mt);
                    system[i][j] = val;
                    system[j][i] = val;
                }
            }
        }
        return system;
    }
}




