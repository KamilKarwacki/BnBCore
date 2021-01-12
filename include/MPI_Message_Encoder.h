#pragma once
#include <sstream>
#include <cassert>
#include <vector>
#include <tuple>
#include <iostream>

template<typename T>
void encodeParam(std::stringstream& ss, const T &p)
{
    assert(false); // if this is called it means that the encoder doesnt know how to encode the current parameter T
}

template<typename T>
void decodeParam(std::stringstream& ss, T &p)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    assert(false); // if this is called it means that the encoder doesnt know how to encode the current parameter T
}

template<class Solution_Parameters>
class MPI_Message_Encoder
{
public: // make it a friend of Solver maybe
   void Encode_Solution(std::stringstream& ss, const Solution_Parameters& params) const
   {
        // takes ss stream by reference
        // lambda takes a list of args, these args are all elements of the tuple
        // the lambda body uses the special syntax to call encode param for all tuple parameters
        std::apply([&ss](auto&&... args) {(encodeParam(ss,args), ...);}, params); // applies encode parameter on all elements
   }

   void Decode_Solution(std::stringstream& ss, Solution_Parameters& params) const
   {
       // takes ss stream by reference
       // lambda takes a list of args, these args are all elements of the tuple
       // the lambda body uses the special syntax to call encode param for all tuple parameters
       std::apply([&ss](auto&&... args) {(decodeParam(ss, args), ...);}, params); // applies encode parameter on all elements
   }
};

// --------------------------------------------------------------------------
// specializations for different basic types including vectors of these types
// TODO matrices of basic types
// TODO C++20 concepts
// --------------------------------------------------------------------------
template<>
void encodeParam(std::stringstream& ss, const int& p)
{
    ss << p << " ";
}

template<>
void decodeParam(std::stringstream& ss, int &p)
{
    ss >> p;
}

template<>
void encodeParam(std::stringstream& ss, const float& p)
{
    ss << p << " ";
}

template<>
void decodeParam(std::stringstream& ss, float &p)
{
    ss >> p;
}

template<>
void encodeParam(std::stringstream& ss, const double& p)
{
    ss << p << " ";
}

template<>
void decodeParam(std::stringstream& ss, double &p)
{
    ss >> p;
}


template<>
void encodeParam(std::stringstream& ss, const std::vector<int>& p)
{
    size_t len = p.size();
    ss << len << " ";

    for(size_t i = 0; i < len; i++)
    {
        ss << p[i] << " ";
    }
}

template<>
void decodeParam(std::stringstream& ss, std::vector<int>& p)
{
    size_t len;
    ss >> len;
    p.resize(len);

    for(size_t i = 0; i < len; i++)
    {
        ss >> p[i];
    }
}


