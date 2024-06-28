#!/bin/bash

dir="/tmp/remu_test_dir/log/"

# 判断命令行参数的数量
if [ $# -eq 0 ]; then
    echo "usage:"
    echo sh $0 127.0.0.1:9000#0 127.0.0.1:9001#0
    echo ""
    exit 1
fi

joined=""

# 使用$@，不能用$*，这里两者有区别
for arg in "$@"; do
    echo ${arg}
    if [ "${joined}" = "" ]; then
        # 第一个参数前面不添加下划线
        joined=$arg
    else
        # 后续参数前面添加下划线
        joined="${joined}_$arg"
    fi
done

joined_keys_file=${dir}/temp/keys.${joined}
rm -f ${joined_keys_file}
touch ${joined_keys_file}
echo "touch ${joined_keys_file}"

for key in $*;do
    #echo $key
    cat ${dir}/temp/keys.${key} >> ${joined_keys_file}
done
echo "generate ${joined_keys_file}"

rm -f ${joined_keys_file}.sm.tmp
touch ${joined_keys_file}.sm.tmp

awk 'NR==FNR { key[$1] = 1; next } $1 in key' "${joined_keys_file}" "${dir}/remu.log.sm" >> ${joined_keys_file}.sm.tmp

cat ${joined_keys_file}.sm.tmp | awk '{if (last != $1) {if (NR != 1) print ""; last = $1} print}' > ${joined_keys_file}.sm
cp ${joined_keys_file}.sm ${dir}
echo "generate ${joined_keys_file}.sm"


