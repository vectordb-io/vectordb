CC:=g++
CFLAGS+=-c -Wall -g -std=c++17

OBJS_SERVER=server_main.o config.o status.o vectordb_rpc.pb.o vectordb_rpc.grpc.pb.o node.o util.o grpc_server.o meta.o coding.o vengine.o vec.o engine_manager.o vindex_annoy.o
INCLUDES_SERVER=-I./ -I ./annoy.v1.10.0/include
LIBS_SERVER=-lglog -lprotobuf -lgrpc++ -lgrpc -lpthread -lleveldb
LFLAGS_SERVER=-L/usr/local/lib

OBJS_CLIENT=client_main.o vectordb_rpc.pb.o vectordb_rpc.grpc.pb.o vclient.o cli_config.o status.o cli_util.o 
INCLUDES_CLIENT=-I./
LIBS_CLIENT=-lglog -lprotobuf -lgrpc++ -lgrpc -lpthread -lleveldb
LFLAGS_CLIENT=-L/usr/local/lib

OBJS_INSERTER=vector_inserter.o vectordb_rpc.pb.o vectordb_rpc.grpc.pb.o vdb_client.o status.o 
INCLUDES_INSERTER=-I./
LIBS_INSERTER=-lglog -lprotobuf -lgrpc++ -lgrpc -lpthread -lleveldb
LFLAGS_INSERTER=-L/usr/local/lib

cs: vectordb-server vectordb-cli vector-inserter
	@echo ""
	@echo "success, go go go!"
	@echo ""

vectordb-server: $(OBJS_SERVER)
	$(CC) $^ -o $@ $(LIBS_SERVER) $(LFLAGS_SERVER)

vectordb-cli: $(OBJS_CLIENT)
	$(CC) $^ -o $@ $(LIBS_CLIENT) $(LFLAGS_CLIENT)

vector-inserter: $(OBJS_INSERTER)
	$(CC) $^ -o $@ $(LIBS_INSERTER) $(LFLAGS_INSERTER)

%.o: %.cc %.h
	$(CC) $< $(INCLUDES_SERVER) $(CFLAGS) -o $@

.PHONY:
proto:
	protoc -I. --cpp_out=. vectordb_rpc.proto
	protoc -I. --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` vectordb_rpc.proto

all:
	make proto && make cs 

clean:
	rm -rf *.o 
	rm -rf vectordb-server
	rm -rf vectordb-cli
	rm -rf vector-inserter

cleanall: clean
	rm -f vectordb_rpc.pb.*
	rm -f vectordb_rpc.grpc.pb.*



