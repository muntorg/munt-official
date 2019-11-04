#! /bin/bash
set -e

source developer-tools/private.conf
python developer-tools/python/genbootstrap.py

echo "compress bootstrap"
xz -zkfe9T0 ${BOOTSTRAP_FILENAME}
echo "done compressing bootstrap"
