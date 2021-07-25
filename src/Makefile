CC:=g++
CFLAGS+=-c -Wall -g -std=c++17
INCLUDES=-I./ -I../third_party/jsonxx.v0.9.2/include/ -I../third_party/annoy.v1.10.0/include/

OBJS_SERVER=server_main.o config.o status.o vectordb_rpc.pb.o vectordb_rpc.grpc.pb.o node.o util.o grpc_server.o meta.o coding.o vengine.o vec.o engine_manager.o vindex_annoy.o vindex_knn_graph.o
LIBS_SERVER=-static -lglog -lprotobuf -lgrpc++ -lgrpc -lpthread -lleveldb
LFLAGS_SERVER=-L/usr/local/lib

OBJS_CLIENT=client_main.o vectordb_rpc.pb.o vectordb_rpc.grpc.pb.o vclient.o cli_config.o status.o cli_util.o 
LIBS_CLIENT=-static -lglog -lprotobuf -lgrpc++ -lgrpc -lpthread -lleveldb
LFLAGS_CLIENT=-L/usr/local/lib

OBJS_INSERTER=vector_inserter.o vectordb_rpc.pb.o vectordb_rpc.grpc.pb.o vdb_client.o status.o 
LIBS_INSERTER=-static -lglog -lprotobuf -lgrpc++ -lgrpc -lpthread -lleveldb
LFLAGS_INSERTER=-L/usr/local/lib

OBJS_IMG=image_inserter.o vectordb_rpc.pb.o vectordb_rpc.grpc.pb.o vdb_client.o status.o 
LIBS_IMG=-static -lglog -lprotobuf -lgrpc++ -lgrpc -lpthread -lleveldb
LFLAGS_IMG=-L/usr/local/lib

OBJS_GETKNN=get_knn_by_key.o vectordb_rpc.pb.o vectordb_rpc.grpc.pb.o vdb_client.o status.o 
LIBS_GETKNN=-static -lglog -lprotobuf -lgrpc++ -lgrpc -lpthread -lleveldb
LFLAGS_GETKNN=-L/usr/local/lib

cs: vectordb-server vectordb-cli vector-inserter image-inserter get-knn-by-key
	@echo ""
	@echo "success, go go go!"
	@echo ""

vectordb-server: $(OBJS_SERVER)
	$(CC) $^ -o $@ $(LIBS_SERVER) $(LFLAGS_SERVER)

vectordb-cli: $(OBJS_CLIENT)
	$(CC) $^ -o $@ $(LIBS_CLIENT) $(LFLAGS_CLIENT)

vector-inserter: $(OBJS_INSERTER)
	$(CC) $^ -o $@ $(LIBS_INSERTER) $(LFLAGS_INSERTER)

image-inserter: $(OBJS_IMG)
	$(CC) $^ -o $@ $(LIBS_IMG) $(LFLAGS_IMG)

get-knn-by-key: $(OBJS_GETKNN)
	$(CC) $^ -o $@ $(LIBS_IMG) $(LFLAGS_IMG)

%.o: %.cc %.h
	$(CC) $< $(INCLUDES) $(CFLAGS) -o $@

.PHONY:
proto:
	protoc -I. --cpp_out=. vectordb_rpc.proto
	protoc -I. --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` vectordb_rpc.proto

install:
	rm -rf output
	mkdir -p output/vectordb/bin
	mkdir -p output/vectordb/conf
	mkdir -p output/vectordb/data
	mkdir -p output/vectordb/log
	cp vectordb-server output/vectordb/bin/
	cp vectordb-cli output/vectordb/bin/
	cp vector-inserter output/vectordb/bin/
	cp get-knn-by-key output/vectordb/bin/

uninstall:
	rm -rf output

all:
	make proto && make cs 

clean:
	rm -rf *.o 
	rm -rf vectordb-server
	rm -rf vectordb-cli
	rm -rf vector-inserter
	rm -rf image-inserter
	rm -rf get-knn-by-key

cleanall: clean uninstall
	rm -f vectordb_rpc.pb.*
	rm -f vectordb_rpc.grpc.pb.*



