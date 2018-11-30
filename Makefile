libmysocket.so:mysocket.h mysocket.cpp
	g++ $+ -o $@ -fpic -shared -std=c++11
	cp *.so /usr/lib/

clean:
	rm -rf *.o *.a *.so *.out 
