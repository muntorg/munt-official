#! /bin/bash
set -e
set -x

if test -f "../../../build_node/src/.libs/lib_unity_node_js-0.dll"; then
    cp ../../../build_node/src/.libs/lib_unity_node_js-0.dll lib_unity.node
elif test -f "../../../build_node/src/.libs/lib_unity_node_js.so"; then
    cp ../../../build_node/src/.libs/lib_unity_node_js.so lib_unity.node
fi

npm install
mkdir wallet | true
npm start
