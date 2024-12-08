# Makefile for HW-4_code.c

# Compiler
CC = gcc

# Compiler Flags
CFLAGS = -Wall -pthread

# Target Executable
TARGET = HW-4_code

# Build Target
all: $(TARGET)

$(TARGET): HW-4_code.c
	$(CC) $(CFLAGS) -o $(TARGET) HW-4_code.c

# Clean Up
clean:
	rm -f $(TARGET) output.txt
