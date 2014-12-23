#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <array>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <ctime>
#include <climits>
#include <map>
#include <deque>

#include "utils.h"
#include "crisprutil.h"

using namespace std;

CrisprUtil::CrisprUtil() {
    //initialise variables
    max_offs = 2000;

    //so we can see if data has been loaded
    crispr_data.num_seqs = 0;

    //populate the array we use to map char -> int
    _populate_cmap();
    _populate_smap();

    //cerr << bits_to_string( crisprs[263164224], 20 ) << "\n";
}

void CrisprUtil::_populate_cmap() {
    //create an array for all possible char values, with 4 as the default value
    std::fill( &cmap[0], &cmap[sizeof(cmap)/sizeof(cmap[0])], 4 );

    //fill in the 8 entries we're actually interested in with their two bit values
    cmap['a'] = cmap['A'] = 0; //00
    cmap['c'] = cmap['C'] = 1; //01
    cmap['g'] = cmap['G'] = 2; //10
    cmap['t'] = cmap['T'] = 3; //11
}

void CrisprUtil::_populate_smap() {
    // fill space table
    std::fill(&smap[0], &smap[sizeof(cmap)/sizeof(smap[0])], 0);
    for ( unsigned int i = 0; i < 256; ++i )
        if ( isspace(i) )
            smap[i] = 1;
}

//return a crispr sequence string
string CrisprUtil::get_crispr(uint64_t id) {
    //make sure load_binary has been called
    if ( crispr_data.num_seqs == 0 )
        throw runtime_error( "CRISPRs must be loaded before calling get_crispr" );

    //the id we get needs to be localised to our 0 based array - spotted by Liang Cai
    id -= crispr_data.offset;

    if ( id >= crispr_data.num_seqs )
        throw runtime_error( "Index out of range" );

    return util::bits_to_string( get_crispr_int(id), crispr_data.seq_length );
}

//returns our 2bit representation of a crispr, mostly this will be given straight
//to the bits_to_string method
uint64_t CrisprUtil::get_crispr_int(uint64_t id) {
    //make sure load_binary has been called
    if ( crispr_data.num_seqs == 0 )
        throw runtime_error( "CRISPRs must be loaded before calling get_crispr" );

    if ( id >= crispr_data.num_seqs )
        throw runtime_error( "Index out of range" );

    return crisprs[id];
}

uint64_t CrisprUtil::num_seqs() {
    return crispr_data.num_seqs;
}

uint64_t CrisprUtil::seq_length() {
    return crispr_data.seq_length;
}

string CrisprUtil::species() {
    return string(crispr_data.species);
}

//load and verify metadata from ifstream (will adjust ifstream position)
void CrisprUtil::load_metadata(ifstream & text) {
    if ( text.is_open() ) {
        //make sure that the binary file has the same endianness as our hardware
        uint8_t endian_test;
        text.read( (char *) &endian_test, sizeof(uint8_t) );

        if ( endian_test != 1 ) {
            throw runtime_error("Endianness of the file does not match your hardware!");
        }

        //make sure the versions matched or things will go very badly.
        uint32_t file_version;
        text.read( (char *) &file_version, sizeof(uint32_t) );

        if ( file_version != VERSION )
            throw runtime_error("File is the wrong version! Please regenerate.");

        cerr << "Version is " << VERSION << "\n";

        //load the metadata
        text.read( (char *) &crispr_data, sizeof(crispr_data) );

        cerr << "Assembly is " << crispr_data.assembly
             << " (" << crispr_data.species << ")" << "\n"
             << "File has " << crispr_data.num_seqs << " sequences" << "\n"
             << "Sequence length is " << crispr_data.seq_length << "\n"
             << "Offset is " << crispr_data.offset << "\n"
             << "Species id is " << int(crispr_data.species_id) << endl;

        uint64_t memory_required = crispr_data.num_seqs * sizeof(uint64_t);
        cerr << "Will require " << (memory_required/1024)/1024 << "MB of memory" << endl;

        //dont use more than 3gb of memory, safety thing for now.
        if ( memory_required > 3221225472 ) {
            throw runtime_error("More than 3gb of memory required, aborting.");
        }
    }
    else {
        cerr << "ifstream passed to load_metadata is not open" << endl;
        throw runtime_error("Failed to load metadata");
    }
}


