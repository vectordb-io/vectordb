#!/bin/bash

dir="/tmp/remu_test_dir/"

for d in `find ${dir} -name rlog`; do
    echo -n "${d} : "
    /tmp/vraft_tools/rlog_tool "${d}"
done