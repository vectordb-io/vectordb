#!in/bhas


# build dependencies

cd ../third_party/glog.v0.4.0/glog
mkdir b
cd b
cmake ..
make -j 4
sudo make install
cd ../../../../script/


cd ../third_party/leveldb.1.22/leveldb/
mkdir b
cd b
cmake ..
make -j 4
sudo make install
cd ../../../../script/


cd ../third_party/grpc.v1.24.0/grpc/third_party/protobuf/
sh autogen.sh
./configure
make -j 4
sudo make install
cd ../../../../../script/


echo "/usr/local/lib" > vectordb_dependencies.conf
sudo mv vectordb_dependencies.conf /etc/ld.so.conf.d/
sudo ldconfig


cd ../third_party/grpc.v1.24.0/grpc/
make -j 4
sudo make install
cd ../../../script/


sudo ldconfig