//could have the option to not care about the pam site by doing an & with pam_off
void CrisprUtil::search_by_seq(string seq, short pam_right, vector<uint64_t> & output) {
    crispr_t query;
    query.id = 0;
    //we only want to pass 1 or 0 to pam_right, anything else will break everything.
    //should probably change string_to_bits to only allow 1 or 0
    query.seq = util::string_to_bits( cmap, seq, pam_right ? 1 : 0 );
    query.rev_seq = util::revcom( query.seq, crispr_data.seq_length );

    //if they put in a load of Zs or something then just return now don't bother looping
    if ( query.seq == ERROR_STR )
        throw runtime_error("Query sequence contains invalid characters");

    //default is to not modify the pam (bitwise or with 0 doesnt change anything)
    //but if pam_right is two we want to allow both, so we change it to a 1
    uint64_t force_pam = 0;
    if ( pam_right == 2 ) {
        force_pam = 0x1ull << crispr_data.seq_length*2;
        query.seq |= force_pam;
        query.rev_seq |= force_pam;
    }

    if ( seq.size() != crispr_data.seq_length )
        throw runtime_error("Sequence sizes must be equal");

    //TODO:
    //make sure binary has been loaded

    clock_t t = clock();

    uint64_t total_error = 0;
    uint64_t current_crispr;

    //scan through the file and check every sequence against ours
    for ( uint64_t i = 1; i <= crispr_data.num_seqs; i++ ) {
        // if ( i % 50000000 == 0 ) {
        //     cerr << "Processed " << i << " sequences" << endl;
        // }

        if ( crisprs[i] != ERROR_STR ) {
            //if force pam is set it will ALWAYS be a 1
            current_crispr = (crisprs[i] | force_pam);
            if ( query.seq == current_crispr || query.rev_seq == current_crispr ) {
                //fprintf( stderr, "Found match at idx: %lu\n", (i+crispr_data.offset) );
                output.push_back( i+crispr_data.offset );
            }

        }
        else {
            total_error++;
            continue;
        }
    }

    t = clock() - t;

    cerr << "Found " << output.size() << " exact matches" << endl;
    //cerr << "Checked against " << crispr_data.num_seqs << " sequences" << endl;
    //cerr << "Skipped " << total_error << " sequences" << endl;

    fprintf( stderr, "Scanning took %f seconds\n", ((float)t)/CLOCKS_PER_SEC );
}

//do speed comparison with crispr string
void CrisprUtil::print_crispr_row(ofstream & out, deque<char> & crispr, string & seqname, int64_t start, bool pam_right, int species_id) {
    //format is X,34759384,GTCATGCAATCGATCGATCGCGG,1,1
    out << seqname << "," << start << ","; //chr
    //display seq
    for ( auto it=crispr.begin(); it != crispr.end(); ++it ) {
        out << *it;
    }

    out << "," << pam_right << "," << species_id << '\n';
}

