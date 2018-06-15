#include <vector>
#include <iostream>
#include <array>
#include <string>
#include <sstream>
#include <bitset>
#include <climits>
#include <stdexcept>
#include <deque>
#include "utils.h"

//add a case to the std::to_string so we dont get errors when passing in a string
namespace std {
    string to_string(const string & str) {
        return str;
    }
}

namespace util {

/**
 * @brief Split a given string by a comma.
 *
 * @param text    the string to be split
 * @return a vector of strings
 */
    std::vector<std::string> split(const std::string & text) {
        std::stringstream stream (text);
        std::string segment;
        std::vector<std::string> items;

        while( getline(stream, segment, ',') ) {
            items.push_back(segment);
        }

        return items;
    }

/**
 * @brief Print the binary representation of a given 64bit integer to stderr
 *
 * @param data    64bit integer
 */
    void print_binary(uint64_t data) {
        std::bitset<64> a (data);
        std::cerr << a << "\n";
    }


    std::string bits_to_string(uint64_t text, int size) {
        //have to & 0x3 to turn off all bits but what we actually want.
        std::string s ( size, ' ' ); //make an empty string of the right size
        int shift = 2 * ( size - 1 ); //there are twice as many bits as there are characters

        //fill with N if its an error string (all bits set to 1)
        if ( text == ERROR_STR ) {
            s.assign( size, 'N' );
        }

        //extract each character from the text
        for ( int i = 0; i < size; i++, shift -= 2 ) {
            //put the character we're interested in at the very end
            //of the integer, and switch all remaining bits to 0 with & 0x3
            uint8_t character = (text >> shift) & 0x3;
            switch ( character ) {
                case 0: s[i] = 'A'; break;
                case 1: s[i] = 'C'; break;
                case 2: s[i] = 'G'; break;
                case 3: s[i] = 'T'; break;
                default: break;
            }
        }

        return s;
    }

    uint64_t string_to_bits(uint8_t * cmap, const std::string & seq, short base = 0) {
        //loop through each character
        //optionally allow the user to prepend the sequence with a short
        uint64_t bits = base;
        for ( std::string::size_type j = 0; j < seq.size(); j++ ) {
            uint8_t const c = seq[j]; //get char

            if ( cmap[c] == 4 ) {
                bits = ERROR_STR; //set to all 1s
                break;
            }
            else {
                bits <<= 2; //shift left to make room for new char
                bits |= cmap[c]; //add our new char to the end
            }
        }

        return bits;
    }

    void lc(std::string & str) {
        for ( uint i = 0; i < str.length(); ++i ) {
            str[i] = tolower(str[i]);
        }
    }

/**
 * @brief reverse complement a bitstring
 *
 * @param text    the bitstring to reverse complement
 * @param size    the length of the sequence
 *
 * @return uint64_t
 */
    uint64_t revcom(uint64_t text, int size) {
        //for a size of 23 we & with 23 1s to undo the complement on the part of
        //the integer we aren't interested in, for consistency
        //i.e. set the unused bits back to 0.
        //assumes size is smaller than sizeof(text)...
        unsigned int num_bits = sizeof(text) * CHAR_BIT;
        //we have to -1 from here to account for the pam_right bit which we DO want flipped
        uint64_t mask = 0xFFFFFFFFFFFFFFFFull >> ( (num_bits - (size * 2)) - 1 );
        //bit complement here maps A -> T, G -> C etc.
        text = ~text & mask;

        //start the reversed encoding off with the flipped PAM position
        uint64_t reversed = text >> (size * 2);
        int shift = 0;

        //now reverse the sequence, 2 bits at a time
        //could just do shift = 0; shift < size*2; shift += 2
        for ( int i = 0; i < size; i++, shift += 2 ) {
            reversed <<= 2;
            reversed |= ( text >> shift ) & 0x3;
        }

        return reversed;
    }

    //we only need this for the PAM site so no point adding all the weird characters
    std::string revcom(std::string text) {
        std::string rev( text.size(), 'N' );
        int j = 0;
        for ( std::string::reverse_iterator it = text.rbegin(); it != text.rend(); ++it ) {
            switch( *it ) {
                case 'A': rev[j++] = 'T'; break;
                case 'T': rev[j++] = 'A'; break;
                case 'G': rev[j++] = 'C'; break;
                case 'C': rev[j++] = 'G'; break;
                //maybe i'll support these later
                // case 'R': rev[j++] = 'Y'; break;
                // case 'Y': rev[j++] = 'R'; break;
                // case 'S': rev[j++] = 'S'; break;
                // case 'W': rev[j++] = 'W'; break;
                // case 'K': rev[j++] = 'M'; break;
                // case 'M': rev[j++] = 'K'; break;
                // case 'B': rev[j++] = 'V'; break;
                // case 'V': rev[j++] = 'B'; break;
                // case 'D': rev[j++] = 'H'; break;
                // case 'H': rev[j++] = 'D'; break;
                default: throw std::runtime_error("Sequence contains non ACGT characters");
            }
        }

        return rev;
    }

    //should be a template really
    bool valid_pam_right(std::deque<char> & seq, std::string pam) { return valid_pam(seq, pam, true); }
    bool valid_pam_left(std::deque<char> & seq, std::string pam) { return valid_pam(seq, pam, false); }

    //we store sequence in a deque, this will extract a pam sequence
    bool valid_pam(std::deque<char> & seq, std::string pam, bool pam_right) {
        int start = pam_right ? seq.size() - pam.size() : 0;

        //pull out the pam region from the deque
        for ( std::string::size_type i = 0; i < pam.size(); ++i ) {
            if ( seq[start+i] != pam[i] ) return false;
        }

        //we could in theory here allow an R or whatever

        //none of the bases mismatched, so its a valid pam
        return true;
    }
}

/*
    Copyright (C) 2014 Genome Research Limited

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.


    Written by Alex Hodgkins (ah19@sanger.ac.uk) in 2014
    Some code taken from scanham written by German Tischler
*/

