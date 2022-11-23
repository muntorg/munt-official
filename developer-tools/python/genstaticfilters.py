#!/usr/bin/env python
# -*- coding: utf-8 -*-

from bitcoinrpc.authproxy import AuthServiceProxy
import sys
import string
import os

__copyright__ = 'Copyright (c) 2022 The Gulden developers'
__license__   = 'Distributed under the GULDEN software license, see the accompanying file COPYING'
__author__    = 'Malcolm MacLeod'
__email__     = 'mmacleod@gmx.com'

# ===== BEGIN USER SETTINGS =====
rpcuser=os.environ["CHECKPOINT_RPC_USER"]
rpcpass=os.environ["CHECKPOINT_RPC_PASSWORD"]
rpcport=os.environ["CHECKPOINT_RPC_PORT"]
rpcip=os.environ["CHECKPOINT_RPC_IP"]
filename=os.environ["STATICFILTER_FILENAME"]
# ====== END USER SETTINGS ======

access = AuthServiceProxy("http://"+rpcuser+":"+rpcpass+"@"+rpcip+":"+rpcport)

if os.path.exists(filename):
    os.remove(filename)
access.dumpfiltercheckpoints(filename)
