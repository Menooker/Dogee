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
EX_KM_DIR=$(PWD_DIR)/examples/K-means
EX_NMF_DIR=$(PWD_DIR)/examples/NMF
EX_PR_DIR=$(PWD_DIR)/examples/PageRank
EX_KM_CP_DIR=$(PWD_DIR)/examples/K-means-checkpoint
EX_MAP_DIR=$(PWD_DIR)/examples/MapExample

CXX ?= g++
CPPFLAGS ?= -std=c++11 -g -I$(INC_DIR) -O3 -ffast-math -march=native
LIBS ?= -lmemcached -pthread

##
export PWD_DIR CXX CPPFLAGS LIBS LIB_DIR TEST_DIR INC_DIR BIN_DIR

##
all: directories lib test simple_example example_logistic_regression example_kmeans example_nmf example_pagerank example_map

directories: ${BIN_DIR}

${BIN_DIR}:
	${MKDIR_P} ${BIN_DIR}

lib:
	${MAKE} -C $(LIB_DIR)

test:
	${MAKE} -C $(TEST_DIR)

simple_example:
	${MAKE} -C $(EX_SIMPLE_DIR)

example_logistic_regression:
	${MAKE} -C $(EX_LR_DIR)

example_kmeans:
	${MAKE} -C $(EX_KM_DIR)

example_nmf:
	${MAKE} -C $(EX_NMF_DIR)

example_pagerank:
	${MAKE} -C $(EX_PR_DIR)

example_kmeans_checkpoint:
	${MAKE} -C $(EX_KM_CP_DIR)

example_map:
	${MAKE} -C $(EX_MAP_DIR)

##
clean:
	${MAKE} -C $(LIB_DIR) clean
	${MAKE} -C $(TEST_DIR) clean
	${MAKE} -C $(EX_SIMPLE_DIR) clean
	${MAKE} -C $(EX_LR_DIR) clean
	${MAKE} -C $(EX_KM_DIR) clean
	${MAKE} -C $(EX_NMF_DIR) clean
	${MAKE} -C $(EX_PR_DIR) clean
	${MAKE} -C $(EX_KM_CP_DIR) clean
	${MAKE} -C $(EX_MAP_DIR) clean
	rm -rf ${BIN_DIR}

 ##
remake_all:
	${MAKE} -C $(LIB_DIR) remake
	${MAKE} -C $(TEST_DIR) remake
	${MAKE} -C $(EX_SIMPLE_DIR) remake
	${MAKE} -C $(EX_LR_DIR) remake
	${MAKE} -C $(EX_KM_DIR) remake
	${MAKE} -C $(EX_NMF_DIR) remake
	${MAKE} -C $(EX_PR_DIR) remake
	${MAKE} -C $(EX_KM_CP_DIR) remake
	${MAKE} -C $(EX_MAP_DIR) remake