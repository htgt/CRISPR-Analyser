CC=g++
CXXFLAGS=-std=c++0x -O3 -W -Wall -pedantic
LIB=-pthread -ldl -Llib -lmongoose

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
	$(CC) $(CXXFLAGS) -o bin/ots_server -Wl,-rpath='$$ORIGIN/../lib' $(SERVER_OBJECTS) $(LIB)

mongoose:
	@echo " Building mongoose"
	mkdir -p include
	$(MAKE) -C $(MONGOOSEDIR) linux && mv $(MONGOOSEDIR)/libmongoose.so lib && cp $(MONGOOSEDIR)/mongoose.h include/

crispr_analyser: $(FINDER_OBJECTS)
	@echo " Linking crispr_analyser"
	$(CC) $(CXXFLAGS) $^ -o bin/crispr_analyser

$(BUILDDIR)/mongcpp.o: $(MONGOOSEDIR)/mongcpp.cpp
	@echo " Building mongoose c++ bindings"
	@mkdir -p $(BUILDDIR)
	cd $(MONGOOSEDIR) && g++ -c -Wall -pthread -o ../$(BUILDDIR)/mongcpp.o mongcpp.cpp $(LIB) && cp mongcpp.h ../include/

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(BUILDDIR)
	$(CC) $(CXXFLAGS) -c -Iinclude -o $@ $<

clean:
	rm -rf bin/*
	rm -rf build
	rm -rf lib/*
	rm -rf include/*
	rm -rf $(MONGOOSEDIR)/mongoose
