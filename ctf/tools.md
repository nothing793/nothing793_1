# 本机 CTF 工具

本机可用的 CTF 工具，以及**可自动化调用**的方式。图形界面类工具需人工操作，
脚本只能驱动其命令行版本。

## 可在 WSL 命令行直接调用

- **wscat 5.1.0**：连接 WebSocket 服务，用于容器题/Web 题。
  - 系统自带的 `/usr/bin/wscat` 因依赖布局问题无法直接运行
    （报 `Cannot find module 'https-proxy-agent'`）。
  - 已在 `~/.local/bin/wscat` 放置包装脚本，为 wscat 进程单独设置
    `NODE_PATH=/usr/share/nodejs`，直接执行 `wscat --connect <url>` 即可。
- **ExifTool 12.40**：`/usr/bin/exiftool`，查看和处理图片等文件的元数据。
  - 由系统包 `libimage-exiftool-perl` 提供，直接 `exiftool <文件>` 即可。
  - Windows 侧另有一套 13.59（见下），一般用 WSL 原生版更方便。

## Windows 侧工具（经 /mnt 路径 + WSL 互操作调用）

- **IDA Professional 9.2**：`/mnt/c/Program Files/IDA Professional 9.2/`
  - `idat.exe`：文本/批处理模式，配合 `-A -S script.py` 可做无人值守分析。
  - `ida.exe`：图形界面，需人工操作。
  - 本机只安装了 IDA Professional，未安装 IDA Teams。
- **Wireshark 4.6.8**：`/mnt/d/Program Files/Wireshark/`
  - `tshark.exe`：命令行协议分析，已验证可用。
  - `dumpcap.exe`：实时抓包，需要管理员权限与 Npcap/WinPcap 驱动。
  - `Wireshark.exe`：图形界面，需人工操作。
- **ExifTool 13.59**：`/mnt/d/exiftool-13.59_64/exiftool-13.59_64/exiftool(-k).exe`
  - 可执行文件名带 `(-k)`，运行结束会多输出一行 `-- press ENTER --`；
    重定向标准输入（`... exiftool\(-k\).exe </dev/null`）即可正常使用。

## 说明

- 原工作区中的 `.lnk` 文件是 Windows 快捷方式，ExifTool 同时保留了可执行文件。
- 上述 Windows 工具的图形界面无法由脚本驱动，涉及交互操作时需人工完成。
