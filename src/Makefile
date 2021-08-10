CC:=g++
CFLAGS+=-c -Wall -g -std=c++17 -DANNOYLIB_MULTITHREADED_BUILD
INCLUDES=-I./ -I../third_party/jsonxx.v0.9.2/include/ -I../third_party/annoy.v1.17.0/include/

OBJS_ENGINE_MANAGER_TEST2=engine_manager_test2.o engine_manager.o vengine.o vec.o status.o util.o vectordb_rpc.pb.o meta.o config.o node.o grpc_server.o vectordb_rpc.pb.o vectordb_rpc.grpc.pb.o coding.o
OBJS_ENGINE_MANAGER_TEST=engine_manager_test.o engine_manager.o vengine.o vec.o status.o util.o vectordb_rpc.pb.o meta.o config.o node.o grpc_server.o vectordb_rpc.pb.o vectordb_rpc.grpc.pb.o coding.o
LIBS_ENGINE_MANAGER_TEST=-lglog -lprotobuf -lgrpc++ -lgrpc -lleveldb -lpthread
LFLAGS_ENGINE_MANAGER_TEST=-L/usr/local/lib

OBJS_VEC_TEST=vec_test.o vec.o vectordb_rpc.pb.o
OBJS_VECOBJ_TEST=vecobj_test.o vec.o vectordb_rpc.pb.o
LIBS_VEC_TEST=-lglog -lprotobuf -lgrpc++ -lgrpc -lleveldb -lpthread
LFLAGS_VEC_TEST=-L/usr/local/lib

OBJS_VENGINE_ADD_BY_META_TEST=vengine_add_by_meta_test.o vengine.o vec.o status.o util.o vectordb_rpc.pb.o config.o meta.o coding.o
OBJS_VENGINE_LOAD_TEST=vengine_load_test.o vengine.o vec.o status.o util.o vectordb_rpc.pb.o
OBJS_VENGINE_TEST=vengine_test.o vengine.o vec.o status.o util.o vectordb_rpc.pb.o
OBJS_VENGINE_PUT_TEST=vengine_put_test.o vengine.o vec.o status.o util.o vectordb_rpc.pb.o
OBJS_VENGINE_GET_TEST=vengine_get_test.o vengine.o vec.o status.o util.o vectordb_rpc.pb.o
LIBS_VENGINE_TEST=-lglog -lprotobuf -lgrpc++ -lgrpc -lleveldb -lpthread
LFLAGS_VENGINE_TEST=-L/usr/local/lib

OBJS_VINDEX_ANNOY_TEST=vindex_annoy_test.o vengine.o vec.o status.o util.o vectordb_rpc.pb.o vindex_annoy.o coding.o
LIBS_VINDEX_ANNOY_TEST=-lglog -lprotobuf -lgrpc++ -lgrpc -lleveldb -lpthread
LFLAGS_VINDEX_ANNOY_TEST=-L/usr/local/lib

OBJS_META_FOREACH_TEST=meta_foreach_test.o meta.o config.o status.o util.o vectordb_rpc.pb.o coding.o
OBJS_META_LOAD_TEST=meta_load_test.o meta.o config.o status.o util.o vectordb_rpc.pb.o coding.o
OBJS_META_TEST=meta_test.o meta.o config.o status.o util.o vectordb_rpc.pb.o coding.o
LIBS_META_TEST=-lglog -lprotobuf -lgrpc++ -lgrpc -lleveldb -lpthread
LFLAGS_META_TEST=-L/usr/local/lib

OBJS_CONFIG_TEST=config_test.o config.o status.o util.o
LIBS_CONFIG_TEST=-lglog -lprotobuf -lgrpc++ -lgrpc -lleveldb -lpthread
LFLAGS_CONFIG_TEST=-L/usr/local/lib

OBJS_LOG_TEST=log_test.o config.o status.o util.o
LIBS_LOG_TEST=-lglog -lprotobuf -lgrpc++ -lgrpc -lleveldb -lpthread
LFLAGS_LOG_TEST=-L/usr/local/lib

