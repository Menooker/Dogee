CXX ?= g++
CPPFLAGS ?= -std=c++11
LIBS ?= -lmemcached
Dogee: Dogee.o DogeeMemcachedStorage.o DogeeShared.o
	$(CXX) -o $@ $(CXXFLAGS) -Wl,--start-group $^ $(LIBS) -Wl,--end-group # Other rules could be implicitly deduced
.PHONY:clean
clean:
	rm -f Dogee.o # If .o does not exist, don't stop
	rm -f DogeeMemcachedStorage.o
	rm -f DogeeShared.o
	rm -f Dogee
