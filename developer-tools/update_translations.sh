#!/bin/bash

#Load private config
source private.conf

#Generate non-ui translatables
#fixme: In future there are a few other files that belong here. 
python3.6 developer-tools/translations/extract_strings_qt.py src/init.cpp src/net.cpp src/wallet/wallet_init.cpp

#Cleanup
cd src/qt/locale/Gulden
rm *.po || true

#Fetch onesky translations
python ../../../../developer-tools/translations/fetch_translations.py ${ONESKY_API_KEY}

#Work around for dropped qt flag (needed for lconvert to behave in a  way that is not brain damaged)
sed -i 's|^\"Language\(.*\)$|"Language\1\n"X-Qt-Contexts: true\\n"|' *.po

#Now update ts files from onesky translations
for filename in *.po; do
    lconvert -locations relative $filename -o `dirname ${filename}`/`basename ${filename} .po`.ts
done

#Remerge updated ts files from source if source has changed (english only)
lupdate ../../../../src -locations relative -no-obsolete -ts gulden_en.ts

#Update OneSky
#fixme: This should deprecated phrases (currently have to do that by manually uploading)
lconvert -locations relative gulden_en.ts -o `dirname gulden_en.ts`/`basename gulden_en .ts`.po
python ../../../../developer-tools/translations/push_translations.py ${ONESKY_API_KEY} || true


sed -i 's|<extra-po-header-po_revision_date>.*</extra-po-header-po_revision_date>|<extra-po-header-po_revision_date>2018-08-29 15:43+0000</extra-po-header-po_revision_date>|' *.ts
sed -i 's|<extra-po-header-pot_creation_date>.*</extra-po-header-pot_creation_date>|<extra-po-header-pot_creation_date>2018-08-29 15:43+0000</extra-po-header-pot_creation_date>|' *.ts

#The below would do the same for other languages (if we needed it)
#lupdate ../../../../src -locations none -no-obsolete -ts `ls *.ts | grep -v src/qt/locale/Gulden/gulden_en.ts`
#Push updated translations back to onesky
#for filename in *.ts; do
#    lconvert -locations relative $filename -o `dirname ${filename}`/`basename ${filename} .ts`.po
#done
#lconvert -locations relative gulden_en.ts -o `dirname gulden_en.ts`/`basename gulden_en .ts`.po


#Cleanup
#rm *.po || true
