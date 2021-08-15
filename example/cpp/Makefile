all:
	g++ test.cc -I ../../src/output/vectordb/include/ ../../src/output/vectordb/libs/libvdbclient.a /usr/local/lib/libglog.a /usr/local/lib/libprotobuf.a /usr/local/lib/libgrpc++.a /usr/local/lib/libgrpc.a /usr/local/lib/libleveldb.a -lpthread -o test
	g++ test2.cc -I ../../src/output/vectordb/include/ ../../src/output/vectordb/libs/libvdbclient.a /usr/local/lib/libglog.a /usr/local/lib/libprotobuf.a /usr/local/lib/libgrpc++.a /usr/local/lib/libgrpc.a /usr/local/lib/libleveldb.a -lpthread -o test2

clean:
	rm -f test
	rm -f test2
	rm -rf test.data

