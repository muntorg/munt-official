import onesky.client
import shutil
import sys

client = onesky.client.Client(api_key='qr0T7bv7id0GXh73i1jDPerqUPeV3E4k', api_secret=sys.argv[1])

status, json_response = client.file_upload(234709, 'gulden_en.po', 'GNU_PO');
if status != 200:
    print('error pushing translations')
    print(json_response)
