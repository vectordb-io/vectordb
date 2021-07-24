#!in/bhas


# get dependencies

cd ./third_party

wget http://vectordb.io/downloads/packages/vectordb_dependencies/glog.v0.4.0.tar.gz
tar zxvf glog.v0.4.0.tar.gz

wget http://vectordb.io/downloads/packages/vectordb_dependencies/grpc.v1.24.0.tar.gz
tar zxvf grpc.v1.24.0.tar.gz

wget http://vectordb.io/downloads/packages/vectordb_dependencies/leveldb.1.22.tar.gz
tar zxvf leveldb.1.22.tar.gz

cd -


# build dependencies

cd third_party/glog.v0.4.0/glog
mkdir b
cd b
cmake ..
make
sudo make install
cd ../../../..


cd third_party/leveldb.1.22/leveldb/
mkdir b
cd b
cmake ..
make
sudo make install
cd ../../../..


cd third_party/grpc.v1.24.0/grpc/third_party/protobuf/
sh autogen.sh
./configure
make
sudo make install
cd ../../../../..


echo "/usr/local/lib" > vectordb_dependencies.conf
sudo mv vectordb_dependencies.conf /etc/ld.so.conf.d/
sudo ldconfig


cd third_party/grpc.v1.24.0/grpc/
make
sudo make install
cd ../../..

sudo ldconfig


# build vectordb

cd src/
make all


# output

cd -
rm -rf output
mkdir -p output/vectordb/bin
mkdir -p output/vectordb/conf
mkdir -p output/vectordb/data
mkdir -p output/vectordb/log
mv src/vectordb-server output/vectordb/bin/
mv src/vectordb-cli output/vectordb/bin/
mv src/vector-inserter output/vectordb/bin/
mv src/get-knn-by-key output/vectordb/bin/


