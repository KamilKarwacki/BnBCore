#pragma once
#include <sstream>
#include <cassert>
#include <vector>
#include <tuple>

template<typename T>
void encodeParam(std::stringstream& ss, T p)
{
    assert(false); // if this is called it means that the encoder doesnt know how to encode
}

template<typename T>
void decodeParam(std::stringstream& ss, T &p)
{
    assert(false); // if this is called it means that the encoder doesnt know how to encode
}

template<class Solution_Parameters>
class MPI_Message_Encoder
{
public: // make it a friend of Solver maybe
   void Encode_Solution(std::stringstream& ss, const Solution_Parameters& params)
   {
        // takes ss stream by reference
        // lambda takes a list of args, these args are all elements of the tuple
        // the lambda body uses the special syntax to call encode param for all tuple parameters
        std::apply([&ss](auto&&... args) {(encodeParam(ss,args), ...);}, params); // applies encode parameter on all elements
   }

   void Decode_Solution(std::stringstream& ss, Solution_Parameters& params)
   {
       // takes ss stream by reference
       // lambda takes a list of args, these args are all elements of the tuple
       // the lambda body uses the special syntax to call encode param for all tuple parameters
       std::apply([&ss](auto&&... args) {(decodeParam(ss, args), ...);}, params); // applies encode parameter on all elements
   }
};


template<>
void encodeParam<int>(std::stringstream& ss, int p)
{
    ss << p << " ";
}

template<>
void decodeParam<int>(std::stringstream& ss, int &p)
{
    ss >> p;
}

template<>
void encodeParam<float>(std::stringstream& ss, float p)
{
    ss << p << " ";
}

template<>
void encodeParam<double>(std::stringstream& ss, double p)
{
    ss << p << " ";
}

template<>
void encodeParam<std::vector<int>>(std::stringstream& ss, std::vector<int> p)
{
    size_t len = p.size();
    ss << len << " ";

    for(int i = 0; i < len; i++)
    {
        ss << p[i] << " ";
    }
}

