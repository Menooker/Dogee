Dogee: Dogee.o DogeeMemcachedStorage.o DogeeShared.o
	g++ -o Dogee -Wl,--start-group DogeeShared.o DogeeMemcachedStorage.o Dogee.o -lmemcached -Wl,--end-group 
Dogee.o: 
	g++ -c Dogee.cpp -std=c++11
DogeeMemcachedStorage.o: 
	g++ -c  DogeeMemcachedStorage.cpp  -std=c++11
DogeeShared.o: 
	g++ -c  DogeeShared.cpp  -std=c++11

clean:
	rm Dogee.o
	rm DogeeMemcachedStorage.o
	rm DogeeShared.o
	rm Dogee