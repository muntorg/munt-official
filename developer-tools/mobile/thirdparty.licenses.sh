#! /bin/bash
set -e
set -x

OUTPUT=./src/data/core-packages.licenses

echo "" > $OUTPUT

echo "-- LevelDB" >> $OUTPUT
cat ./src/leveldb/LICENSE >> $OUTPUT

echo "-- QFontIcon" >> $OUTPUT
cat ./src/qt/qfonticon/License.md >> $OUTPUT

echo "-- QFontIcon" >> $OUTPUT
cat ./src/qt/qfonticon/License.md >> $OUTPUT

echo "-- scrypt" >> $OUTPUT
cat ./src/crypto/scrypt/COPYING >> $OUTPUT

echo "-- ctaes" >> $OUTPUT
cat ./src/crypto/ctaes/COPYING >> $OUTPUT

echo "-- libsecp256k1" >> $OUTPUT
cat ./src/secp256k1/COPYING >> $OUTPUT

echo "-- LRUCache11" >> $OUTPUT
cat ./src/LRUCache/COPYING >> $OUTPUT

echo "-- UniValue" >> $OUTPUT
cat ./src/univalue/COPYING >> $OUTPUT

echo "-- libqrencode" >> $OUTPUT
cat <<EOT >> $OUTPUT
Copyright (C) 2006-2018 Kentaro Fukuchi

This library is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation; either version 2.1 of the License, or any later version.

This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this library; if not, write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
EOT

echo "-- Bitcoin Core" >> $OUTPUT
cat <<EOT >> $OUTPUT
The MIT License (MIT)

Copyright (c) 2009-2019 The Bitcoin Core developers
Copyright (c) 2009-2019 Bitcoin Developers

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
EOT
