.PHONY: spdf_c spdf_cpp clean

spdf_c: spdf.c
	gcc spdf.c -o spdf_c -lpthread

spdf_cpp: spdf.cpp
	g++ spdf.cpp -o spdf_cpp

clean:
	rm -f spdf_c spdf_cpp
