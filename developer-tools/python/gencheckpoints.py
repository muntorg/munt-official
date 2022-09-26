#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""Generate C++ code for the compiled-in checkpoints.
   Script will connect to Munt rpc server.
   Generated code goes into chainparams.cpp
"""

from bitcoinrpc.authproxy import AuthServiceProxy
import sys
import string
import os

__copyright__ = 'Copyright (c) 2018-2022 The Centure developers'
__license__   = 'Distributed under the Libre Chain License, see the accompanying file COPYING'
__author__    = 'Willem de Jonge'
__email__     = 'willem@isnapp.nl'

# ===== BEGIN USER SETTINGS =====
rpcuser=os.environ["CHECKPOINT_RPC_USER"]
rpcpass=os.environ["CHECKPOINT_RPC_PASSWORD"]
rpcport=os.environ["CHECKPOINT_RPC_PORT"]
rpcip=os.environ["CHECKPOINT_RPC_IP"]


# ====== END USER SETTINGS ======

# code format example:
# {  50000, { uint256S("0x5baeb5a5c3d5fefbb094623e85e3e16a1ea47875b5ffd1ff5a200e639908a059"), 1400560264 } },

access = AuthServiceProxy("http://"+rpcuser+":"+rpcpass+"@"+rpcip+":"+rpcport)

def print_checkpoint(height):
    hash = access.getblockhash(height)
    block = access.getblock(hash)
    print('    {{ {height:6}, {{ uint256S(\"0x{hash}\"), {time} }} }},'.format(height=block['height'], hash=block['hash'], time=block['time'], bits=block['bits']))

# checkpoints are generated at regular intervals, in addition 2 extra checkpoints are generated
# close to the tip this is a performance optimization which is most notable right after a release with new checkpoints
# the extra before last checkpoint helps initial partial sync, which needs 2 checkpoints for secure initialization
chain_height = access.getblockcount()
last_checkpoint = chain_height - 1 * 576
extra_before_last = last_checkpoint - 2 * 576
checkpoint_period = 5000

h = 0
while  h < extra_before_last:
    print_checkpoint(h)
    h = h + checkpoint_period

print_checkpoint(extra_before_last)
print_checkpoint(last_checkpoint)
