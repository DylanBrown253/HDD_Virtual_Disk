# Makefile

CC = gcc
CFLAGS = -Wall -Wextra -g -Iheader
SRC_DIR = src
BIN_DIR = bin
HEADER_DIR = header

# Source files
SRC_FILES = $(SRC_DIR)/fs_Management_Functions.c $(SRC_DIR)/disk.c

# Executable names
EXECUTABLES = demo

all: $(EXECUTABLES)



demo: $(SRC_DIR)/demo.c $(SRC_FILES)
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/$@

clean:
	rm -f $(BIN_DIR)/*
