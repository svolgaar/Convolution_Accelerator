all: conv2DTestbench

conv2DTestbench: conv2DTestbench.cpp util.cpp util.hpp
	g++ -O3 -Wall conv2DTestbench.cpp util.cpp -o conv2DTestbench -lm -lpthread

clean:
	rm -f conv2DTestbench
