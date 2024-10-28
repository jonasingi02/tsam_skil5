# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -std=c++11

# Detect if the architecture is arm64 and set the correct flags
ARCH := $(shell uname -m)
ifneq ($(ARCH),arm64)
    ARCHFLAGS = 
else
    ARCHFLAGS = -arch arm64
endif

all: server client

server: server.cpp
	$(CXX) $(CXXFLAGS) $(ARCHFLAGS) -o tsamgroup43 server.cpp

client: client.cpp
	$(CXX) $(CXXFLAGS) $(ARCHFLAGS) -o client client.cpp

clean:
	rm -f tsamgroup43 client