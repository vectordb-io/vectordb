CC = g++
CFLAGS = -std=c++17 -Wall -g 
# 添加ASAN选项，默认为no
ASAN = no
ifeq ($(ASAN),yes)
    CFLAGS += -fsanitize=address -fno-omit-frame-pointer
    LDFLAGS += -fsanitize=address
endif

COV = no
ifeq ($(COV),yes)
    CFLAGS += -fprofile-arcs -ftest-coverage
    LDFLAGS += -fprofile-arcs -ftest-coverage
endif

INCLUDES = -I.
INCLUDES += -I./src/common
INCLUDES += -I./src/vdb 
INCLUDES += -I./src/util
INCLUDES += -I./third_party/googletest/googletest/include 
INCLUDES += -I./third_party/googletest/googlemock/include 
INCLUDES += -I./third_party/spdlog/include 
INCLUDES += -I./third_party/nlohmann_json/single_include 
INCLUDES += -I./third_party/rocksdb/include
INCLUDES += -I./third_party/hnswlib
INCLUDES += -I./third_party/protobuf/src

LDFLAGS = -L./third_party/googletest/build/lib

LIBS = -lgtest -lgtest_main -lpthread -lstdc++fs 
LIBS += ./third_party/rocksdb/librocksdb.a -lz -lsnappy -llz4 -lzstd -lbz2
LIBS += ./third_party/protobuf/src/.libs/libprotobuf.a

# 目录结构
SRC_DIR = src
OBJ_DIR = output/obj
TEST_DIR = output/test

# 源文件
VDB_SRCS = $(SRC_DIR)/vdb/vdb.cc
VDB_OBJS = $(OBJ_DIR)/vdb/vdb.o

VECTORDB_SRCS = $(SRC_DIR)/vdb/vectordb.cc
VECTORDB_OBJS = $(OBJ_DIR)/vdb/vectordb.o

TABLE_SRCS = $(SRC_DIR)/vdb/table.cc
TABLE_OBJS = $(OBJ_DIR)/vdb/table.o

VDB_PROTO_SRCS = $(SRC_DIR)/vdb/vdb.pb.cc
VDB_PROTO_OBJS = $(OBJ_DIR)/vdb/vdb.pb.o

VINDEX_SRCS = $(SRC_DIR)/vdb/vindex.cc
VINDEX_OBJS = $(OBJ_DIR)/vdb/vindex.o

RETNO_SRCS = $(SRC_DIR)/common/retno.cc
RETNO_OBJS = $(OBJ_DIR)/common/retno.o

LOGGER_SRCS = $(SRC_DIR)/util/logger.cc
LOGGER_OBJS = $(OBJ_DIR)/util/logger.o

UTIL_SRCS = $(SRC_DIR)/util/util.cc
UTIL_OBJS = $(OBJ_DIR)/util/util.o

PB2JSON_SRCS = $(SRC_DIR)/util/pb2json.cc
PB2JSON_OBJS = $(OBJ_DIR)/util/pb2json.o

VDB_TEST_SRCS = $(SRC_DIR)/vdb/vdb_test.cc
VDB_TEST_OBJS = $(OBJ_DIR)/vdb/vdb_test.o

TABLE_TEST_SRCS = $(SRC_DIR)/vdb/table_test.cc
TABLE_TEST_OBJS = $(OBJ_DIR)/vdb/table_test.o

VINDEX_TEST_SRCS = $(SRC_DIR)/vdb/vindex_test.cc
VINDEX_TEST_OBJS = $(OBJ_DIR)/vdb/vindex_test.o

VDB_PROTO_TEST_SRCS = $(SRC_DIR)/vdb/vdb_proto_test.cc
VDB_PROTO_TEST_OBJS = $(OBJ_DIR)/vdb/vdb_proto_test.o

RETNO_TEST_SRCS = $(SRC_DIR)/common/retno_test.cc
RETNO_TEST_OBJS = $(OBJ_DIR)/common/retno_test.o

LOGGER_TEST_SRCS = $(SRC_DIR)/util/logger_test.cc
LOGGER_TEST_OBJS = $(OBJ_DIR)/util/logger_test.o

JSON_TEST_SRCS = $(SRC_DIR)/misc/json_test.cc
JSON_TEST_OBJS = $(OBJ_DIR)/misc/json_test.o

