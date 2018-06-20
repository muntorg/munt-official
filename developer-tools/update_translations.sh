#!/bin/bash

#Load private config
source private.conf

#Cleanup
cd src/qt/locale/Gulden
rm *.po || true

#Fetch onesky translations
python ../../../../developer-tools/translations/fetch_translations.py ${ONESKY_API_KEY}

#Update ts files from onesky translations
for filename in *.po; do
    lconvert -locations relative $filename -o `dirname ${filename}`/`basename ${filename} .po`.ts
done

#Update ts files from source if source has changed
lupdate ../../../../src -locations relative -no-obsolete -ts gulden_en.ts
lupdate ../../../../src -locations none -no-obsolete -ts `ls *.ts | grep -v src/qt/locale/Gulden/gulden_en.ts`

#Push updated translations back to onesky
#for filename in *.ts; do
#    lconvert -locations relative $filename -o `dirname ${filename}`/`basename ${filename} .ts`.po
#done
lconvert -locations relative gulden_en.ts -o `dirname gulden_en.ts`/`basename gulden_en .ts`.po
python ../../../../developer-tools/translations/push_translations.py ${ONESKY_API_KEY} || true

#Cleanup
rm *.po || true
