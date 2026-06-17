# Host-side unit tests (system compiler, not the cross toolchain).
CXX      ?= c++
CXXFLAGS ?= -Wall -Wextra -O2

.PHONY: run clean

run: test_blit
	./test_blit

test_blit: test_blit.cpp ../src/video/blit.cpp ../src/video/blit.h
	$(CXX) $(CXXFLAGS) -o $@ test_blit.cpp ../src/video/blit.cpp

clean:
	rm -f test_blit
