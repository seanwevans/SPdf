.PHONY: spdf_c spdf_cpp clean

spdf_c: main.c spdf.c
	gcc main.c spdf.c -o spdf_c -lpthread

spdf_cpp: main.cpp spdf.cpp
	g++ main.cpp spdf.cpp -o spdf_cpp

clean:
	rm -f spdf_c spdf_cpp