ROCKSDB_TEST_SRCS = $(SRC_DIR)/misc/rocksdb_test.cc
ROCKSDB_TEST_OBJS = $(OBJ_DIR)/misc/rocksdb_test.o

HNSWLIB_TEST_SRCS = $(SRC_DIR)/misc/hnswlib_test.cc
HNSWLIB_TEST_OBJS = $(OBJ_DIR)/misc/hnswlib_test.o

PROTOBUF_TEST_SRCS = $(SRC_DIR)/misc/protobuf_test.cc
PROTOBUF_TEST_OBJS = $(OBJ_DIR)/misc/protobuf_test.o

PERSON_PROTO_SRCS = $(SRC_DIR)/misc/person.proto
PERSON_PROTO_OBJS = $(OBJ_DIR)/misc/person.pb.o

UTIL_TEST_SRCS = $(SRC_DIR)/util/util_test.cc
UTIL_TEST_OBJS = $(OBJ_DIR)/util/util_test.o

DISTANCE_SRCS = $(SRC_DIR)/vdb/distance.cc
DISTANCE_OBJS = $(OBJ_DIR)/vdb/distance.o

DISTANCE_TEST_SRCS = $(SRC_DIR)/vdb/distance_test.cc
DISTANCE_TEST_OBJS = $(OBJ_DIR)/vdb/distance_test.o

VECTORDB_TEST_SRCS = $(SRC_DIR)/vdb/vectordb_test.cc
VECTORDB_TEST_OBJS = $(OBJ_DIR)/vdb/vectordb_test.o

PB2JSON_TEST_SRCS = $(SRC_DIR)/util/pb2json_test.cc
PB2JSON_TEST_OBJS = $(OBJ_DIR)/util/pb2json_test.o

# 目标文件
VDB_TEST = $(TEST_DIR)/vdb_test
TABLE_TEST = $(TEST_DIR)/table_test
VDB_PROTO_TEST = $(TEST_DIR)/vdb_proto_test
RETNO_TEST = $(TEST_DIR)/retno_test
LOGGER_TEST = $(TEST_DIR)/logger_test
JSON_TEST = $(TEST_DIR)/json_test
ROCKSDB_TEST = $(TEST_DIR)/rocksdb_test
HNSWLIB_TEST = $(TEST_DIR)/hnswlib_test
PROTOBUF_TEST = $(TEST_DIR)/protobuf_test
VINDEX_TEST = $(TEST_DIR)/vindex_test
UTIL_TEST = $(TEST_DIR)/util_test
DISTANCE_TEST = $(TEST_DIR)/distance_test
VECTORDB_TEST = $(TEST_DIR)/vectordb_test
PB2JSON_TEST = $(TEST_DIR)/pb2json_test

# 默认目标
all: prepare test

# 准备目录
prepare:
	@mkdir -p $(OBJ_DIR)/vdb
	@mkdir -p $(OBJ_DIR)/common
	@mkdir -p $(OBJ_DIR)/misc
	@mkdir -p $(OBJ_DIR)/util
	@mkdir -p $(TEST_DIR)

