#!/bin/bash

dir="/tmp/remu_test_dir/log/"

cat ${dir}/remu.log | grep -E "state_change|event_start|event_stop|state_begin|event_recv|event_send|event_timer|event_other|state_end" | awk '{if (last != $1) {if (NR != 1) print ""; last = $1} print}' > ${dir}/remu.log.sm
cat ${dir}/remu.log | grep "raft-tick:" > ${dir}/remu.log.tick
cat ${dir}/remu.log | grep "global-state:" | awk '{if (last != $1) {if (NR != 1) print ""; last = $1} print}' > ${dir}/remu.log.global

rm -rf ${dir}/temp
mkdir ${dir}/temp
cat ${dir}/remu.log.sm | grep state_change | awk '{print $7}' | sort | uniq > ${dir}/temp/ids
for id in `cat ${dir}/temp/ids`; do
    echo "generate log for ${id} ..."
    cat ${dir}/remu.log.sm | grep state_change | grep ${id} | awk '{print $1}' | sort | uniq > ${dir}/temp/keys.${id}
done

rm -f ${file}.sm.tmp
touch ${file}.sm.tmp

for file in `ls ${dir}/temp/keys.*`; do
    echo "analyzing ${file} ..."

    echo "generate ${file}.sm.tmp ..."
    awk 'NR==FNR { key[$1] = 1; next } $1 in key' "${file}" "${dir}/remu.log.sm" >> ${file}.sm.tmp

    cat ${file}.sm.tmp | awk '{if (last != $1) {if (NR != 1) print ""; last = $1} print}' > ${file}.sm
    cp ${file}.sm ${dir}
done


# 定义文件路径
file_path="${dir}/temp/ids"

# 检查文件是否存在
if [ ! -f "$file_path" ]; then
    echo "文件不存在: $file_path"
    exit 1
fi

# 读取文件中的每一行，构造所有可能的两行组合
# 由于不能使用数组，我们将使用嵌套循环并再次读取文件
while IFS= read -r line1; do
    # 引入这个变量令文件内部循环从头开始
    while IFS= read -r line2; do
        # 仅当两行不同的时候打印组合，防止打印重复的行
        if [ "$line1" != "$line2" ]; then
            echo "processing $line1 - $line2 ..."
            sh extract.sh $line1 $line2
        fi
    done < "$file_path"
done < "$file_path"