OBJS_SERVER=server_main.o config.o status.o vectordb_rpc.pb.o vectordb_rpc.grpc.pb.o node.o util.o grpc_server.o meta.o coding.o vengine.o vec.o engine_manager.o vindex_annoy.o vindex_knn_graph.o
LIBS_SERVER=/usr/local/lib/libglog.a /usr/local/lib/libprotobuf.a /usr/local/lib/libgrpc++.a /usr/local/lib/libgrpc.a /usr/local/lib/libleveldb.a -lpthread
LFLAGS_SERVER=-L/usr/local/lib

OBJS_CLIENT=client_main.o vectordb_rpc.pb.o vectordb_rpc.grpc.pb.o vectordb_cli.o cli_config.o status.o cli_util.o vdb_client.o
LIBS_CLIENT=/usr/local/lib/libglog.a /usr/local/lib/libprotobuf.a /usr/local/lib/libgrpc++.a /usr/local/lib/libgrpc.a /usr/local/lib/libleveldb.a -lpthread
LFLAGS_CLIENT=-L/usr/local/lib

OBJS_INSERTER=vector_inserter.o libvdbclient.a
LIBS_INSERTER=/usr/local/lib/libglog.a /usr/local/lib/libprotobuf.a /usr/local/lib/libgrpc++.a /usr/local/lib/libgrpc.a /usr/local/lib/libleveldb.a -lpthread
LFLAGS_INSERTER=-L/usr/local/lib

OBJS_API=vectordb_rpc.pb.o vectordb_rpc.grpc.pb.o vdb_client.o status.o 
LIBS_API=
LFLAGS_API=-L/usr/local/lib

OBJS_IMG=image_inserter.o vectordb_rpc.pb.o vectordb_rpc.grpc.pb.o vdb_client.o status.o 
LIBS_IMG=/usr/local/lib/libglog.a /usr/local/lib/libprotobuf.a /usr/local/lib/libgrpc++.a /usr/local/lib/libgrpc.a /usr/local/lib/libleveldb.a -lpthread
LFLAGS_IMG=-L/usr/local/lib

OBJS_GETKNN=get_knn_by_key.o vectordb_rpc.pb.o vectordb_rpc.grpc.pb.o vdb_client.o status.o 
LIBS_GETKNN=/usr/local/lib/libglog.a /usr/local/lib/libprotobuf.a /usr/local/lib/libgrpc++.a /usr/local/lib/libgrpc.a /usr/local/lib/libleveldb.a -lpthread
LFLAGS_GETKNN=-L/usr/local/lib

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

config_test: $(OBJS_CONFIG_TEST)
	$(CC) $^ -o $@ $(LIBS_CONFIG_TEST) $(LFLAGS_CONFIG_TEST)

log_test: $(OBJS_LOG_TEST)
	$(CC) $^ -o $@ $(LIBS_LOG_TEST) $(LFLAGS_LOG_TEST)

meta_test: $(OBJS_META_TEST)
	$(CC) $^ -o $@ $(LIBS_META_TEST) $(LFLAGS_META_TEST)

meta_load_test: $(OBJS_META_LOAD_TEST)
	$(CC) $^ -o $@ $(LIBS_META_TEST) $(LFLAGS_META_TEST)

meta_foreach_test: $(OBJS_META_FOREACH_TEST)
	$(CC) $^ -o $@ $(LIBS_META_TEST) $(LFLAGS_META_TEST)

vengine_test: $(OBJS_VENGINE_TEST)
	$(CC) $^ -o $@ $(LIBS_VENGINE_TEST) $(LFLAGS_VENGINE_TEST)

vengine_put_test: $(OBJS_VENGINE_PUT_TEST)
	$(CC) $^ -o $@ $(LIBS_VENGINE_TEST) $(LFLAGS_VENGINE_TEST)

vengine_get_test: $(OBJS_VENGINE_GET_TEST)
	$(CC) $^ -o $@ $(LIBS_VENGINE_TEST) $(LFLAGS_VENGINE_TEST)

