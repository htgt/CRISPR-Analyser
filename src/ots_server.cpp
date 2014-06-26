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
    Uses mongoose c library and vpiotr-mongoose c++ bindings

    To compile manually:
    download vpiotr-mongoose
    run make to generate libmongoose.so

    Build mongocpp.o with:
    g++ -c -W -Wall -pthread mongcpp.cpp

    Make sure its working by compiling mongotest:
    g++ -W -Wall -pthread -ldl -o mongotest mongcpp.o ots_server.cpp -L. -lmongoose
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <signal.h>
#include <stdexcept>
#include <unistd.h>

#include "mongcpp.h"
#include "crisprutil.h"
#include "utils.h"
#include "lastaccessed.h"

using namespace mongoose;
using namespace std;

int done = 0;

void sig_handler(int sig) {
    cerr << "Received signal " << util::to_string(sig) << ", exiting" << endl;
    done = 1;
}

class OffTargetServer: public MongooseServer {
public:
    LastAccessed last_accessed;
    OffTargetServer(): MongooseServer() {
        last_accessed = LastAccessed();
    }

    virtual ~OffTargetServer() {
        //delete all the crispr util objects
        for ( map<string,CrisprUtil*>::iterator i = crispr_util.begin(); i != crispr_util.end(); ++i ) {
            delete i->second;
        }

        crispr_util.clear();
    }

    void load_index(string species, string& index) {
        util::lc( species ); //make lowercase
        cerr << "Loading " << species << " index" << endl;

        //see if species has already been loaded
        if ( species_exists(species) ) {
            throw runtime_error(species + " index has already been loaded!");
        }

        //get a pointer to a new crispr finder,
        //we have to use pointers or the map will create a copy each time,
        //and as soon as one goes out of scope the entire off targets array is deleted
        crispr_util[species] = new CrisprUtil();
        crispr_util[species]->load_binary( index );

        string loaded_species = crispr_util[species]->species();
        util::lc(loaded_species);

        if ( loaded_species != species )
            throw runtime_error("'" + loaded_species + "' does not match user specified species " + species);

        cerr << species << " index loaded" << endl;
    }

    bool species_exists(string species) {
        return crispr_util.count(species) > 0;
    }

    CrisprUtil* get_util(string species) {
        if ( ! species_exists(species) ) {
            throw runtime_error(species + " does not exist!");
        }

        return crispr_util[species];
    }

    void remove_util(string species) {
        if ( crispr_util.count(species) == 0 ) {
            throw runtime_error(species + " does not exist!");
        }

        delete crispr_util[species]; //delete object
        crispr_util.erase(species); //delete key
    }

    void log(string log_file, string& text) {
        ofstream log( log_file, ios_base::out | ios_base::app );
        log << text << endl;
    }
protected:
    map<string, CrisprUtil*> crispr_util;

    virtual bool handleEvent(ServerHandlingEvent eventCode, MongooseConnection& connection, const MongooseRequest& request, MongooseResponse& response) {
        bool res = false; //gets set to true if successful
        string result, content_type;

        if (eventCode == MG_NEW_REQUEST) {
            string uri = request.getUri();
            //returns 0 if they match
            if ( ! uri.compare(0, 4, "/api") ) {
                cerr << "API URI: " << uri << endl;
                //prefix is api so we know it will be json
                content_type = "application/json";

                try {
                    if ( uri == string("/api/search") ) {
                        result = search_json(request);
                        res = true;
                    }
                    else if ( uri == string("/api/off_targets") ) {
                        result = find_off_targets(request);
                        res = true;
                    }
                    else {
                        cerr << "Couldn't find api URI " << uri << endl;
                    }
                }
                catch (const runtime_error& error) {
                    string e = error.what();
                    result = json_error( e );
                    res = true; //we need this or we return 404
                }

                if ( res ) handle_jsonp(request, result);
            }
            else {
                cerr << "URI: " << uri << endl;
                content_type = "text/html";

                try {
                    if ( uri == string("/search") ) {
                        result = search(request);
                    }
                    else {
                        cerr << "Couldn't find URI " << uri << endl;
                    }
                }
                catch (const runtime_error& error) {
                    result = "<h1>" + string(error.what()) + "</h1>";
                }
            }
        }

        if ( res ) {
            process_response(request, response, content_type, result);
            last_accessed.set(); //update last accessed to now
        }

        return res;
    }

