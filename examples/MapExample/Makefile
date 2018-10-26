$(BIN_DIR)/MapExample: MapExample.o
	$(CXX) -o $@ $(CXXFLAGS) -Wl,--start-group $^ $(BIN_DIR)/libDogee.a $(LIBS) -Wl,--end-group 
.PHONY:clean
clean:
	# If .o does not exist, don't stop
	rm -f MapExample.o 
	rm -f $(BIN_DIR)/MapExample
remake: clean MapExample
