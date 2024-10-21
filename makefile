# Compiler
CXX = g++
CXXFLAGS = -Wall 

all: server client

server: server.cpp
    $(CXX) -o tsamgroup43 server.cpp

client: client.cpp
    $(CXX) -o client client.cpp

clean:
    rm -f tsamgroup43 client
