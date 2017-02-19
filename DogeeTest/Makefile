DogeeTest: DogeeTest.o
	$(CXX) -o $(BIN_DIR)/$@ $(CXXFLAGS) -Wl,--start-group $^ $(BIN_DIR)/libDogee.a $(LIBS) -Wl,--end-group 
.PHONY:clean
clean:
	# If .o does not exist, don't stop
	rm -f DogeeTest.o 
	rm -f $(BIN_DIR)/DogeeTest
remake: clean DogeeTest
