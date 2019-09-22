CXX := g++
CXXFLAGS := -std=c++11
LDFLAGS :=-L. -lcurlpp -l:libcurlpp.a -lcurl -lpthread
INCLUDE := -I./include
TARGET := pbCPP
SRC := main.cpp

all:
	$(CXX) -o $(TARGET) $(SRC) $(CXXFLAGS) $(INCLUDE) $(LDFLAGS)