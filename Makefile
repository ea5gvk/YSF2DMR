CC      = gcc
CXX     = g++
CFLAGS  = -g -O3 -Wall -std=c++0x -pthread
LIBS    = -lm -lpthread
LDFLAGS = -g

OBJECTS = 	BPTC19696.o Conf.o CRC.o Golay24128.o Hamming.o Log.o ModeConv.o YSFNetwork.o \
			StopWatch.o Sync.o Thread.o Timer.o DMREMB.o DMREmbeddedData.o DMRFullLC.o \
			DMRLC.o DMRSlotType.o QR1676.o RS129.o Golay2087.o JitterBuffer.o UDPSocket.o Utils.o \
			DMRData.o SHA256.o DMRNetwork.o YSFConvolution.o YSFFICH.o YSF2DMR.o YSFPayload.o

all:		YSF2DMR

YSF2DMR:	$(OBJECTS)
		$(CXX) $(OBJECTS) $(CFLAGS) $(LIBS) -o YSF2DMR

%.o: %.cpp
		$(CXX) $(CFLAGS) -c -o $@ $<

clean:
		$(RM) YSF2DMR *.o *.d *.bak *~
 