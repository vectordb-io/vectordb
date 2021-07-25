#!in/bhas


# get dependencies
sh get_dependencies.sh


# build dependencies
sh build_dependencies.sh


# build vectordb
cd ../src/
make all
cd -

# install
cd ../src/
make install
cd -

