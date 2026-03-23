MAKEFLAGS += " -j2 "
CFLAGS=-O3 -Wall

all: obj cnnSolver

cnnSolver: obj/cnnSolver.o obj/model.o obj/cnn.o obj/CAccelProxy.o obj/CConv2DProxy.o obj/CMaxPoolProxy.o
	g++ $(CFLAGS) obj/cnnSolver.o obj/model.o obj/cnn.o obj/CAccelProxy.o obj/CConv2DProxy.o obj/CMaxPoolProxy.o -o cnnSolver -lm -lcma -lpthread

obj/cnnSolver.o: src/cnnSolver.cpp src/model.h src/cnn.h
	g++ -c $(CFLAGS) src/cnnSolver.cpp -o obj/cnnSolver.o
obj/model.o: src/model.cpp src/model.h src/cnn.h
	g++ -c $(CFLAGS) src/model.cpp -o obj/model.o
obj/cnn.o: src/cnn.cpp src/cnn.h
	g++ -c $(CFLAGS) src/cnn.cpp -o obj/cnn.o
obj/CAccelProxy.o: src/CAccelProxy.cpp src/CAccelProxy.hpp
	g++ -c $(CFLAGS) src/CAccelProxy.cpp -o obj/CAccelProxy.o
obj/CConv2DProxy.o: src/CConv2DProxy.cpp src/CConv2DProxy.hpp
	g++ -c $(CFLAGS) src/CConv2DProxy.cpp -o obj/CConv2DProxy.o
obj/CMaxPoolProxy.o: src/CMaxPoolProxy.cpp src/CMaxPoolProxy.hpp
	g++ -c $(CFLAGS) src/CMaxPoolProxy.cpp -o obj/CMaxPoolProxy.o

obj:
	mkdir obj/

clean:
	rm -f cnnSolver
	rm -rf obj/
