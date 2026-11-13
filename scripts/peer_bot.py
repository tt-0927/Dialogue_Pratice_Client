#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""无界面测试客户端（peer bot）。

用来在没有第二个 GUI 的情况下验证聊天链路：登录 → 进房 → 发消息 → 打印收到的消息。
配合客户端一起跑，就能看到「对方」的消息气泡。

用法:
    python3 scripts/peer_bot.py --name bob --room 1001 --say "你好，我是 bob"
"""
import argparse
import json
import socket
import struct
import sys
import threading
import time

OP_LOGIN = 0x01
OP_JOIN = 0x02
OP_CHAT = 0x04
OP_VOICE = 0x09

NAMES = {
    0x81: "LoginResp", 0x82: "RoomState", 0x83: "ChatPush",
    0x84: "SystemMsg", 0x85: "ErrorMsg", 0x86: "Pong", 0x87: "RoomList",
    0x88: "ProfileResp",
}


def send(sock, op, obj):
    payload = json.dumps(obj, ensure_ascii=False).encode("utf-8")
    sock.sendall(struct.pack(">I", len(payload) + 1) + bytes([op]) + payload)


def reader(sock, stop):
    buf = b""
    while not stop.is_set():
        try:
            chunk = sock.recv(65536)
        except OSError:
            return
        if not chunk:
            return
        buf += chunk
        while len(buf) >= 5:
            ln = struct.unpack(">I", buf[:4])[0]
            if len(buf) < 4 + ln:
                break
            op = buf[4]
            body = buf[5:4 + ln]
            buf = buf[4 + ln:]
            data = json.loads(body.decode("utf-8")) if body else {}
            if op == 0x83:
                print(f"[{data.get('name')}] {data.get('text')}", flush=True)
            elif op in (0x84, 0x85):
                print(f"  <{NAMES.get(op, hex(op))}> {data.get('text')}", flush=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=9000)
    ap.add_argument("--name", default="bob")
    ap.add_argument("--room", default="1001")
    ap.add_argument("--say", action="append", default=[])
    ap.add_argument("--voice-b64", default="",
                    help="语音消息：文件内容为 voice_make 输出的一行 '<durationMs> <base64>'")
    ap.add_argument("--listen", type=float, default=0.0,
                    help="发送后继续监听多少秒（0 表示发完即退出）")
    args = ap.parse_args()

    sock = socket.create_connection((args.host, args.port), timeout=5)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    stop = threading.Event()
    threading.Thread(target=reader, args=(sock, stop), daemon=True).start()

    send(sock, OP_LOGIN, {"name": args.name})
    time.sleep(0.2)
    send(sock, OP_JOIN, {"room": args.room})
    time.sleep(0.3)

    for text in (args.say or [f"你好，我是 {args.name}"]):
        send(sock, OP_CHAT, {"text": text})

    if args.voice_b64:
        with open(args.voice_b64, "r", encoding="utf-8") as fh:
            line = fh.read().strip()
        duration_str, b64 = line.split(" ", 1)
        send(sock, OP_VOICE, {"durationMs": int(duration_str), "opus": b64})
        print(f"[voice] 已发送语音 {int(duration_str)}ms / {len(b64)} base64 字符")
        time.sleep(0.35)

    if args.listen > 0:
        time.sleep(args.listen)

    stop.set()
    sock.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
