#ifndef __CRISPR_UTIL__
#define __CRISPR_UTIL__

#define MAX_CHAR_SIZE 30

const uint32_t VERSION = 3;

#include <cstdlib>
#include <string>
#include <sstream>
#include <vector>
#include <iostream>
#include <deque>

//struct that is stored in the binary index
struct metadata_t {
    uint64_t num_seqs;
    uint64_t seq_length;
    uint64_t offset; //so we can give real mouse db ids
    uint8_t species_id;
    char species[MAX_CHAR_SIZE]; //use fixed char arrays so we don't have to store size
    char assembly[MAX_CHAR_SIZE];
};

//crisprs we want to compare are inserted into this
struct crispr_t {
    uint64_t id;
    uint64_t seq;
    uint64_t rev_seq;
};

struct ots_data_t {
    uint64_t id;
    std::string off_targets;
    std::string off_target_summary;

    //override allowing you to magically have a text representation in a << into a stream
    friend std::ostream& operator << (std::ostream& os, const ots_data_t& ots) {
        os << "{\"id\":" << ots.id
           << ", \"off_targets\":" << ots.off_targets
           << ", \"off_target_summary\":\"" << ots.off_target_summary << "\"}";

        return os;
    }
};

class CrisprUtil {
    private:
        uint64_t * crisprs;
        metadata_t crispr_data;
        unsigned int max_offs;
        static const unsigned int max_mismatches = 4; //make this non static
        std::vector<ots_data_t> _find_off_targets(std::vector<crispr_t> queries, bool store_offs = false);
    public:
        CrisprUtil();
        ~CrisprUtil() { if ( crispr_data.num_seqs > 0 ) delete[] crisprs; }
        uint8_t cmap[256];
        bool smap[256];
        void _populate_cmap();
        void _populate_smap();
        void parse_genome(const std::string & filename, const std::string & outfile, int species_id, std::string pam);
        void load_binary(const std::string & outfile);
        std::string get_crispr(uint64_t id);
        uint64_t get_crispr_int(uint64_t id);
        uint64_t num_seqs();
        uint64_t seq_length();
        std::string species();
        std::vector<ots_data_t> find_off_targets(std::vector<uint64_t> ids, bool store_offs = false);
        std::vector<ots_data_t> find_off_targets(uint64_t start, uint64_t amount, bool store_offs = false);
        void text_to_binary(const std::vector<std::string> & infiles, const std::string & outfile, metadata_t * data);
        void load_metadata(std::ifstream & text);
        void search_by_seq(std::string seq, short pam_right, std::vector<uint64_t> & output);
        std::string off_targets_by_seq(std::string seq, bool pam_right);
        void print_crispr_row(std::ofstream & out, std::deque<char> & crispr, std::string & seqname, int64_t start, bool pam_right, int species_id);
};

#endif
