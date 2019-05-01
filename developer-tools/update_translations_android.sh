#!/bin/bash

#Load private config
source developer-tools/private.conf

python developer-tools/translations/fetch_translations_android.py ${ONESKY_API_KEY}

