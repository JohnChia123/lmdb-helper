CROSS_COMPILE=arm-linux-gnueabihf-

CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
LD=$(CROSS_COMPILE)gcc

LIBS=-llmdb -lsqlite3

TARGET = acqmgr_timing 

all: $(TARGET)

$(TARGET): acqmgr_timing.o lmdbhelper.o
	$(CXX) -g -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) -g -c -O0 -o $@ -Wall -pedantic $<

clean:
	rm -f *.o $(TARGET)

