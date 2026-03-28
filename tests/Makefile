CXX = g++
CXXFLAGS = -std=c++14 -I. -I../src/audio -I../src -DARDUINO=1
OBJS = test_bassgroove.o ../src/audio/BassGroove.o mock_env.o

runner: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) runner
