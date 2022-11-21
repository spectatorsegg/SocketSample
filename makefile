all : server client
.PHONY : all
server : server.c config.h
	gcc -Wall -O2 server.c -L/usr/local/lib -lwolfssl -pthread -o server 
client : client.c config.h opencv_test.cpp opencv_test.h
	g++ -Wall -O2 client.c -L/usr/local/lib -lwolfssl opencv_test.cpp -o client `pkg-config --cflags --libs opencv`
clean :
	rm server client