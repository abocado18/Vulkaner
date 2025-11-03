//My Bitset Implementation
#pragma once
#include <iostream>
#include <cinttypes>
#include <vector>

namespace bit
{
    class Bitset
    {

    public:
        Bitset(size_t number_of_bits) : number_of_bits(number_of_bits)
        {
            bitset.resize(getSizeOfVector(), 0);
        }

        ~Bitset() = default;

        inline bool checkBit(size_t position) const
        {
            size_t vector_index = position / 64;
            size_t offset = position % 64;

            if (vector_index + 1 > bitset.size())
                return false;

            return (bitset[vector_index] & ((uint64_t)1 << offset)) != 0;
        }

        inline void setBit(size_t position, bool value)
        {
            size_t vector_index = position / 64;
            size_t offset = position % 64;

            if (vector_index >= bitset.size())
            {
                bitset.resize(vector_index + 1, 0);
                number_of_bits = bitset.size() * 64;
            }

            if (value)
                bitset[vector_index] |= ((uint64_t)1 << offset);
            else
                bitset[vector_index] &= ~((uint64_t)1 << offset);
        }

        inline const uint64_t getNumberOfBits() const
        {
            return number_of_bits;
        };

        Bitset operator&(const Bitset &other) const
        {
            Bitset result(std::max(number_of_bits, other.number_of_bits));
            size_t min_size = std::min(bitset.size(), other.bitset.size());
            for (size_t i = 0; i < min_size; ++i)
                result.bitset[i] = bitset[i] & other.bitset[i];
            return result;
        }

        Bitset operator|(const Bitset &other) const
        {
            Bitset result(std::max(number_of_bits, other.number_of_bits));
            size_t min_size = std::min(bitset.size(), other.bitset.size());
            for (size_t i = 0; i < min_size; ++i)
                result.bitset[i] = bitset[i] | other.bitset[i];
            return result;
        }

        bool any() const
        {
            for (auto val : bitset)
                if (val != 0)
                    return true;
            return false;
        }

    private:
        std::vector<uint64_t> bitset;

        inline const size_t getSizeOfVector() const
        {
            return (number_of_bits + 63) / 64;
        };

        uint64_t number_of_bits;
    };
}