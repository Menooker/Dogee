#CXX ?= g++
#CPPFLAGS ?= -std=c++11 -g -I$.../include
#LIBS ?= -lmemcached -pthread
#Dogee: Dogee.o DogeeMemcachedStorage.o DogeeShared.o  DogeeRemote.o  DogeeThreading.o DogeeMemcachedStorage.o DogeeHelper.o DogeeDirectoryCache.o
#	$(CXX) -o $@ $(CXXFLAGS) -Wl,--start-group $^ $(LIBS) -Wl,--end-group 
	# Other rules could be implicitly deduced
libDogee.a: DogeeMemcachedStorage.o DogeeShared.o  DogeeRemote.o  DogeeThreading.o DogeeMemcachedStorage.o DogeeHelper.o DogeeDirectoryCache.o DogeeAccumulator.o DogeeCheckpoint.o DogeeThreadPool.o
	ar -crv $(BIN_DIR)/$@ $^ 
.PHONY:clean
clean:
	# If .o does not exist, don't stop
	rm -f Dogee.o 
	rm -f DogeeMemcachedStorage.o
	rm -f DogeeShared.o
	rm -f DogeeRemote.o
	rm -f DogeeThreading.o
	rm -f DogeeMemcachedStorage.o
	rm -f DogeeHelper.o
	rm -f DogeeDirectoryCache.o
	rm -f DogeeAccumulator.o
	rm -f DogeeCheckpoint.o
	rm -f $(BIN_DIR)/libDogee.a
remake: clean libDogee.a
