#!/bin/bash

MODEL=""
TOOL=""

# 解析命令行参数
for arg in "$@"
do
    if [ "${arg%%=*}" = "--model" ]; then
        MODEL="${arg#*=}"
    elif [ "${arg%%=*}" = "--tool" ]; then
        TOOL="${arg#*=}"
    fi
done

# 检查参数是否存在，如果不存在则提示用户输入
if [ -z "$MODEL" ]; then
    echo "example:"
    echo "sh $0 --model=claude-3.7-sonnet --tool=cursor"
    echo ""
    exit 1
fi

if [ -z "$TOOL" ]; then
    echo "example:"
    echo "sh $0 --model=claude-3.7-sonnet --tool=cursor"
    echo ""
    exit 1
fi

timestamp=$(date +%Y-%m-%d-%H-%M-%S)
mkdir -p ./test_result
file_name=./test_result/result.$timestamp.txt
touch $file_name

echo "Test Time: $timestamp" >> $file_name
echo "" >> $file_name
echo "Model: $MODEL" >> $file_name
echo "Tool: $TOOL" >> $file_name

# 打印操作系统版本
echo "OS: $(cat /etc/os-release | grep PRETTY_NAME | cut -d= -f2 | tr -d '"')" >> $file_name

# 打印操作系统内核版本
echo "Kernel: $(uname -r)" >> $file_name

# 打印g++版本
echo "" >> $file_name
echo "g++: $(g++ --version)" >> $file_name

# 打印gcc版本
echo "" >> $file_name
echo "gcc: $(gcc --version)" >> $file_name

# 打印python版本
echo "" >> $file_name
echo "python: $(python --version)" >> $file_name

echo "" >> $file_name
echo "Asan Test:" >> $file_name
echo "" >> $file_name

# 执行测试并捕获返回值
make clean && make ASAN=yes && make run_test
TEST_RESULT=$?

# 检查测试结果
if [ "$TEST_RESULT" -eq 0 ]; then
    echo "Asan Test Pass" >> $file_name
else
    echo "Asan Test Fail" >> $file_name
fi