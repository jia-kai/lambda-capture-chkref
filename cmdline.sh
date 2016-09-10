#!/bin/bash

make -C $(dirname $0) > /dev/null

echo "-Xclang -load -Xclang $(readlink -f $(dirname $0))/chkref.so -Xclang -plugin -Xclang chkref"