    void process_response(const MongooseRequest& request, MongooseResponse& response, string content_type, string result) {
        response.setStatus(200);
        response.setConnectionAlive(false);
        response.setCacheDisabled();
        response.setContentType(content_type);
        response.addContent(result);
        response.write();
    }

    void get_matches(const MongooseRequest& request, vector<uint64_t>& matches) {
        string seq;
        string pam_right_str;
        string species;

        request.getVar("seq", seq);
        request.getVar("pam_right", pam_right_str);
        request.getVar("species", species);

        //make species lowercase
        util::lc( species );

        //default pam_right is 2, which means don't consider the pam
        if ( pam_right_str.size() == 0 ) pam_right_str = "2";

        if ( seq.empty() )
            throw runtime_error("Please provide a sequence");

        if ( seq.size() != 20 )
            throw runtime_error("Sequence must be 20 bases long");

        if ( pam_right_str.empty() )
            throw runtime_error("Please specify pam right");

        int pam_right = atoi( pam_right_str.c_str() );

        if ( ! ( pam_right == 0 || pam_right == 1 || pam_right == 2 ) )
            throw runtime_error("pam_right must be 0, 1 or 2");

        if ( species.empty() )
            throw runtime_error("Please specify a species");

        cerr << "Searching for " << seq << ", pam_right is " << pam_right << endl;

        get_util(species)->search_by_seq( seq, pam_right, matches );
    }

    const string search(const MongooseRequest& request) {
        string result;
        result = "<h1>Sample Info Page</h1>";
        result += "<br />Request URI: " + request.getUri();

        vector<uint64_t> matches;
        get_matches(request, matches);

        //result += "<br />Seq is: " + seq;
        result += "<br />Found " + util::to_string(matches.size()) + " matches:";

        for ( uint i = 0; i < matches.size(); i++ ) {
            result += "<br />" + util::to_string(matches[i]);
        }


        return result;
    }

    const string search_json(const MongooseRequest& request) {
        vector<uint64_t> matches;
        get_matches(request, matches);

        return util::to_json_array(matches);
    }

    const string find_off_targets(const MongooseRequest& request) {
        string species;
        string result = "{";

        vector<uint64_t> ids_all;
        string ids_text;
        request.getVar("ids", ids_text);
        request.getVar("species", species);

        if ( ids_text.empty() )
            throw runtime_error("Please provide ids");

        if ( species.empty() )
            throw runtime_error("Please provide a species");

        util::lc(species);

        vector<string> ids = util::split(ids_text);

        for ( uint64_t i = 0; i < ids.size(); i++ ) {
            ids_all.push_back( stoull(ids[i]) );
        }

        vector<ots_data_t> offs = get_util(species)->find_off_targets(ids_all, true);

        for ( uint i = 0; i < offs.size(); ++i ) {
            if ( i > 0 ) result += ",";
            result += "\"" + util::to_string(offs[i].id) + "\":" + util::to_string(offs[i]);
        }

        result += "}";

        //result = "Got data for " + util::to_string(offs.size()) + " crisprs";

        return result;
    }

    //wrap a json string in jsonp. return true if it was jsonp
    bool handle_jsonp(const MongooseRequest& request, string& result) {
        string callback;
        request.getVar("callback", callback);

        if ( callback.empty() ) return false;

        callback += "(";

        result.insert(0, callback);
        result += ");";

        return true;
    }

    string json_error(string& s) {
        return "{\"error\":\"" + s + "\"}";
    }
};

