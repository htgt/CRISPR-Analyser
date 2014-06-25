#ifndef __UTILS__
#define __UTILS__

#include <vector>
#include <string>
#include <sstream>

#define ERROR_STR 0xFFFFFFFFFFFFFFFFull

namespace util {
    //unsigned int pop_count(uint64_t x);
    std::vector<std::string> split(const std::string & text);
    void print_binary(uint64_t data);
    std::string bits_to_string(uint64_t text, int size);
    uint64_t string_to_bits(uint8_t * cmap, const std::string & seq, short base);
    uint64_t revcom(uint64_t text, int size);
    void lc(std::string & str);

/**
 * @brief Count the number of bits set to 1 in a given 64bit integer
 * this is about twice as fast as __builtin_popcountll
 * inlining seems to shave a bit more off
 *
 * @param x    the integer to count
 * @return the number of bits set to 1 as an unsigned integer
 */
    inline unsigned int pop_count(uint64_t x) {
        //as everything is two bit we must convert them all to one bit,
        //to do this we must turn off all MSBs, but before we can do that
        //we need to ensure that when an MSB is set to 1, the LSB is also set.
        //the 4 will change pam_right to 0 so it doesnt get counted.
        //5 is 0101, 4 is 0100
        //x = (x | (x >> 1)) & (0x5555545555555555ull);
        x = (x | (x >> 1)) & (0x5555555555555555ull);

        x = x-( (x>>1) & 0x5555555555555555ull);
        x = (x & 0x3333333333333333ull) + ((x >> 2) & 0x3333333333333333ull);
        x = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0full;
        return  (0x0101010101010101ull*x >> 56);
    }

/**
 * @brief Create a JSON string representation of a container (e.g. vector)
 * Implemented here because templates suck. otherwise I'd have to declare
 * every type I support manually
 *
 * @warning apparently stringstreams are slow
 *
 * @param begin          iterator begin, pointing to a container
 * @param end            iterator end
 * @param include_idx    whether or not the index should be included
 *
 * @return JSON string
 */
    template <typename T>
    std::string array_to_string(T begin, T end, bool include_idx) {
        std::ostringstream oss;

        //if its empty return
        if ( begin == end )
            return oss.str();

        oss << "{" << (include_idx ? "0: " : "")  << *(begin++);

        //iterate over array, appending each value to the string
        for ( int i = 1; begin < end; ++i ) {
            oss << ",";
            if ( include_idx )
                oss << " " << i << ": ";
            oss << *(begin++);
        }

        oss << "}";

        return oss.str();
    }

    template <class T>
    inline std::string to_string (const T & t) {
        std::stringstream ss;
        ss << t;
        return ss.str();
    }

    template <typename T>
    std::string to_json_array(const T & list) {
        std::string result = "[";

        for ( uint64_t i = 0; i < list.size(); i++ ) {
            result += to_string(list[i]);
            if ( i != list.size()-1 ) result += ",";
        }

        result += "]";

        return result;
    }

}

#endif
