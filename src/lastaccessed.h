#ifndef __LASTACCESSED__
#define __LASTACCESSED__

#include <ctime>

class LastAccessed {
private:
    time_t t;

public:
    LastAccessed () { set(); }
    ~LastAccessed() {}

    //i think this is atomic because its a native type?
    void set() {
        time(&t);
    }

    time_t get() {
        return t;
    }

    //time since the last access
    double diff() {
        return difftime(time(NULL), t);
    }
};

#endif
