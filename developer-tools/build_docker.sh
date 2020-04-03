#! /bin/bash
set -e

#Set up docker container
cd developer-tools/docker-build
docker build -t nlg-build .

#Run the build
cd ../../
docker run -it --mount type=bind,src=$PWD,dst=/work -w /work nlg-build

