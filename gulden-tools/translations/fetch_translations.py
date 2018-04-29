import onesky.client
import shutil
import sys

client = onesky.client.Client(api_key='qr0T7bv7id0GXh73i1jDPerqUPeV3E4k', api_secret=sys.argv[1])

def fetch_file(locale_code, an_code):
    status, json_response = client.translation_export(234709, locale_code, 'gulden_en.po', 'gulden.po');
    if status != 200:
        print('error fetching translation')
        print(json_response)
        return
    shutil.move("gulden.po", 'gulden'  + an_code  + '.po')


lang_vector = [
('nl', '_nl'),
('en', '_en'),
]


for k, v in lang_vector:
	print(k)
	fetch_file(k, v);
