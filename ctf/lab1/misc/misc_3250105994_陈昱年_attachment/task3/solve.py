import dpkt
import socket
import re
from urllib.parse import unquote

def extract_secret(pcap_path):
    with open(pcap_path, 'rb') as f:
        pcap = dpkt.pcap.Reader(f)
        connections = {}
        
        for ts, buf in pcap:
            eth = dpkt.ethernet.Ethernet(buf)
            ip = eth.data
            tcp = ip.data
            src = socket.inet_ntoa(ip.src)
            
            if src == '10.13.37.23' and len(tcp.data) > 0 and b'POST' in tcp.data:
                body = tcp.data.split(b'\r\n\r\n', 1)
                connections[tcp.sport] = {
                    'req_ts': ts,
                    'body': body[1].decode('utf-8', errors='replace').strip() if len(body) > 1 else '',
                    'resp_ts': None
                }
            elif src == '10.13.37.80' and len(tcp.data) > 0 and b'HTTP' in tcp.data:
                if tcp.dport in connections:
                    connections[tcp.dport]['resp_ts'] = ts
        
        # Extract character map
        char_map = {}
        for port, data in connections.items():
            if data['resp_ts'] is None:
                continue
            body = data['body']
            rtt = (data['resp_ts'] - data['req_ts']) * 1000
            
            if 'SUBSTRING' in body and 'SLEEP' in body:
                decoded = unquote(body)
                pos_match = re.search(r'SUBSTRING.*?(\d+),1', decoded)
                ascii_match = re.search(r'ASCII.*?\)\)>(\d+)', decoded)
                if pos_match and ascii_match:
                    pos = int(pos_match.group(1))
                    threshold = int(ascii_match.group(1))
                    is_true = rtt > 100
                    
                    if pos not in char_map:
                        char_map[pos] = {'true': [], 'false': []}
                    if is_true:
                        char_map[pos]['true'].append(threshold)
                    else:
                        char_map[pos]['false'].append(threshold)
        
        # Reconstruct
        result = ''
        for pos in sorted(char_map.keys()):
            true_set = set(char_map[pos]['true'])
            false_set = set(char_map[pos]['false'])
            if true_set and false_set:
                max_true = max(true_set)
                min_false = min(false_set)
                if max_true + 1 == min_false:
                    result += chr(min_false)
        
        return result

print(extract_secret('slow-login.pcap'))

