'use strict';

// Exploit for Killerecc CTF challenge
// Uses WebSocket proxy to connect, then exploits elliptic@6.6.0 malformed input bug
// Signing "hex" and "-hex" produces same k (nonce reuse) -> recover private key

const WebSocket = require('ws');
const { ec: EC } = require('elliptic');

const WS_URL = 'wss://ctf.zjusec.net/api/proxy/019f7a38-cce1-7228-892e-7df501a29433';
const MSG_HEX = 'aaaa';  // short hex triggers same-k with '-aaaa'

class WSClient {
    constructor(url) {
        this.url = url;
        this.ws = null;
        this.buf = '';
        this._msgResolve = null;
    }

    connect() {
        return new Promise((resolve, reject) => {
            this.ws = new WebSocket(this.url);
            this.ws.on('open', resolve);
            this.ws.on('error', reject);
            this.ws.on('message', (data) => {
                this.buf += data.toString();
                // Check if we have a pending resolver
                if (this._msgResolve && this.buf.includes('> ')) {
                    const r = this._msgResolve;
                    this._msgResolve = null;
                    const result = this.buf;
                    this.buf = '';
                    r(result);
                }
            });
            this.ws.on('close', () => {
                if (this._msgResolve) {
                    this._msgResolve(null);
                }
            });
        });
    }

    async send(cmd) {
        return new Promise((resolve, reject) => {
            if (this._msgResolve) {
                reject(new Error('Already waiting for response'));
                return;
            }
            this._msgResolve = resolve;
            this.buf = '';
            this.ws.send(cmd + '\n');
            // Safety timeout
            setTimeout(() => {
                if (this._msgResolve) {
                    const r = this._msgResolve;
                    this._msgResolve = null;
                    r(this.buf);
                }
            }, 15000);
        });
    }

    close() {
        this.ws.close();
    }
}

async function main() {
    console.log('[*] Connecting via WebSocket proxy...');
    const client = new WSClient(WS_URL);
    await client.connect();
    console.log('[+] Connected!');

    // Read welcome
    const welcome = await client.send('');
    console.log('[<] Welcome message received');

    // Extract public key (for reference)
    const pubXMatch = welcome.match(/x = (\d+)/);
    const pubYMatch = welcome.match(/y = (\d+)/);
    if (pubXMatch) {
        console.log(`[+] Public key X: ${pubXMatch[1].substring(0, 30)}...`);
        console.log(`[+] Public key Y: ${pubYMatch[1].substring(0, 30)}...`);
    }

    // Get first signature
    console.log(`[*] Requesting sign ${MSG_HEX}...`);
    const resp1 = await client.send(`sign ${MSG_HEX}`);
    console.log('[<]', resp1.replace(/\n/g, ' | ').trim());

    const r1Match = resp1.match(/r = (\d+)/);
    const s1Match = resp1.match(/s = (\d+)/);
    if (!r1Match || !s1Match) throw new Error('Failed to parse first signature');
    const r1 = r1Match[1];
    const s1 = s1Match[1];

    // Get second signature (negative hex)
    console.log(`[*] Requesting sign -${MSG_HEX}...`);
    const resp2 = await client.send(`sign -${MSG_HEX}`);
    console.log('[<]', resp2.replace(/\n/g, ' | ').trim());

    const r2Match = resp2.match(/r = (\d+)/);
    const s2Match = resp2.match(/s = (\d+)/);
    if (!r2Match || !s2Match) throw new Error('Failed to parse second signature');
    const r2 = r2Match[1];
    const s2 = s2Match[1];

    console.log('');
    console.log(`[+] sig1 r = ${r1}`);
    console.log(`[+] sig2 r = ${r2}`);
    console.log(`[+] r match: ${r1 === r2}`);

    if (r1 !== r2) {
        console.log('[-] r values differ — trying alternative approach...');
        // Try with a different short hex
        client.close();
        process.exit(1);
    }

    // Compute private key
    const BN = require('bn.js');
    const ec = new EC('secp256k1');
    const n = ec.n;

    const h1 = ec._truncateToN(MSG_HEX, false);
    const h2 = ec._truncateToN('-' + MSG_HEX, false);

    const rBN = new BN(r1, 10);
    const s1BN = new BN(s1, 10);
    const s2BN = new BN(s2, 10);

    // k = (h1 - h2) / (s1 - s2) mod n
    const hDiff = h1.sub(h2).umod(n);
    const sDiff = s1BN.sub(s2BN).umod(n);
    const k = hDiff.mul(sDiff.invm(n)).umod(n);

    // Verify k
    const kp = ec.g.mul(k);
    const rCheck = kp.getX().umod(n);
    console.log(`[+] k verified: ${rCheck.toString() === r1 ? '✓' : '✗'}`);

    // d = (k*s1 - h1) * r^-1 mod n
    const d = k.mul(s1BN).sub(h1).mul(rBN.invm(n)).umod(n);

    const keyHex = d.toString(16);
    console.log(`[+] Recovered private key: ${keyHex}`);

    // Submit the key
    console.log('[*] Submitting key...');
    const submitResp = await client.send(`submit ${keyHex}`);
    console.log('[<]', submitResp.replace(/\n/g, ' | ').trim());

    // Check if we got the flag
    if (submitResp.includes('ZJUCTF') || submitResp.includes('flag{')) {
        const flagMatch = submitResp.match(/Flag: ([^\n]+)/);
        if (flagMatch) {
            console.log(`\n🚩🚩🚩 FLAG: ${flagMatch[1]} 🚩🚩🚩`);
        } else {
            console.log('\n🚩 Flag found in response!');
            console.log(submitResp);
        }
    }

    client.close();
    console.log('[+] Done!');
}

main().catch(e => {
    console.error('[-] Error:', e.message);
    console.error(e.stack);
    process.exit(1);
});
