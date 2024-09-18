#!/bin/bash

#sudo apt install valgrind -y

if [ $# -eq 0 ]; then
  echo "$0 program"
  exit 1
else
  args=""
  for arg in "$@"
  do
    args="$args $arg"
  done
  valgrind --tool=memcheck --leak-check=yes --leak-check=full --show-leak-kinds=all $args
fi
