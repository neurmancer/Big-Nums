CC      = gcc
CFLAGS  = -Wall -Wextra -O2
LDFLAGS = -lm

# Library sources (everything except the main driver)
LIB_SRCS = bigNumLibThingy.c smartFFTThingy.c
LIB_OBJS = $(LIB_SRCS:.c=.o)

# Main file and output binary – change these from the command line
MAIN   ?= main.c
TARGET ?= test_big

# Derive the main object file name from MAIN
MAIN_OBJ = $(MAIN:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(MAIN_OBJ) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c bignums.h complexFFT.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(LIB_OBJS) $(MAIN_OBJ) $(TARGET) *.o