#! /bin/bash
set -e
set -x

if test -f "../../../build_electron/src/.libs/libgulden_unity_node_js-0.dll"; then
    cp ../../../build_electron/src/.libs/libunity_node_js-0.dll libflorin_unity_node_js.node
elif test -f "../../../build_electron/src/.libs/libgulden_unity_node_js.so"; then
    cp ../../../build_electron/src/.libs/libunity_node_js.so libflorin_unity_node_js.node
fi

npm install
mkdir wallet | true
npm start
