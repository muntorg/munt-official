# Copyright (c) 2013 The Gulden Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from distutils.core import setup
setup(name='btcspendfrom',
      version='1.0',
      description='Command-line utility for gulden "coin control"',
      author='Gavin Andresen',
      author_email='gavin@guldenfoundation.org',
      requires=['jsonrpc'],
      scripts=['spendfrom.py'],
      )
