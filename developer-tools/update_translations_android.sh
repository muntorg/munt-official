#!/bin/bash

#Load private config
source private.conf

python developer-tools/translations/fetch_translations_android.py ${ONESKY_API_KEY}

