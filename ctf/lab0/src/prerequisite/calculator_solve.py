import socket
import re

HOST = '10.214.160.13'
PORT = 11002

def solve():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(15)
    s.connect((HOST, PORT))

    data = b''
    s.settimeout(1)
    while True:
        try:
            chunk = s.recv(4096)
            if not chunk:
                break
            data += chunk
        except socket.timeout:
            break

    text = data.decode(errors='ignore')

    for i in range(10):
        match = re.search(r'(\d[\d\s\+\-\*]+\d)\s*=\s*', text)
        if not match:
            break
        expr = match.group(1).strip()
        result = eval(expr)
        s.sendall((str(result) + '\n').encode())

        data = b''
        s.settimeout(5)
        while True:
            try:
                chunk = s.recv(4096)
                if not chunk:
                    break
                data += chunk
                text = data.decode(errors='ignore')
                if '=' in text or 'flag' in text.lower():
                    break
            except socket.timeout:
                break

    s.settimeout(3)
    try:
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            data += chunk
    except:
        pass

    print(data.decode(errors='ignore'))
    s.close()

if __name__ == '__main__':
    solve()
