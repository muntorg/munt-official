#! /bin/bash
set -e

#Fetch our private configuration variables and then hand the rest of the work off to python script
source developer-tools/private.conf
HEIGHT=$(python developer-tools/python/genstaticdiffs.py)
sed -i 's/const int32_t nMaxHeight =.*/const int32_t nMaxHeight = '${HEIGHT}';/g' src/pow/diff_old.cpp
sed -ri 's/DIFF_SWITCHOVER[(]([^,]+).*/DIFF_SWITCHOVER(\1, '${HEIGHT}');/g' src/pow/diff_common.cpp
sed -i 's/ //g' ${STATICDIFF_FILENAME}