void CrisprUtil::parse_genome(const string & filename, const string & outfile, int species_id, string pam) {
    cerr << "Loading genome from:\n" << filename << "\n";
    ifstream text( filename );
    ofstream out ( outfile );

    if ( ! out.is_open() ) {
        cerr << "Error opening output file: " << outfile << endl;
        throw runtime_error("Failed to open output file");
    }
    if ( ! text.is_open() ) {
        cerr << "Error opening input file: " << filename << endl;
        throw runtime_error("Failed to open input file");
    }

    clock_t t = clock();

    string rev_pam = util::revcom( pam );
    deque<char> current (23, 'N');
    string seqname, line;
    int64_t seqpos = 0;
    int64_t patlen = 23; //should allow this to be configurable?
    uint64_t total = 0;

    //will need to process the pam here and make it a bit version

    while ( getline(text, line) ) {
        if ( ! line.size() ) continue;

        //if its a header line reset everything then go to the next line
        if ( line[0] == '>' ) {
            //take any text up to the first space (or the end if there's no space)
            //we go from 1 instead of 0 as we don't want the > character
            string::size_type first = line.find(" ");
            if ( first != string::npos )
                seqname = line.substr(1, first-1); //-1 as we start at 1 not 0
            else
                seqname = line.substr(1, line.size()-1);

            //if it has chr in it then strip it as we don't want those redundant
            //characters 300m times
            if ( seqname.find("Chr") == 0 || seqname.find("chr") == 0 )
                seqname = seqname.substr(3, seqname.size()-1);

            //reset seqpos to start of chromosome
            seqpos = 0;

            cerr << "Processing chromosome " << seqname << endl;
            continue;
        }

        //iterate through every sequence character on the line
        for ( string::size_type i = 0; i < line.size(); ++i ) {
            uint8_t const c = line[i];

            //skip if its a whitespace character
            if ( smap[c] )
                continue;

            //push off whatever base is at the start, and append our new base at the end
            //can't be bothered storing it in an int as we want more than just
            //the few characters in the cmap, this isn't that efficient but whatever
            current.pop_front();
            current.push_back(c);

            //if we have a full pattern length worth of text we can start
            //checking for crisprs
            if ( ++seqpos >= patlen ) {
                int64_t seq_start = seqpos - patlen;

                //its possible a crispr is both a valid pam left and a valid pam right
                //crispr, in which case we will print it twice.
                //we add +1 to the seq_start so its on ensembl coords rather than ucsc
                if ( util::valid_pam_left( current, rev_pam ) ) {
                    total++;
                    print_crispr_row(out, current, seqname, seq_start+1, false, species_id);
                }

                if ( util::valid_pam_right( current, pam ) ) {
                    total++;
                    print_crispr_row(out, current, seqname, seq_start+1, true, species_id);
                }
            }

        }
    }

    out.close();
    text.close();


    t = clock() - t;
    fprintf( stderr, "Gathering CRISPRs took %f seconds\n", ((float)t)/CLOCKS_PER_SEC );
}

void CrisprUtil::load_binary(const string & filename) {
    cerr << "Loading binary data from:\n" << filename << "\n";
    ifstream text ( filename );

    if ( text.is_open() ) {
        load_metadata( text );

        clock_t t = clock();

        /*
            should consider creating a struct with bitfields around the uint64_t,
            as we can then name sections and stop passing the lengths around.
            so, something like:
            typdef struct {
                uint64_t seq:40, pam_right:1, blank:23;
            } crispr_seq_t;

            might be slower, however.
        */

        //allocate heap memory as its too big for stack
        //store pointer in the class variable
        crisprs = new uint64_t [crispr_data.num_seqs+1];
        crisprs[0] = 0; //we don't use 0 so that all ids match the db ids

        //could load in batches of 50m, but on htgt this now takes 2 seconds~ so why bother

        // read all data into our array in one go (6 seconds vs 16 if we do one at a time)
        text.read( reinterpret_cast<char *>( &crisprs[1] ), sizeof(uint64_t)*crispr_data.num_seqs );

        //old way loading one integer at a time
        // uint64_t total_error = 0;
        // for ( uint64_t i = 1; i <= crispr_data.num_seqs; i++ ) {
        //     if ( i % 50000000 == 0 ) {
        //         cerr << "Loaded " << i << " sequences" << endl;
        //     }
        //     //load each 'integer'
        //     text.read( (char *) &crisprs[i], sizeof(uint64_t) );

        //     if ( crisprs[i] == ERROR_STR ) total_error++;
        // }

        t = clock() - t;

        cerr << "Loaded " << crispr_data.num_seqs << " sequences" << endl;
        //cerr << "Skipped " << total_error << " sequences" << endl;

        text.close();

        fprintf( stderr, "Loading took %f seconds\n", ((float)t)/CLOCKS_PER_SEC );
    }
    else {
        cerr << "Error opening file" << filename << endl;
        throw runtime_error("Failed to open input file");
    }
}

