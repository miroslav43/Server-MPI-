# Compiler and Flags
CC = mpicc
CFLAGS = -Wall -O2 -I$(INC_DIR)
LDFLAGS =

# Directories
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin
OUT_DIR = output

# Ensure directories exist
$(shell mkdir -p $(OBJ_DIR))
$(shell mkdir -p $(BIN_DIR))

# Program name
PROGRAM = server_cluster

# Source files
MAIN_SRC = $(SRC_DIR)/main.c
WORKER_SRC = $(SRC_DIR)/worker.c
UTILS_SRC = $(SRC_DIR)/utils.c
COMANDS_SRC = $(SRC_DIR)/comands.c

# Header files
COMMON_HDR = $(INC_DIR)/common.h
UTILS_HDR = $(INC_DIR)/utils.h
COMANDS_HDR = $(INC_DIR)/comands.h

# Object files
MAIN_OBJ = $(OBJ_DIR)/main.o
WORKER_OBJ = $(OBJ_DIR)/worker.o
UTILS_OBJ = $(OBJ_DIR)/utils.o
COMANDS_OBJ = $(OBJ_DIR)/comands.o

# Default target
all: $(BIN_DIR)/$(PROGRAM)

# Compilation rules
$(MAIN_OBJ): $(MAIN_SRC) $(COMMON_HDR) $(UTILS_HDR) $(COMANDS_HDR)
	$(CC) $(CFLAGS) -c $< -o $@

$(WORKER_OBJ): $(WORKER_SRC) $(COMMON_HDR) $(UTILS_HDR) $(COMANDS_HDR)
	$(CC) $(CFLAGS) -c $< -o $@

$(UTILS_OBJ): $(UTILS_SRC) $(COMMON_HDR) $(UTILS_HDR)
	$(CC) $(CFLAGS) -c $< -o $@

$(COMANDS_OBJ): $(COMANDS_SRC) $(COMMON_HDR) $(COMANDS_HDR)
	$(CC) $(CFLAGS) -c $< -o $@

# Linking
$(BIN_DIR)/$(PROGRAM): $(MAIN_OBJ) $(WORKER_OBJ) $(UTILS_OBJ) $(COMANDS_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Clean up
clean:
	rm -f $(OBJ_DIR)/*.o
	rm -f $(BIN_DIR)/$(PROGRAM)\

oclean:
	rm -f $(OUT_DIR)/*result.txt

run:
	mpirun -np 4 $(BIN_DIR)/$(PROGRAM) comand_file.txt	
