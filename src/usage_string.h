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
put {\"table_name\":\"test_table_dim10\", \"key\":\"key0_test\", \"vector\":[1.13, 2.25, 3.73, 4.99, 3.28, 4.22, 6.21, 9.44, 8.12, 5.23], \"attach_value1\":\"attach_value1\", \"attach_value2\":\"attach_value2\", \"attach_value3\":\"attach_value3\"} \n \
                                                                                                                              \n \
* use default parameters, attach_value1=\"\", attach_value2=\"\", attach_value3=\"\"                                          \n \
put {\"table_name\":\"test_table_dim10\", \"key\":\"key0_test\", \"vector\":[1.13, 2.25, 3.73, 4.99, 3.28, 4.22, 6.21, 9.44, 8.12, 5.23]} \n \
                                                                                                                              \n \
2.4 build index                                                                                                               \n \
                                                                                                                              \n \
* whole parameters                                                                                                            \n \
build index {\"table_name\":\"test_table_dim10\", \"index_type\":\"annoy\", \"distance_type\":\"cosine\"}                     \n \
build index {\"table_name\":\"test_table_dim10\", \"index_type\":\"annoy\", \"distance_type\":\"inner_product\"}              \n \
build index {\"table_name\":\"test_table_dim10\", \"index_type\":\"annoy\", \"distance_type\":\"euclidean\"}                  \n \
build index {\"table_name\":\"test_table_dim10\", \"index_type\":\"transparent\", \"distance_type\":\"cosine\"}               \n \
build index {\"table_name\":\"test_table_dim10\", \"index_type\":\"transparent\", \"distance_type\":\"inner_product\"}        \n \
build index {\"table_name\":\"test_table_dim10\", \"index_type\":\"transparent\", \"distance_type\":\"euclidean\"}            \n \
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
get {\"table_name\":\"test_table_dim10\", \"key\":\"key0_test\"}                                                              \n \
                                                                                                                              \n \
2.7 getknn                                                                                                                    \n \
                                                                                                                              \n \
* whole parameters                                                                                                            \n \
getknn {\"table_name\":\"test_table_dim10\", \"key\":\"key0_test\", \"limit\":20, \"index_name\":\"test_table_dim10#annoy#timestamp\"}                     \n \
                                                                                                                              \n \
* use default parameters, use the newest index                                                                                \n \
getknn {\"table_name\":\"test_table_dim10\", \"key\":\"key0_test\", \"limit\":20}                                             \n \
                                                                                                                              \n \
2.8 distance key, calculate the distance between the vector of key1 and the vector of key2 in one table                       \n \
                                                                                                                              \n \
* whole parameters                                                                                                            \n \
distance key {\"table_name\":\"test_table_dim10\", \"key1\":\"key0_test\", \"key2\":\"key1_test\", \"distance_type\":\"cosine\"}      \n \
distance key {\"table_name\":\"test_table_dim10\", \"key1\":\"key0_test\", \"key2\":\"key1_test\", \"distance_type\":\"inner_product\"} \n \
distance key {\"table_name\":\"test_table_dim10\", \"key1\":\"key0_test\", \"key2\":\"key1_test\", \"distance_type\":\"euclidean\"}   \n \
                                                                                                                              \n \
* use default parameters, distance_type = cosine                                                                              \n \
distance key {\"table_name\":\"test_table_dim10\", \"key1\":\"key0_test\", \"key2\":\"key1_test\"}                           \n \
                                                                                                                              \n \
* simple format, distance key table_name key1 key2 distance_type                                                              \n \
distance key test_table_dim10 key0_test key1_test cosine                                                                      \n \
distance key test_table_dim10 key0_test key1_test inner_product                                                               \n \
distance key test_table_dim10 key0_test key1_test euclidean                                                                   \n \
                                                                                                                              \n \
* simple command, use default parameters, distance_type = cosine                                                              \n \
distance key test_table_dim10 key0_test key1_test                                                                             \n \
                                                                                                                              \n \
2.9 distance vector, calculate the distance between vector1 and vector2                                                       \n \
                                                                                                                              \n \
* whole parameters                                                                                                            \n \
distance vector {\"vector1\":[1.13, 2.25, 3.73, 4.99, 3.28, 4.22, 6.21, 9.44, 8.12, 5.23], \"vector2\":[28.63, 22.45, 3.93, 7.93, 4.21, 8.42, 7.29, 32.44, 44.12, 12.23], \"distance_type\":\"cosine\"}           \n \
distance vector {\"vector1\":[1.13, 2.25, 3.73, 4.99, 3.28, 4.22, 6.21, 9.44, 8.12, 5.23], \"vector2\":[28.63, 22.45, 3.93, 7.93, 4.21, 8.42, 7.29, 32.44, 44.12, 12.23], \"distance_type\":\"inner_product\"}    \n \
distance vector {\"vector1\":[1.13, 2.25, 3.73, 4.99, 3.28, 4.22, 6.21, 9.44, 8.12, 5.23], \"vector2\":[28.63, 22.45, 3.93, 7.93, 4.21, 8.42, 7.29, 32.44, 44.12, 12.23], \"distance_type\":\"euclidean\"}        \n \
                                                                                                                              \n \
* use default parameters, distance_type = cosine                                                                              \n \
distance vector {\"vector1\":[1.13, 2.25, 3.73, 4.99, 3.28, 4.22, 6.21, 9.44, 8.12, 5.23], \"vector2\":[28.63, 22.45, 3.93, 7.93, 4.21, 8.42, 7.29, 32.44, 44.12, 12.23]}        \n \
                                                                                                                              \n \
2.10 drop table                                                                                                               \n \
drop table test_table_dim10                                                                                                   \n \
                                                                                                                              \n \
2.11 drop index                                                                                                               \n \
                                                                                                                              \n \
* whole parameters                                                                                                            \n \
drop index {\"index_names\":[\"test_table_dim10#annoy#1628842057\", \"test_table_dim10#annoy#1628840769\"]}                   \n \
                                                                                                                              \n \
* drop one index                                                                                                              \n \
drop index test_table_dim10#annoy#timestamp                                                                                   \n \
                                                                                                                              \n \
2.12 leave index                                                                                                              \n \
                                                                                                                              \n \
* leave the newest count indexes of the table, drop the others                                                                \n \
leave index test_table_dim10 3                                                                                                \n \
                                                                                                                              \n \
-----------------------------------------------------------------------------------------                                     \n \
";


#endif
