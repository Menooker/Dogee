LogisticRegression: LogisticRegression.o
	$(CXX) -o $(BIN_DIR)/$@ $(CXXFLAGS) -Wl,--start-group $^ $(BIN_DIR)/libDogee.a $(LIBS) -Wl,--end-group 
.PHONY:clean
clean:
	# If .o does not exist, don't stop
	rm -f LogisticRegression.o 
	rm -f $(BIN_DIR)/LogisticRegression
remake: clean LogisticRegression
