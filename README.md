

## build

### step 1. build dependency
```
git submodule update --init
cd third_party && sh onekey.sh 

```

### step 2. build
```
# build test
make proto && make -j4

# build asan test
make proto && make ASAN=yes -j4
```

