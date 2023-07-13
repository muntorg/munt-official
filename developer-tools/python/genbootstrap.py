#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""Generate bootstrap.dat
   Script will connect to Munt rpc server and fetch all blocks necessary to create a clean bootstrap.dat file
   The results are incremental i.e. you can re-run this periodically (if you leave the files in place) and it will update the exiting file with only the latest blocks instead of completely regenerating it
   First run is slow (few hours) but subsequent runs should be reasonably fast as they have much less work to do
"""

from bitcoinrpc.authproxy import AuthServiceProxy
import sys
import string
import os
import binascii
import struct

__copyright__ = 'Copyright (c) 2019-2022 The Centure developers'
__license__   = 'Distributed under the GNU Lesser General Public License v3, see the accompanying file COPYING'
__author__    = 'Malcolm MacLeod'
__email__     = 'mmacleod@gmx.com'

# ===== BEGIN USER SETTINGS =====
rpcuser=os.environ["CHECKPOINT_RPC_USER"]
rpcpass=os.environ["CHECKPOINT_RPC_PASSWORD"]
rpcport=os.environ["CHECKPOINT_RPC_PORT"]
rpcip=os.environ["CHECKPOINT_RPC_IP"]
messageheader=os.environ["CHECKPOINT_MSG_HEADER"]
bootstrapfilename=os.environ["BOOTSTRAP_FILENAME"]

# ====== END USER SETTINGS ======

access = AuthServiceProxy("http://"+rpcuser+":"+rpcpass+"@"+rpcip+":"+rpcport)

bootstrap_start=0

try:
    bootstrap_file_last = open(bootstrapfilename+".pos", "r")
    bootstrap_start = int(bootstrap_file_last.read());
    bootstrap_file_last.close()
    print("pre-existing bootstrap, starting from", bootstrap_start)
    bootstrap_file = open(bootstrapfilename, "ba")
except IOError as e:
    print("no pre-existing bootstrap, starting from 0")
    bootstrap_file = open(bootstrapfilename, "wb")

def print_bootstrap(height):
    hash = access.getblockhash(height)
    block_hex = access.getblock(hash, 0)
    block_bin = binascii.unhexlify(block_hex)
    bootstrap_file.write(binascii.unhexlify(messageheader))
    bootstrap_file.write(struct.pack('i', len(block_bin)))
    bootstrap_file.write(block_bin)

# generate bootstrap from genesis->chaintip-100
chain_height = access.getblockcount()
last_bootstrap = chain_height - 100

h = bootstrap_start
while h < last_bootstrap:
    if (h % 1000 == 0):
        print("writing block ", h)
    print_bootstrap(h)
    h = h + 1


bootstrap_file.close()
bootstrap_file_last = open(bootstrapfilename+".pos", "w")
bootstrap_file_last.write(str(last_bootstrap));
bootstrap_file_last.close()

print("done writing bootstrap, final height: ", last_bootstrap)

