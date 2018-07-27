#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""Generate C++ code for the compiled-in checkpoints.
   Script will connect to Gulden rpc server.
   Generated code goes into chainparams.cpp
"""

from jsonrpc import ServiceProxy
import sys
import string

__copyright__ = 'Copyright (c) 2018 The Gulden developers'
__license__   = 'Distributed under the GULDEN software license, see the accompanying file COPYING'
__author__    = 'Willem de Jonge'
__email__     = 'willem@isnapp.nl'

# ===== BEGIN USER SETTINGS =====
rpcuser='<your-rpc-user>'
rpcpass='<your-rpc-password>'
# ====== END USER SETTINGS ======

# code format example:
# {  50000, { uint256S("0x5baeb5a5c3d5fefbb094623e85e3e16a1ea47875b5ffd1ff5a200e639908a059"), 1400560264 } },

access = ServiceProxy("http://"+rpcuser+":"+rpcpass+"@127.0.0.1:9232")


chainheight = access.getblockcount()

h = 0
while  h < chainheight - 2 * 576:
    hash = access.getblockhash(h)
    block = access.getblock(hash)
    print('    {{ {height:6}, {{ uint256S(\"0x{hash}\"), {time} }} }},'.format(height=block['height'], hash=block['hash'], time=block['time'], bits=block['bits']))

    h = h + 25000