int usage() {
    fprintf(stderr, "\n");
    fprintf(stderr, "Program: ots_server\n");
    fprintf(stderr, "Contact: Alex Hodgkins <ah19@sanger.ac.uk>\n");
    fprintf(stderr, "Description: A server version of the crispr_analyser that responds to JSON requests\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: ots_server [options]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options: -t INT     The number of threads the server should use. Default is 5\n");
    fprintf(stderr, "         -p INT     The port to run the server on\n");
    fprintf(stderr, "         -l FILE    The file containing the human CRISPR index\n");
    fprintf(stderr, "         -m FILE    The file containing the mouse CRISPR index\n");
    fprintf(stderr, "         -s         Load the default mouse/crispr indexes (sanger only)\n");
    fprintf(stderr, "         -d         Daemon mode\n");
    fprintf(stderr, "\n");

    return 1;
}

int main(int argc, char * argv[]) {
    string num_threads = "5"; //this has to be a string for some reason
    string port        = "8080";

    bool default_opts = false;
    bool daemon = false;
    string human_index, mouse_index;
    int c = -1;
    while ( (c = getopt(argc, argv, "t:p:l:m:sd")) != -1 ) {
        switch ( c ) {
            case 't': num_threads  = optarg; break;
            case 'p': port         = optarg; break;
            case 'l': human_index  = optarg; break;
            case 'm': mouse_index  = optarg; break;
            case 's': default_opts = true; break;
            case 'd': daemon       = true; break;
            case '?': return usage();
        }
    }

    if ( default_opts ) {
        human_index = "/lustre/scratch109/sanger/team87/crispr_indexes/GRCh37_index.bin";
        mouse_index = "/lustre/scratch109/sanger/team87/crispr_indexes/GRCm38_index.bin";
    }
    else { 
        if ( human_index.empty() && mouse_index.empty() ) {
            cerr << "You must specify at least 1 index to load, or use -s to use the default indexes in /lustre" << endl;
            return usage();
        }
    }

    if ( daemon ) {
        cerr << "Launching daemon" << endl;
        pid_t pid, sid;

        pid = fork();
        if (pid < 0) exit(EXIT_FAILURE); //make sure the fork was successful

        if (pid > 0) exit(EXIT_SUCCESS); //close parent

        //Change File Mask
        umask(0);

        //Create a new Signature Id for our child
        sid = setsid();
        if (sid < 0) { exit(EXIT_FAILURE); }

        //Change Directory
        //If we cant find the directory we exit with failure.
        if ((chdir("/")) < 0) { exit(EXIT_FAILURE); }

        //Close Standard File Descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    OffTargetServer server;

    if ( ! human_index.empty() ) server.load_index("human", human_index);
    if ( ! mouse_index.empty() ) server.load_index("mouse", mouse_index);

    server.setOption("document_root", "html");
    server.setOption("listening_ports", port);
    server.setOption("num_threads", num_threads);

    cerr << "Starting server with " << num_threads << " threads on port " << port << endl;

    server.start();

    //set done var if we get term or int for graceful shutdown
    signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);

    cout << "Server started, press Ctrl-C to quit" << endl;
    while ( ! done ) {
       sleep(2);

       //for some reason not doing anything for a while seems to slow down the memory access
       //this should keep it active
       if ( server.last_accessed.diff() > 600 ) {
            cout << "10 minutes since last access, running search" << endl;
            server.last_accessed.set();

            //if the server sits idle for too long the next search after the inactivity
            //will be incredibly slow; this will prevent that by keeping it alive
            vector<uint64_t> matches;
            try {
                CrisprUtil* util = server.get_util("human");
                util->search_by_seq( "GTGTCAGTGAAACTTACTCT", 0, matches );
            }
            catch (const runtime_error& error) {
                cout << "Error in search by seq test:\n";
                cout << error.what() << endl;
            }
       }
    }

    server.stop();
    cout << "Server stopped" << endl;
}
