#! /bin/bash
set -e
set -x

npm install
mkdir wallet | true
npm start