# 编译规则
$(OBJ_DIR)/vdb/%.o: $(SRC_DIR)/vdb/%.cc
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/common/%.o: $(SRC_DIR)/common/%.cc
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/misc/%.o: $(SRC_DIR)/misc/%.cc
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/util/%.o: $(SRC_DIR)/util/%.cc
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 链接测试程序
$(VDB_TEST): $(VDB_OBJS) $(VDB_TEST_OBJS) $(TABLE_OBJS) $(VINDEX_OBJS) $(RETNO_OBJS) $(LOGGER_OBJS) $(UTIL_OBJS) $(DISTANCE_OBJS) $(VDB_PROTO_OBJS) $(PB2JSON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(TABLE_TEST): $(TABLE_OBJS) $(TABLE_TEST_OBJS) $(VINDEX_OBJS) $(RETNO_OBJS) $(UTIL_OBJS) $(DISTANCE_OBJS) $(VDB_PROTO_OBJS) $(PB2JSON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(VDB_PROTO_TEST): $(VDB_PROTO_TEST_OBJS) $(VDB_PROTO_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(RETNO_TEST): $(RETNO_OBJS) $(RETNO_TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(LOGGER_TEST): $(LOGGER_OBJS) $(LOGGER_TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(JSON_TEST): $(JSON_TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(ROCKSDB_TEST): $(ROCKSDB_TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(HNSWLIB_TEST): $(HNSWLIB_TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(PROTOBUF_TEST): $(PROTOBUF_TEST_OBJS) $(PERSON_PROTO_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(VINDEX_TEST): $(VINDEX_OBJS) $(VINDEX_TEST_OBJS) $(RETNO_OBJS) $(UTIL_OBJS) $(VDB_PROTO_OBJS) $(PB2JSON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(UTIL_TEST): $(UTIL_OBJS) $(UTIL_TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(DISTANCE_TEST): $(DISTANCE_OBJS) $(DISTANCE_TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(VECTORDB_TEST): $(VECTORDB_TEST_OBJS) $(VECTORDB_OBJS) $(VDB_OBJS) $(TABLE_OBJS) $(VINDEX_OBJS) $(RETNO_OBJS) $(LOGGER_OBJS) $(UTIL_OBJS) $(DISTANCE_OBJS) $(VDB_PROTO_OBJS) $(PB2JSON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(PB2JSON_TEST): $(PB2JSON_TEST_OBJS) $(VDB_PROTO_OBJS) $(PB2JSON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

vdb_test: prepare $(VDB_TEST)
retno_test: prepare $(RETNO_TEST)
json_test: prepare $(JSON_TEST)
rocksdb_test: prepare $(ROCKSDB_TEST)
table_test: prepare proto $(TABLE_TEST)
logger_test: prepare $(LOGGER_TEST)
hnswlib_test: prepare $(HNSWLIB_TEST)
protobuf_test: prepare proto $(PROTOBUF_TEST)
vdb_proto_test: prepare proto $(VDB_PROTO_TEST)
vindex_test: prepare proto $(VINDEX_TEST)
util_test: prepare $(UTIL_TEST)
distance_test: prepare $(DISTANCE_TEST)
vectordb_test: prepare $(VECTORDB_TEST)
pb2json_test: prepare proto $(PB2JSON_TEST)

proto:
	./third_party/protobuf/src/protoc --cpp_out=. src/misc/person.proto
	./third_party/protobuf/src/protoc --cpp_out=. src/vdb/vdb.proto

# 编译测试
test: prepare vdb_test retno_test json_test rocksdb_test table_test logger_test hnswlib_test protobuf_test vdb_proto_test vindex_test util_test distance_test vectordb_test pb2json_test

# 运行测试
run_test: 
	./$(VDB_TEST)
	./$(RETNO_TEST)
	./$(JSON_TEST)
	./$(ROCKSDB_TEST)
	./$(TABLE_TEST)
	./$(LOGGER_TEST)
	./$(HNSWLIB_TEST)
	./$(PROTOBUF_TEST)
	./$(VDB_PROTO_TEST)
	./$(VINDEX_TEST)
	./$(UTIL_TEST)
	./$(DISTANCE_TEST)
	./$(VECTORDB_TEST)
	./$(PB2JSON_TEST)
	./$(VECTORDB_TEST)

# 清理
clean:
	rm -rf $(OBJ_DIR)/* $(TEST_DIR)/* 

.PHONY: all prepare test clean run_test

format:
	clang-format --style=Google -i `find ./src -type f \( -name "*.h" -o -name "*.c" -o -name "*.cc" -o -name "*.cpp" \) | grep -v "*.pb.*"`

# 覆盖率测试
COV_DIR = output/coverage
COV_INFO = $(COV_DIR)/coverage.info
COV_REPORT = $(COV_DIR)/report

coverage: prepare
	@mkdir -p $(COV_DIR)
	$(MAKE) clean
	$(MAKE) test COV=yes -j4
	$(MAKE) run_test
	lcov --capture --directory $(OBJ_DIR) --output-file $(COV_INFO) --ignore-errors mismatch --rc lcov_branch_coverage=1
	lcov --remove $(COV_INFO) '/usr/include/*' '/usr/lib/gcc/x86_64-linux-gnu/13/include/*' '*/third_party/*' '*/src/*_test.cc' --output-file $(COV_INFO) --rc lcov_branch_coverage=1
	genhtml $(COV_INFO) --output-directory $(COV_REPORT) --branch-coverage --function-coverage
	@echo "覆盖率报告已生成在 $(COV_REPORT)/index.html"

clean_coverage:
	rm -rf $(COV_DIR)

.PHONY: coverage clean_coverage