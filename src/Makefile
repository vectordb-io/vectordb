CC:=g++
CFLAGS+=-c -Wall -g -std=c++17

OBJS_SERVER=server_main.o config.o status.o vectordb_rpc.pb.o vectordb_rpc.grpc.pb.o node.o util.o grpc_server.o meta.o coding.o vectordb_meta.pb.o vengine_annoy.o vec.o engine_manager.o
INCLUDES_SERVER=-I./
LIBS_SERVER=-lglog -lprotobuf -lgrpc++ -lgrpc -lpthread -lleveldb
LFLAGS_SERVER=-L/usr/local/lib

OBJS_CLIENT=client_main.o vectordb_rpc.pb.o vectordb_rpc.grpc.pb.o vclient.o cli_config.o status.o cli_util.o 
INCLUDES_CLIENT=-I./
LIBS_CLIENT=-lglog -lprotobuf -lgrpc++ -lgrpc -lpthread -lleveldb
LFLAGS_CLIENT=-L/usr/local/lib

cs: vectordb-server vectordb-cli
	@echo ""
	@echo "success, go go go!"
	@echo ""

vectordb-server: $(OBJS_SERVER)
	$(CC) $^ -o $@ $(LIBS_SERVER) $(LFLAGS_SERVER)

vectordb-cli: $(OBJS_CLIENT)
	$(CC) $^ -o $@ $(LIBS_CLIENT) $(LFLAGS_CLIENT)

%.o: %.cc %.h
	$(CC) $< $(CFLAGS) -o $@

.PHONY:
proto:
	protoc -I. --cpp_out=. vectordb_rpc.proto
	protoc -I. --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` vectordb_rpc.proto
	protoc -I. --cpp_out=. vectordb_meta.proto

all:
	make proto && make cs 

clean:
	rm -rf *.o 
	rm -rf vectordb-server
	rm -rf vectordb-cli

cleanall: clean
	rm -f vectordb_rpc.pb.*
	rm -f vectordb_rpc.grpc.pb.*
	rm -f vectordb_meta.pb.*



