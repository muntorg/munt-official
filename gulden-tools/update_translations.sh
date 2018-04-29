#!/bin/bash

#Load private config
source private.conf

#Cleanup
cd src/qt/locale/Gulden
rm *.po || true

#Fetch onesky translations
python ../../../../gulden-tools/translations/fetch_translations.py ${ONESKY_API_KEY}

#Update ts files from onesky translations
for filename in *.po; do
    lconvert -locations relative $filename -o `dirname ${filename}`/`basename ${filename} .po`.ts
done

#Update ts files form source if source has changed
lupdate ../../../../src -locations relative -no-obsolete -ts gulden_en.ts
lupdate ../../../../src -locations none -no-obsolete -ts `ls *.ts | grep -v src/qt/locale/Gulden/gulden_en.ts`

#Push updated translations back to onesky
for filename in *.ts; do
    lconvert -locations relative $filename -o `dirname ${filename}`/`basename ${filename} .ts`.po
done
python ../../../../gulden-tools/translations/push_translations.py ${ONESKY_API_KEY}


