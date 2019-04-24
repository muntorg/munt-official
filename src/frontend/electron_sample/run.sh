#! /bin/bash
set -e
set -x

cp ../../../build_electron/src/.libs/libgulden_unity_node_js.so libgulden_unity_node_js.node
npm install
mkdir wallet | true
npm start
