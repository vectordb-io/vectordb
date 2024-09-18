all:
	gcc -I ../../../third_party/libuv/include -Wall -g demo.c ../../../third_party/libuv/.libs/libuv.a -lpthread -ldl -o demo
	gcc -I ../../../third_party/libuv/include -Wall -g demo2.c ../../../third_party/libuv/.libs/libuv.a -lpthread -ldl -o demo2
clean:
	rm -f demo demo2

