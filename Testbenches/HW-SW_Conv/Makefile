MAKEFLAGS += " -j2 "

all: conv2DTestbench

conv2DTestbench: conv2DTestbench.o CAccelProxy.o CConv2DProxy.o util.o
	g++ -O3 -Wall CAccelProxy.o CConv2DProxy.o util.o conv2DTestbench.o -o conv2DTestbench -lm -lcma -lpthread

conv2DTestbench.o: conv2DTestbench.cpp CAccelProxy.hpp CConv2DProxy.hpp util.hpp
	g++ -c -O3 -Wall conv2DTestbench.cpp -o conv2DTestbench.o
CAccelProxy.o: CAccelProxy.cpp CAccelProxy.hpp CConv2DProxy.hpp util.hpp
	g++ -c -O3 -Wall CAccelProxy.cpp -o CAccelProxy.o
CConv2DProxy.o: CConv2DProxy.cpp CAccelProxy.hpp CConv2DProxy.hpp util.hpp
	g++ -c -O3 -Wall CConv2DProxy.cpp -o CConv2DProxy.o
util.o: util.cpp CAccelProxy.hpp CConv2DProxy.hpp util.hpp
	g++ -c -O3 -Wall util.cpp -o util.o

clean:
	rm -f conv2DTestbench
	rm -f conv2DTestbench.o CAccelProxy.o CConv2DProxy.o util.o
