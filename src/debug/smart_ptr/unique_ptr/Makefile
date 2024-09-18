# 编译器
CXX = g++
# 编译参数
CXXFLAGS = -g -Wall -std=c++14

# 查找所有的 .cc 文件
SRC = $(wildcard *.cc)
# 把 .cc 文件后缀替换成可执行文件
EXES = $(SRC:.cc=)

# 规则
all: $(EXES)

# 将每个 .cc 文件编译成对应的可执行文件
%: %.cc
	$(CXX) $(CXXFLAGS) -o $@ $<

# clean 规则
clean:
	rm -f $(EXES)

format:
	clang-format --style=Google -i `find ./ -type f \( -name "*.h" -o -name "*.c" -o -name "*.cc" -o -name "*.cpp" \)`