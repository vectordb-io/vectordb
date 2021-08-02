#ifndef __VECTORDB_USAGE_STRING_H__
#define __VECTORDB_USAGE_STRING_H__

const char *command_help_str = " \
-----------------------------------------------------------------------------------------                                     \n \
console command example:                                                                                                      \n \
                                                                                                                              \n \
1. simple commands:                                                                                                           \n \
help                                                                                                                          \n \
info                                                                                                                          \n \
ping                                                                                                                          \n \
exit                                                                                                                          \n \
quit                                                                                                                          \n \
version                                                                                                                       \n \
show tables                                                                                                                   \n \
                                                                                                                              \n \
2. complex commands:                                                                                                          \n \
                                                                                                                              \n \
2.1 create table                                                                                                              \n \
                                                                                                                              \n \
* whole parameters                                                                                                            \n \
create table {\"table_name\":\"test_table_dim10\", \"dim\":10, \"partition_num\":8, \"replica_num\":1}                        \n \
                                                                                                                              \n \
* use default parameters, partition_num = 1, replica_num = 1                                                                  \n \
create table {\"table_name\":\"test_table_dim10\", \"dim\":10}                                                                \n \
                                                                                                                              \n \
2.2 desc                                                                                                                      \n \
                                                                                                                              \n \
* desc table_name                                                                                                             \n \
desc test_table_dim10                                                                                                         \n \
                                                                                                                              \n \
* desc partition_name                                                                                                         \n \
desc test_table_dim10#partition_0                                                                                             \n \
                                                                                                                              \n \
* desc replica_name                                                                                                           \n \
desc test_table_dim10#partition_0#replica_0                                                                                   \n \
                                                                                                                              \n \
2.3 put                                                                                                                       \n \
                                                                                                                              \n \
* whole parameters                                                                                                            \n \
put {\"table_name\":\"test_table_dim10\", \"key\":\"kkk\", \"vector\":[1.13, 2.25, 3.73, 4.99, 3.28, 4.22, 6.21, 9.44, 8.12, 5.23], \"attach_value1\":\"attach_value1\", \"attach_value2\":\"attach_value2\", \"attach_value3\":\"attach_value3\"} \n \
                                                                                                                              \n \
* use default parameters, attach_value1=\"\", attach_value2=\"\", attach_value3=\"\"                                          \n \
put {\"table_name\":\"test_table_dim10\", \"key\":\"kkk\", \"vector\":[1.13, 2.25, 3.73, 4.99, 3.28, 4.22, 6.21, 9.44, 8.12, 5.23]} \n \
                                                                                                                              \n \
2.4 build index                                                                                                               \n \
                                                                                                                              \n \
* whole parameters                                                                                                            \n \
build index {\"table_name\":\"test_table_dim10\", \"index_type\":\"annoy\", \"distance_type\":\"cosine\"}                     \n \
build index {\"table_name\":\"test_table_dim10\", \"index_type\":\"annoy\", \"distance_type\":\"inner_product\"}              \n \
build index {\"table_name\":\"test_table_dim10\", \"index_type\":\"annoy\", \"distance_type\":\"euclidean\"}                  \n \
                                                                                                                              \n \
* notice: the \"knn_graph\" is just for test accuracy, don't use it on line!!!                                                \n \
build index {\"table_name\":\"test_table_dim10\", \"index_type\":\"knn_graph\", \"k\":20}                                     \n \
                                                                                                                              \n \
* use default parameters, index_type = annoy, distance_type = cosine                                                          \n \
build index test_table_dim10                                                                                                  \n \
                                                                                                                              \n \
2.5 keys                                                                                                                      \n \
* whole parameters                                                                                                            \n \
keys {\"table_name\":\"test_table_dim10\", \"begin\":0, \"limit\":20}                                                         \n \
                                                                                                                              \n \
* use default parameters, begin = 0, limit = 20                                                                               \n \
keys test_table_dim10                                                                                                         \n \
                                                                                                                              \n \
2.6 get                                                                                                                       \n \
                                                                                                                              \n \
* whole parameters                                                                                                            \n \
get {\"table_name\":\"test_table_dim10\", \"key\":\"kkk\"}                                                                    \n \
                                                                                                                              \n \
2.7 getknn                                                                                                                    \n \
                                                                                                                              \n \
* whole parameters                                                                                                            \n \
getknn {\"table_name\":\"test_table_dim10\", \"key\":\"kkk\", \"limit\":20, \"index_name\":\"annoy.xxx\"}                     \n \
                                                                                                                              \n \
* use default parameters, use the newest index                                                                                \n \
getknn {\"table_name\":\"test_table_dim10\", \"key\":\"kkk\", \"limit\":20}                                                   \n \
                                                                                                                              \n \
2.8 distance key                                                                                                              \n \
                                                                                                                              \n \
* whole parameters                                                                                                            \n \
distance key {\"table_name\":\"test_table_dim10\", \"key1\":\"key_1\", \"key2\":\"key_2\", \"distance_type\":\"cosine\"}      \n \
distance key {\"table_name\":\"test_table_dim10\", \"key1\":\"key_1\", \"key2\":\"key_2\", \"distance_type\":\"inner_product\"} \n \
distance key {\"table_name\":\"test_table_dim10\", \"key1\":\"key_1\", \"key2\":\"key_2\", \"distance_type\":\"euclidean\"}   \n \
                                                                                                                              \n \
* use default parameters, distance_type = cosine                                                                              \n \
distance key {\"table_name\":\"test_table_dim10\", \"key1\":\"key_1\", \"key2\":\"key_2\"}                                    \n \
                                                                                                                              \n \
2.9 distance vector                                                                                                           \n \
                                                                                                                              \n \
* whole parameters                                                                                                            \n \
distance vector {\"vector1\":[1.13, 2.25, 3.73, 4.99, 3.28, 4.22, 6.21, 9.44, 8.12, 5.23], \"vector2\":[28.63, 22.45, 3.93, 7.93, 4.21, 8.42, 7.29, 32.44, 44.12, 12.23], \"distance_type\":\"cosine\"}           \n \
distance vector {\"vector1\":[1.13, 2.25, 3.73, 4.99, 3.28, 4.22, 6.21, 9.44, 8.12, 5.23], \"vector2\":[28.63, 22.45, 3.93, 7.93, 4.21, 8.42, 7.29, 32.44, 44.12, 12.23], \"distance_type\":\"inner_product\"}    \n \
distance vector {\"vector1\":[1.13, 2.25, 3.73, 4.99, 3.28, 4.22, 6.21, 9.44, 8.12, 5.23], \"vector2\":[28.63, 22.45, 3.93, 7.93, 4.21, 8.42, 7.29, 32.44, 44.12, 12.23], \"distance_type\":\"euclidean\"}        \n \
                                                                                                                              \n \
2.10 drop table                                                                                                               \n \
                                                                                                                              \n \
2.11 drop index                                                                                                               \n \
                                                                                                                              \n \
-----------------------------------------------------------------------------------------                                     \n \
";


#endif
