#! /bin/bash
set -e

#Connect to a Gulden rpc server and fetch all blocks necessary to create a clean bootstrap.dat file
# The results are incremental i.e. you can re-run this periodically (if you leave the files in place) and it will update the exiting file with only the latest blocks instead of completely regenerating it
# First run is slow (few hours) but subsequent runs should be reasonably fast as they have much less work to do

#Fetch our private configuration variables and then hand the rest of the work off to python script
source developer-tools/private.conf
python developer-tools/python/gencheckpoints.py > src/data/chainparams_mainnet_static_checkpoint_data.cpp
