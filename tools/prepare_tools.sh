#!/bin/bash

rm -rf /tmp/vraft_tools
mkdir -p /tmp/vraft_tools

cp analyze.sh /tmp/vraft_tools/
cp extract.sh /tmp/vraft_tools/
cp *.sh /tmp/vraft_tools/
cp *.js /tmp/vraft_tools/
cp *.css /tmp/vraft_tools/
cp -r ./html /tmp/vraft_tools/

cp ../output/main/meta-tool /tmp/vraft_tools/
cp ../output/main/rlog-tool /tmp/vraft_tools/
cp ../output/main/db-tool /tmp/vraft_tools/
