VPATH = ../src

CPP = g++ -g
CFL =-std=c++17 -MMD -MP -O3
INC =-I../src $(shell pkg-config --cflags jsoncpp) -I/usr/local/include
LIB =-lmirsdrapi-rsp $(shell pkg-config --libs jsoncpp) -L/usr/local/lib -lpthread

BOOST = -lboost_iostreams -lboost_system -lboost_filesystem
SDROBJ = iqdata.o utility.o network.o server.o shell.o radio.o sdr.o jsocket.o logger.o

TARGETS = lsrsp fftplot fftcor phasediff sdr gfft histogram

all: $(TARGETS)

lsrsp: lsrsp.o
	@echo linking $@
	@$(CPP) -o $@ $^ $(LIB)

sdr: $(SDROBJ)
		@echo linking $@
		@$(CPP) -o $@ $^ $(LIB)

fftplot: fftplot.o iqdata.o
	@echo linking $@
	@$(CPP) -o $@ $^ -lfftw3 $(shell pkg-config --libs jsoncpp) -lm

fftcor: fftcor.o iqdata.o spectrum.o
	@echo linking $@
	@$(CPP) -o $@ $^ -lfftw3 $(shell pkg-config --libs jsoncpp) -lm

gfft:   gfft.o iqdata.o
	@echo linking $@
	@$(CPP) -o $@ gfft.o iqdata.o -lfftw3 $(BOOST) $(shell pkg-config --libs jsoncpp) -lm

phasediff: phasediff.o iqdata.o spectrum.o
	@echo linking $@
	@$(CPP) -o $@ $^ -lm -lfftw3 $(BOOST) $(shell pkg-config --libs jsoncpp)

histogram: histogram.o iqdata.o spectrum.o
		@echo linking $@
		@$(CPP) -o $@ $^ -lm -lfftw3 $(BOOST) $(shell pkg-config --libs jsoncpp)

clean:
	@rm -f $(TARGETS)
	@rm -f *.o *.d

%.o:%.cpp
	@echo compiling $<
	@$(CPP) -o $@ -c $(CFL) $(INC) $<

-include $(SRC:.cpp=.d)
