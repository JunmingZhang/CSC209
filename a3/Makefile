FLAGS = -Wall -g -std=gnu99
.PHONY: clean all

# default when running make
all: psort

#compile the executable psort, link psort.o and helper.o together
psort: psort.o helper.o 
	gcc ${FLAGS} -o $@ $^

mkwords: mkwords.o
	gcc ${FLAGS} -o mkwords mkwords.o -lm

%.o: %.c helper.h
	gcc ${FLAGS} -c $<

#clean psort and helper executables and objective codes after finishing
clean:
	rm -f *.o psort helper

clean_mkwords:
	rm -f mkwords.o mkwords
