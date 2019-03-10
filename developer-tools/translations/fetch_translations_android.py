import onesky.client
import shutil
import os
import sys

client = onesky.client.Client(api_key='qr0T7bv7id0GXh73i1jDPerqUPeV3E4k', api_secret=sys.argv[1])

def fetch_file(locale_code, an_code):
    status, json_response = client.translation_export(235159, locale_code, 'strings.xml', 'strings.xml');
    if status != 200:
        print('error fetching translation')
        print(json_response)
        return
    if not os.path.exists('src/frontend/android/unity_wallet/app/src/main/res/values'  + an_code):
        os.mkdir('src/frontend/android/unity_wallet/app/src/main/res/values'  + an_code)
    shutil.move("strings.xml", 'src/frontend/android/unity_wallet/app/src/main/res/values'  + an_code  + '/strings.xml')


lang_vector = [
('fr', '-fr'),
('de', '-de'),
('ca', '-ca'),
('es', '-es'),
('ko', '-ko'),
('nl', '-nl'),
('ja', '-ja'),
('pt-PT', '-pt'),
('pt-BR', '-pt-rBR'),
('it', '-it'),
('fi', '-fi'),
('no', '-no'),
('pl', '-pl'),
('ru', '-ru'),
('vi', '-vi'),
('zh-CN', '-zh-rCN'),
('zh-TW', '-zh-rTW'),
('en-US', '-en-rUS'),
('en-ZA', '-en-rZA'),
('en-CA', '-en-rCA'),
('en-AU', '-en-rAU'),
('en-GB', '-en-rGB'),
]


for k, v in lang_vector:
	print(k)
	fetch_file(k, v);
