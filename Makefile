.PHONY:all clean directories

MKDIR_P = mkdir -p

##
PWD_DIR=$(shell pwd)
LIB_DIR=$(PWD_DIR)/Dogee
TEST_DIR=$(PWD_DIR)/DogeeTest
INC_DIR=$(PWD_DIR)/include
BIN_DIR=$(PWD_DIR)/bin
EX_SIMPLE_DIR=$(PWD_DIR)/examples/SimpleExample
EX_LR_DIR=$(PWD_DIR)/examples/LogisticRegression

CXX ?= g++
CPPFLAGS ?= -std=c++11 -g -I$(INC_DIR) -O3
LIBS ?= -lmemcached -pthread

##
export PWD_DIR CXX CPPFLAGS LIBS LIB_DIR TEST_DIR INC_DIR BIN_DIR

##
all: directories lib test simple_example example_logistic_regression

directories: ${BIN_DIR}

${BIN_DIR}:
	${MKDIR_P} ${BIN_DIR}

lib:
	make -C $(LIB_DIR)

test:
	make -C $(TEST_DIR)

simple_example:
	make -C $(EX_SIMPLE_DIR)

example_logistic_regression:
	make -C $(EX_LR_DIR)

##
clean:
	make -C $(LIB_DIR) clean
	make -C $(TEST_DIR) clean
	make -C $(EX_SIMPLE_DIR) clean
	make -C $(EX_LR_DIR) clean
	rm -rf ${BIN_DIR}

 ##
remake_all:
	make -C $(LIB_DIR) remake
	make -C $(TEST_DIR) remake
	make -C $(EX_SIMPLE_DIR) remake
	make -C $(EX_LR_DIR) remake