all:
	g++ test.cc -I ../../src/output/vectordb/include/ ../../src/output/vectordb/libs/libvdbclient.a /usr/local/lib/libglog.a /usr/local/lib/libprotobuf.a /usr/local/lib/libgrpc++.a /usr/local/lib/libgrpc.a /usr/local/lib/libleveldb.a -lpthread -o test
	g++ test_simple.cc -I ../../src/output/vectordb/include/ ../../src/output/vectordb/libs/libvdbclient.a /usr/local/lib/libglog.a /usr/local/lib/libprotobuf.a /usr/local/lib/libgrpc++.a /usr/local/lib/libgrpc.a /usr/local/lib/libleveldb.a -lpthread -o test_simple

clean:
	rm -f test
	rm -f test_simple
	rm -rf test.data