//view this with:
//xxd -c 8 -b -l 10000 /lustre/scratch110/sanger/ah19/crisprs.bin | less
void CrisprUtil::text_to_binary(const vector<string> & infiles, const string & outfile, metadata_t * data) {
    ofstream out ( outfile, ios::binary );

    //write out a 1 as the first 8 bits. we will read this back in, and make sure its a 1
    uint8_t endian_test = 1;
    out.write( (char *) &endian_test, sizeof(endian_test) );
    out.write( (char *) &VERSION, sizeof(VERSION) );

    size_t offset = sizeof(endian_test) + sizeof(VERSION);

    cerr << "Writing metadata\n";

    //leave space at the beginning for the number of sequences and the sequence length
    out.seekp( offset + sizeof(*data), ios::beg );

    uint64_t total_skipped = 0;

    cerr << "Version is " << VERSION << "\n";

    for ( vector<string>::size_type i = 0; i < infiles.size(); i++ ) {
        cerr << "Processing " << infiles[i] << "\n";
        ifstream text ( infiles[i] );

        /*
            NOTE: apparently C style FILE* access with fread is quicker
        */

        if ( text.is_open() && out.is_open() ) {
            string line;

            while ( getline( text, line ) ) {
                if ( line.size() ) {
                    //split the line up so we can get the parts we want
                    vector<string> spl = util::split(line);

                    //treated as a boolean
                    short pam_right = stoi( spl[3] );
                    if ( ! (pam_right == 0 || pam_right == 1) )
                        throw runtime_error( "pam_right field must be 1 or 0" );

                    //if its pam right remove last 3 chars, if its pam left remove first 3
                    string seq = pam_right ? spl[2].substr(0, 20) : spl[2].substr(3);

                    if ( seq.length() != data->seq_length )
                        throw runtime_error( "Different seq lengths in file!" );

                    //get a binary representation of the string and write it to the file
                    uint64_t bits = util::string_to_bits( cmap, seq, pam_right );
                    out.write( (char *) &bits, sizeof(bits) );

                    //we use a string of all 1s as an error string
                    if ( bits == ERROR_STR ) {
                        total_skipped++;
                        //cerr << "Skipped:" << data->num_seqs << "\n";
                    }

                    //keep track of how many we have so we can write it to the file
                    if ( ++data->num_seqs % 50000000 == 0 ) {
                        cerr << "Converted " << data->num_seqs << " sequences\n";
                    }
                }
            }
            text.close();
        }
        else {
            cerr << "Error opening files: " << infiles[i] << ", " << outfile << "\n";
            throw runtime_error("Failed to open files");
        }
    }

    //write the metadata after the endianness and version numbers
    out.seekp( offset, ios::beg );
    out.write( (char *) data, sizeof(*data) );

    out.close();

    cerr << "Sequence length is " << data->seq_length << endl;
    cerr << "Converted " << data->num_seqs << " sequences" << endl;
    cerr << "Skipped " << total_skipped << " sequences" << endl;
}

vector<ots_data_t> CrisprUtil::find_off_targets(vector<uint64_t> ids, bool store_offs) {
    vector<crispr_t> queries;

    for ( auto i = ids.begin(); i < ids.end(); i++ ) {
        crispr_t crispr;
        crispr.id = *i - crispr_data.offset; //subtract offset to get the right number
        if ( crispr.id > crispr_data.num_seqs ) {
            cerr << crispr.id << " is an invalid id. Wrong species?" << "\n";
            throw runtime_error("Index out of bounds");
        }

        crispr.seq = crisprs[crispr.id];
        crispr.rev_seq = util::revcom(crisprs[crispr.id], crispr_data.seq_length);

        queries.push_back( crispr );
    }

   return  _find_off_targets( queries, store_offs );
}

std::string CrisprUtil::off_targets_by_seq(std::string seq, bool pam_right) {

/*
 * 1) create a crispr_t structure for the sequence and revcom of the query string
 * by converting the query sequence to integer form (using method in utils.cpp)
 * and revcomping that integer.
 * 2) pass that into _find_off_targets, which returns a vector of off-target site (ots) data structures
 * 3) pull out the off_target_summary string from the ots_data_t
 * 4) add that to result string and return from this method
 * 5) check that the query crispr sequence length is 20 bases 
 *
 *
 */
    crispr_t query_crispr;
    vector<crispr_t> crispr_query_list;
    vector<ots_data_t> vodt;
    ots_data_t first_ot;
    std::string ret_string = "None";
/* 
 * cmap is a public attribute of CrisprUtil that gets instantiated with a lookup table of letters and numberic codes
 */
    query_crispr.seq = util::string_to_bits(cmap, seq, pam_right);
    query_crispr.rev_seq = util::revcom( query_crispr.seq, crispr_data.seq_length );
    query_crispr.id = 0;
    crispr_query_list.push_back(query_crispr);

    vodt = _find_off_targets( crispr_query_list, 1 );
    
    cout << vodt.size() << " units" << "\n";
    if ( !vodt.empty() ) {
       first_ot = vodt.front(); 
       ret_string = first_ot.off_target_summary;
    }

    return ret_string;
}

