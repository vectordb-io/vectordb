#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 file_name"
    exit 1
fi

file_name="$1"
echo ${file_name}

cp tpl_vectordb_class.h ${file_name}.h
cp tpl_vectordb_class.cc ${file_name}.cc


# 使用awk将字符串按照下划线分割，然后将每个单词的首字母大写
class_name=$(echo $file_name | awk 'BEGIN{FS=OFS="_"} {for(i=1; i<=NF; i++) $i=toupper(substr($i, 1, 1)) substr($i, 2)} 1' | tr -d '_')
ifdef_name=$(echo "$file_name" | tr '[:lower:]' '[:upper:]')

echo "create Class ${class_name}"
echo "create Class ${ifdef_name}"

sed -i 's/tpl/'"${file_name}"'/g' ${file_name}.h ${file_name}.cc
sed -i 's/Tpl/'"${class_name}"'/g' ${file_name}.h ${file_name}.cc
sed -i 's/TPL/'"${ifdef_name}"'/g' ${file_name}.h ${file_name}.cc