vengine_load_test: $(OBJS_VENGINE_LOAD_TEST)
	$(CC) $^ -o $@ $(LIBS_VENGINE_TEST) $(LFLAGS_VENGINE_TEST)

vengine_add_by_meta_test: $(OBJS_VENGINE_ADD_BY_META_TEST)
	$(CC) $^ -o $@ $(LIBS_VENGINE_TEST) $(LFLAGS_VENGINE_TEST)

vec_test: $(OBJS_VEC_TEST)
	$(CC) $^ -o $@ $(LIBS_VEC_TEST) $(LFLAGS_VEC_TEST)

vecobj_test: $(OBJS_VECOBJ_TEST)
	$(CC) $^ -o $@ $(LIBS_VEC_TEST) $(LFLAGS_VEC_TEST)

engine_manager_test : $(OBJS_ENGINE_MANAGER_TEST)
	$(CC) $^ -o $@ $(LIBS_ENGINE_MANAGER_TEST) $(LFLAGS_ENGINE_MANAGER_TEST)

engine_manager_test2 : $(OBJS_ENGINE_MANAGER_TEST2)
	$(CC) $^ -o $@ $(LIBS_ENGINE_MANAGER_TEST) $(LFLAGS_ENGINE_MANAGER_TEST)

vindex_annoy_test: $(OBJS_VINDEX_ANNOY_TEST)
	$(CC) $^ -o $@ $(LIBS_VINDEX_ANNOY_TEST) $(LFLAGS_VINDEX_ANNOY_TEST)

%.o: %.cc %.h
	$(CC) $< $(INCLUDES) $(CFLAGS) -o $@

.PHONY:

test: config_test log_test meta_test meta_load_test meta_foreach_test vengine_test vengine_load_test vec_test vecobj_test vengine_put_test vengine_get_test engine_manager_test engine_manager_test2 vengine_add_by_meta_test vindex_annoy_test
	@echo "make test finish!"

api: $(OBJS_API)
	ar rcs libvdbclient.a $^ 

cs: vectordb-server vectordb-cli vector-inserter image-inserter get-knn-by-key 
	@echo ""
	@echo "success, go go go!"
	@echo ""

proto:
	protoc -I. --cpp_out=. vectordb_rpc.proto
	protoc -I. --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` vectordb_rpc.proto

install:
	rm -rf output
	mkdir -p output/vectordb/bin
	mkdir -p output/vectordb/conf
	mkdir -p output/vectordb/data
	mkdir -p output/vectordb/log
	mkdir -p output/vectordb/include
	mkdir -p output/vectordb/libs
	cp vectordb-server output/vectordb/bin/
	cp vectordb-cli output/vectordb/bin/
	cp vector-inserter output/vectordb/bin/
	cp get-knn-by-key output/vectordb/bin/
	cp libvdbclient.a output/vectordb/libs
	cp vdb_client.h output/vectordb/include
	cp status.h output/vectordb/include
	cp slice.h output/vectordb/include
	cp vectordb_rpc.grpc.pb.h output/vectordb/include
	cp vectordb_rpc.pb.h output/vectordb/include

uninstall:
	rm -rf output

all:
	make proto && make api && make cs && make install

clean:
	rm -rf *.o 
	rm -rf vectordb-server
	rm -rf vectordb-cli
	rm -rf vector-inserter
	rm -rf image-inserter
	rm -rf get-knn-by-key
	rm -rf libvdbclient.a
	rm -rf config_test
	rm -rf log_test
	rm -rf meta_load_test
	rm -rf meta_test
	rm -rf meta_foreach_test
	rm -rf vengine_test
	rm -rf vengine_load_test
	rm -rf vec_test
	rm -rf vecobj_test
	rm -rf vengine_put_test
	rm -rf vengine_get_test 
	rm -rf engine_manager_test
	rm -rf engine_manager_test2
	rm -rf vengine_add_by_meta_test
	rm -rf vindex_annoy_test 

cleanall: clean uninstall
	rm -f vectordb_rpc.pb.*
	rm -f vectordb_rpc.grpc.pb.*



