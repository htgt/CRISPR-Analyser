#ifndef __UTILS__
#define __UTILS__

#include <vector>
#include <string>
#include <sstream>
#include <deque>

#define ERROR_STR 0xFFFFFFFFFFFFFFFFull

namespace std {
    string to_string(const string & str);
}

namespace util {
    //unsigned int pop_count(uint64_t x);
    std::vector<std::string> split(const std::string & text);
    void print_binary(uint64_t data);
    std::string bits_to_string(uint64_t text, int size);
    uint64_t string_to_bits(uint8_t * cmap, const std::string & seq, short base);
    uint64_t revcom(uint64_t text, int size);
    std::string revcom(std::string text);
    void lc(std::string & str);
    bool valid_pam_left(std::deque<char> & seq, std::string pam);
    bool valid_pam_right(std::deque<char> & seq, std::string pam);
    bool valid_pam(std::deque<char> & seq, std::string pam, bool pam_right);

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
 *
 * @param begin          iterator begin, pointing to a container
 * @param end            iterator end
 * @param include_idx    whether or not the index should be included
 *
 * @return JSON string
 */
    template <typename T>
    std::string format_off_targets(const T & list) {
        //if its empty return
        if ( list.empty() )
            return "{}";

        //to make sure we don't have to re-allocate memory we reserve enough
        //space for the whole string.

        //each entry in list has a minimum of '0: 1,' which is 5 characters
        //more realistically it will be something like '4: 9999,' which is 8
        //plus {} which is 2
        //so this gives us a good approximation of the size
        std::string result = "{";
        result.reserve( (list.size()*8) + 2 );

        //iterate over array, appending each value to the string
        uint64_t index = 0;
        for ( auto it = list.begin(); it != list.end(); ++it ) {
            result.append(std::to_string(index++)).append(": ").append( std::to_string( *it ) );
            if (index != list.size()) result.append(", ");
        }

        result += "}";

        return result;
    }

    //useful for structs so you can override the <<
    template <class T>
    inline std::string to_string (const T & t) {
        std::stringstream ss;
        ss << t;
        return ss.str();
    }

    template <typename T>
    std::string to_json_array(const T & list, bool quote_all = false) {
        std::string result = "[";

        for ( size_t i = 0; i < list.size(); ++i ) {
            std::string temp = std::to_string(list[i]);
            //quote all would be set to true if you were passing in strings
            result += quote_all ? "'"+temp+"'" : temp;
            if ( i != list.size()-1 ) result += ",";
        }

        result += "]";

        return result;
    }

    template <typename T>
    std::string to_postgres_array(const T & list, bool quote_all = false) {
        std::string result = to_json_array(list, quote_all);
        result.front() = '{';
        result.back() = '}';

        return result;
    }

    template <typename T>
    std::string container_to_string(const T & list) {
        std::string result(list.size(), '-');

        for ( size_t i = 0; i < list.size(); ++i ) {
            result[i] = list[i];
        }

        return result;
    }

}

#endif
