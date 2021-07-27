all:
	g++ test.cc -I ../../src/output/vectordb/include/ ../../src/output/vectordb/libs/libvdbclient.a /usr/local/lib/libglog.a /usr/local/lib/libprotobuf.a /usr/local/lib/libgrpc++.a /usr/local/lib/libgrpc.a /usr/local/lib/libleveldb.a -lpthread -o test
clean:
	rm -f test