vector<ots_data_t> CrisprUtil::find_off_targets(uint64_t start, uint64_t amount, bool store_offs) {
    vector<crispr_t> queries;

    //create a crispr_t for every requested id
    for ( uint64_t i = 0; i < amount; i++ ) {
        crispr_t crispr;
        crispr.id = (start-crispr_data.offset)+i;
        crispr.seq = crisprs[crispr.id];
        crispr.rev_seq = util::revcom( crisprs[crispr.id], crispr_data.seq_length );

        queries.push_back( crispr );
    }

    return _find_off_targets( queries, store_offs );
}

vector<ots_data_t> CrisprUtil::_find_off_targets(vector<crispr_t> queries, bool store_offs) {
    unsigned int mm;

    //this can be used to check if the pam_right bit is set.
    //the complement of it can be used to turn off the flag.
    const uint64_t pam_on = 0x1ull << crispr_data.seq_length*2;
    const uint64_t pam_off = ~pam_on;

    array<uint64_t, max_mismatches+1> summary;
    vector<ots_data_t> results;

    cerr << "Searching for off targets\n";

    clock_t t = clock();
    //calculate each query individually
    for ( vector<crispr_t>::size_type i = 0; i < queries.size(); i++ ) {
        //make vector with 5000 slots reserved
        //should we do this outside the loop like summary?
        vector<uint64_t> off_targets;
        off_targets.reserve( max_offs );

        //new crispr so reset the summary to all 0
        summary.fill(0);

        uint64_t total_matches = 0;

        cerr << "Finding off targets for " << queries[i].id+crispr_data.offset << endl;

        //iterate over every crispr checking for of targets
        for ( uint64_t j = 1; j <= crispr_data.num_seqs; j++ ) {
            //skip any sequences that are all A (i.e 0)
            if ( crisprs[j] == ERROR_STR ) continue;
            //xor the two bit strings
            //try forward first
            uint64_t match = queries[i].seq ^ crisprs[j];


            //we only care about the orientation that matches the orientation
            //of the sequence. the other can safely be ignored.
            //one of these will always be true as its a single bit

            //if this is true its because the pams did NOT match.
            //when they don't match we need to use the reverse complemented sequence,
            //which will match.
            if ( match & pam_on ) {
                //orientations didnt match so we need to try the reverse
                uint64_t match_r = queries[i].rev_seq ^ crisprs[j];
                mm = util::pop_count( match_r & pam_off );
            }
            else {
                //the PAMs matched so count the mismatches
                mm = util::pop_count( match & pam_off );
            }

            if ( mm <= max_mismatches ) {
                summary[mm]++;

                if ( ++total_matches < max_offs ) {
                    off_targets.push_back( j + crispr_data.offset );
                }
            }
        }

        cerr << "Found " << total_matches << " off targets.\n";

        //bulk mode won't use store offs as quickest is just to dump to stdout
        if ( store_offs ) {
            ots_data_t ots_data;
            ots_data.id = queries[i].id+crispr_data.offset;

            //add the json array of off target ids or empty array
            if ( total_matches < max_offs ) {
                ots_data.off_targets = util::to_json_array(off_targets);
            }
            else {
                ots_data.off_targets = "[]";
            }

            ots_data.off_target_summary = util::format_off_targets(summary);
            results.push_back(ots_data);
            continue;
        }

        //output is:
        //crispr_id,species_id,off_targets,off_target_summary
        cout << (queries[i].id+crispr_data.offset) << "\t" << int(crispr_data.species_id) << "\t";

        //print off targets
        if ( total_matches < max_offs ) {
            cout << util::to_postgres_array(off_targets) << "\t";
        }
        else {
            cout << "\\N\t"; //standard NULL is a \N
        }

        cout << util::format_off_targets(summary) << "\n";
    }

    t = clock() - t;

    fprintf( stderr, "Took %f seconds\n", ((float)t)/CLOCKS_PER_SEC );

    return results;
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
