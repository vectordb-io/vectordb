#!/bin/bash

dir="/tmp/remu_test_dir/"

for d in `find ${dir} -name meta`; do
    echo -n "${d} : "
    /tmp/vraft_tools/meta_tool "${d}"
done