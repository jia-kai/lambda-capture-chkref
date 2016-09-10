#!/bin/bash

# cmake seems to pass CMAKE_CXX_FLAGS also to the linker, so we have to check
# whether clang++ is used for compile before adding -Xclang options.

function chk_arg {
    # idiomatic parameter and option handling in sh
    while test $# -gt 0
    do
        if [ "$1" = '-c' ]; then
            echo -n " -Xclang -load -Xclang"
            echo -n " $(readlink -f $(dirname "${BASH_SOURCE[0]}")/chkref.so)"
            echo " -Xclang -plugin -Xclang chkref"
            return
        fi
        shift
    done
}

args=$(chk_arg "$@")
[ -n "$args" ] && clang++ "$@" $args

# seems that clang would not write output file if plugin is usedd
exec clang++ "$@"
