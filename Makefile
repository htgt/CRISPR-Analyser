CC=g++
CXXFLAGS=-std=c++14 -O3 -W -Wall -pedantic
LIB=-pthread -ldl -Llib -lmongoose

debug: CXXFLAGS += -O0 -DDEBUG -g
debug: CCFLAGS += -O0 -DDEBUG -g
debug: server crispr_analyser 


SRCDIR:=src
BUILDDIR:=build
MONGOOSEDIR:=vpiotr-mongoose-cpp
SOURCES:=$(shell find $(SRCDIR) -type f -name *.cpp)
OBJECTS:=$(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.cpp=.o))
SERVER_OBJECTS:=$(BUILDDIR)/mongcpp.o $(filter-out build/crispr_analyser.o, $(OBJECTS))
FINDER_OBJECTS:=$(filter-out build/ots_server.o, $(OBJECTS))

#TODO: build OffTargetServer separate of ots_server

all: server crispr_analyser

server: mongoose $(SERVER_OBJECTS)
	@echo " Linking ots_server"
	mkdir -p bin
	$(CC) $(CXXFLAGS) -o bin/ots_server -Wl,-rpath='$$ORIGIN/../lib' $(SERVER_OBJECTS) $(LIB)

mongoose:
	@echo " Building mongoose"
	mkdir -p include
	mkdir -p lib
	$(MAKE) -C $(MONGOOSEDIR) linux && mv $(MONGOOSEDIR)/libmongoose.so lib && cp $(MONGOOSEDIR)/mongoose.h include/

crispr_analyser: $(FINDER_OBJECTS)
	@echo " Linking crispr_analyser"
	mkdir -p bin
	$(CC) $(CXXFLAGS) $^ -o bin/crispr_analyser

$(BUILDDIR)/mongcpp.o: $(MONGOOSEDIR)/mongcpp.cpp
	@echo " Building mongoose c++ bindings"
	@mkdir -p $(BUILDDIR)
	cd $(MONGOOSEDIR) && g++ -c -Wall -pthread -o ../$(BUILDDIR)/mongcpp.o mongcpp.cpp $(LIB) && cp mongcpp.h ../include/

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(BUILDDIR)
	$(CC) $(CXXFLAGS) -c -Iinclude -o $@ $<

clean:
	rm -rf bin/ots_server
	rm -rf bin/crispr_analyser
	rm -rf build
	rm -rf lib/*
	rm -rf include/*
	rm -rf $(MONGOOSEDIR)/mongoose
