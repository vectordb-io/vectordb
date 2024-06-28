#!/bin/bash

for file in `ls *_test`; do
    echo "Running $file..."
    ./"$file" 
done