/*
    The MIT License (MIT)

    Copyright (c) 2014 Genome Research Limited

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.

    Written by Alex Hodgkins (ah19@sanger.ac.uk)
    Incredibly basic config parser, intended to just be included inline
*/
#ifndef __CONFIG_PARSER__
#define __CONFIG_PARSER__

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>
#include <map>
#include <regex>

class ServerConfig {
    public:
        std::map<std::string,std::string> data;
        ServerConfig(const std::string & filename) {
            std::cerr << "Loading file: " << filename << std::endl;
            std::ifstream text ( filename );

            if ( text.is_open() ) {
                std::cerr << "Parsing config" << std::endl;
                std::string line;
                while ( std::getline(text, line) ) {
                    if ( ! line.length() ) continue;
                    if ( line[0] == '#' ) continue; //allow comments

                    parse_line( line );
                }

                if ( ! data.size() ) {
                    throw std::runtime_error("No valid configurations found in file");
                }
            }
            else {
                std::cerr << "Error opening file " << filename << std::endl;
                throw std::runtime_error("Failed to open input file");
            }
        }

        void parse_line(const std::string & line) {
            std::size_t pos = line.find('=');
            if ( pos == std::string::npos ) {
                std::cerr << "Error parsing line (no equals found): " << line << std::endl;
                std::cerr << "The format for the file is: option = value" << std::endl;
                throw std::runtime_error("Matching failed");
            }

            std::string name  = trim( line.substr(0, pos) );
            std::string value = trim( line.substr(pos + 1) );

            std::cout << "Species: " << name << std::endl;
            std::cout << "Filename: " << value << std::endl;

            //make sure we don't overwrite any values
            if ( data.count(name) > 0 )
                throw std::runtime_error("Duplicate config value specified!");

            data[name] = value;
        }

        std::string trim(const std::string & text, const std::string & whitespace = " \t\r\n" ) {
            std::size_t begin = text.find_first_not_of(whitespace);
            if ( begin == std::string::npos )
                return "";

            std::size_t end = text.find_last_not_of(whitespace);

            return text.substr( begin, (end - begin) + 1 );
        }
};

#endif
