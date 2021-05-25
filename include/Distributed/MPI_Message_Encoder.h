#pragma once
#include <sstream>
#include <cassert>
#include <vector>
#include <tuple>
#include <iostream>

// not in the namespace so the user can easily define his own function
template<typename T>
void encodeParam(std::stringstream &ss, const T &p) {
    assert(false); // if this is called it means that the encoder doesnt know how to encode the current parameter T
}

template<typename T>
void decodeParam(std::stringstream &ss, T &p) {
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    assert(false); // if this is called it means that the encoder doesnt know how to encode the current parameter T
}

template<class Solution_Parameters>
class MPI_Message_Encoder {
public: // make it a friend of Solver maybe
    void Encode_Solution(std::stringstream &ss, const Solution_Parameters &params) const {
        // takes ss stream by reference
        // lambda takes a list of args, these args are all elements of the tuple
        // the lambda body uses the special syntax to call encode param for all tuple parameters
        std::apply([&ss](auto &&... args) { (encodeParam(ss, args), ...); },
                   params); // applies encode parameter on all elements
    }

    void Decode_Solution(std::stringstream &ss, Solution_Parameters &params) const {
        // takes ss stream by reference
        // lambda takes a list of args, these args are all elements of the tuple
        // the lambda body uses the special syntax to call encode param for all tuple parameters
        std::apply([&ss](auto &&... args) { (decodeParam(ss, args), ...); },
                   params); // applies encode parameter on all elements
    }
};

// --------------------------------------------------------------------------
// specializations for different basic types including vectors of these types
// TODO C++20 concepts
// TODO develop some automatic way to create MPI_Types
// --------------------------------------------------------------------------
template<>
void encodeParam(std::stringstream &ss, const int &p) {
    ss << p << " ";
}

template<>
void decodeParam(std::stringstream &ss, int &p) {
    ss >> p;
}

template<>
void encodeParam(std::stringstream &ss, const float &p) {
    ss << p << " ";
}

template<>
void decodeParam(std::stringstream &ss, float &p) {
    ss >> p;
}

template<>
void encodeParam(std::stringstream &ss, const double &p) {
    ss << p << " ";
}

template<>
void decodeParam(std::stringstream &ss, double &p) {
    ss >> p;
}


template<>
void encodeParam(std::stringstream &ss, const std::vector<int> &p) {
    size_t len = p.size();
    ss << len << " ";

    for (size_t i = 0; i < len; i++) {
        ss << p[i] << " ";
    }
}


template<>
void decodeParam(std::stringstream &ss, std::vector<int> &p) {
    size_t len;
    ss >> len;
    p.resize(len);

    for (size_t i = 0; i < len; i++) {
        ss >> p[i];
    }
}

// ----------------------------------- Matrix for TSP ---------------------------------------- //
template<>
void encodeParam(std::stringstream &ss, const std::vector<std::vector<int>> &p) {
    size_t N = p.size();
    size_t M = p[0].size();
    ss << N << " " << M << " ";

    for (size_t i = 0; i < N; i++) {
        for (size_t j = 0; j < M; j++)
            ss << p[i][j] << " ";
    }
}


template<>
void decodeParam(std::stringstream &ss, std::vector<std::vector<int>> &p) {
    size_t N;
    size_t M;
    ss >> N >> M;
    p.resize(N);

    for (size_t i = 0; i < N; i++) {
        p[i].resize(M);
        for (size_t j = 0; j < M; j++)
            ss >> p[i][j];
    }
}


template<>
void encodeParam(std::stringstream &ss, const std::vector<std::pair<int, int>> &p) {
    size_t s = p.size();
    ss << s << " ";
    for (const auto &i : p)
        ss << i.first << " " << i.second << " ";
}


template<>
void decodeParam(std::stringstream &ss, std::vector<std::pair<int, int>> &p) {
    size_t size;
    ss >> size;
    p.resize(size);
    for (size_t i = 0; i < size; i++) {
        double u, l;
        ss >> l >> u;
        p[i] = {l, u};
    }
}

