#!/bin/bash

dir="/tmp/remu_test_dir/"

rm -rf "${dir}/remu_web"
mkdir -p "${dir}/remu_web"

cp ./*.css "${dir}/remu_web"

cp "${dir}/log/remu.log" "${dir}/remu_web"
cp "${dir}/log/remu.log.global" "${dir}/remu_web"
#cp "${dir}/log/remu.log.sm" "${dir}/remu_web"
#cp ${dir}/log/keys.* "${dir}/remu_web" 2>/dev/null
cp ${dir}/log/*.sm "${dir}/remu_web" 2>/dev/null

node generate_global.js "${dir}/remu_web/remu.log.global" "${dir}/remu_web/global.html.body" 
cat ./html/global.html.head "${dir}/remu_web/global.html.body" ./html/global.html.tail > "${dir}/remu_web/global.html"
rm "${dir}/remu_web/global.html.body"

for sm in `ls ${dir}/log/*.sm`;do
    echo "processing ${sm} ..."
    node generate_node.js "${sm}" "${dir}/remu_web/node.html.body" 
    node generate_node2.js "${sm}" "${dir}/remu_web/node.html.body2"
    cat ./html/node.html.head "${dir}/remu_web/node.html.body" "${dir}/remu_web/node.html.body2" ./html/node.html.tail > ${sm}.html
    mv ${sm}.html "${dir}/remu_web"
    rm "${dir}/remu_web/node.html.body" "${dir}/remu_web/node.html.body2"
done

#node generate_node.js "${dir}/remu_web/remu.log.sm" "${dir}/remu_web/node.html.body" 
#node generate_node2.js "${dir}/remu_web/remu.log.sm" "${dir}/remu_web/node.html.body2"
#cat ./html/node.html.head "${dir}/remu_web/node.html.body" "${dir}/remu_web/node.html.body2" ./html/node.html.tail > "${dir}/remu_web/node.html"
#rm "${dir}/remu_web/node.html.body" "${dir}/remu_web/node.html.body2"

