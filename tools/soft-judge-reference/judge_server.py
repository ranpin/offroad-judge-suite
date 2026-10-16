#!/usr/bin/env python3
"""协议级参考实现：比赛服务器 + 裁判下发端（纯线协议，无第三方依赖）。

用途：在**没有专有裁判客户端/考题包**的情况下，按 `docs/communication-protocol.md`
描述的线协议驱动参赛端（MockClient / 兼容实现）完成一次可复现的闭环：
    START(0x02 0x30 0x03) -> BEGIN(0x02 0x31 0x03) -> 按时间戳下发 token
    -> 接收并解码参赛端上传的二进制答案帧（全字段），验证 START->token->答案 全链路贯通。

角色与端口（参赛端为 Server，本脚本为 Client；就绪/答案通道本脚本为 Server）：
    7001  指令(本脚本->参赛端, 定长3B)     7002  token(本脚本->参赛端, 定长12B)
    7000  就绪/结束(参赛端->本脚本)         7003  答案(参赛端->本脚本)

token.csv 需含表头 `timestamp,type,token`。示例：
    python3 judge_server.py --token-csv ./token.csv
"""
import argparse
import socket
import struct
import threading
import time

STX, ETX = 0x02, 0x03


def cmd_frame(mid: int) -> bytes:
    """7001 指令帧：0x02 <动作字节> 0x03。0x30=开始准备,0x31=开始,0x35=结束。"""
    return bytes([STX, mid, ETX])


def token_frame(ts: int, typ: str, key: int) -> bytes:
    """7002 token 帧：0x02 + 8B 大端 timestamp + type(1B) + key(1B) + 0x03。"""
    return bytes([STX]) + struct.pack(">q", ts) + bytes([ord(typ[0]), key & 0xFF]) + bytes([ETX])


def parse_answer_stream(buf: bytes):
    """从 TCP 流按 0x02|len(4B大端) 切分答案帧；len=95+imgLen，整帧=len+6。"""
    out, i = [], 0
    while True:
        j = buf.find(bytes([STX]), i)
        if j < 0:
            return out, buf[i:]
        if len(buf) - j < 5:
            return out, buf[j:]
        ln = int.from_bytes(buf[j + 1:j + 5], "big")
        total = ln + 6
        if total <= 5 or len(buf) - j < total:
            return out, buf[j:]
        fr = buf[j:j + total]
        i = j + total
        if len(fr) >= 100 and fr[5] == 0x4F:  # 'O'
            box = [struct.unpack(">d", fr[15 + k * 8:23 + k * 8])[0] for k in range(8)]
            out.append({
                "timestamp": int.from_bytes(fr[6:14], "big"),
                "box_type": fr[14],
                "box": box,
                "attribute": fr[79],
                "lon": struct.unpack(">d", fr[80:88])[0],
                "lat": struct.unpack(">d", fr[88:96])[0],
                "cols": int.from_bytes(fr[96:98], "big"),
                "rows": int.from_bytes(fr[98:100], "big"),
                "size": total,
            })
    return out, buf[i:]


class Loop:
    def __init__(self, host, ports, token_csv, target, quiet):
        self.host = host
        self.P = ports
        self.token_csv = token_csv
        self.target = target
        self.quiet = quiet
        self.answers = []
        self.ready = []
        self.lock = threading.Lock()
        self.stop = threading.Event()

    def log(self, *a):
        if not self.quiet:
            print(*a, flush=True)

    def _serve(self, port, handler):
        srv = socket.socket()
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind((self.host, port))
        srv.listen(4)
        srv.settimeout(0.5)
        self.log(f"[judge] listening :{port}")
        while not self.stop.is_set():
            try:
                c, _ = srv.accept()
            except socket.timeout:
                continue
            threading.Thread(target=lambda: handler(c), daemon=True).start()

    def _on_ready(self, c):
        c.settimeout(60)
        try:
            while True:
                d = c.recv(64)
                if not d:
                    break
                with self.lock:
                    self.ready.append(d)
                self.log(f"[{self.P['ready']}] 参赛端状态帧: {d.hex()}")
        except OSError:
            pass

    def _on_answer(self, c):
        c.settimeout(60)
        acc = b""
        try:
            while True:
                d = c.recv(65536)
                if not d:
                    break
                acc += d
                frames, acc = parse_answer_stream(acc)
                if frames:
                    with self.lock:
                        self.answers.extend(frames)
        except OSError:
            pass

    def _connect(self, port):
        for _ in range(240):
            try:
                return socket.create_connection((self.host, port), timeout=2)
            except OSError:
                time.sleep(0.5)
        raise SystemExit(f"无法连接参赛端 {self.host}:{port}（请先启动参赛端）")

    def run(self):
        threading.Thread(target=lambda: self._serve(self.P["ready"], self._on_ready), daemon=True).start()
        threading.Thread(target=lambda: self._serve(self.P["answer"], self._on_answer), daemon=True).start()
        time.sleep(0.5)
        s1 = self._connect(self.P["cmd"])
        s2 = self._connect(self.P["token"])
        self.log("[judge] 已连接参赛端 cmd/token 通道")

        s1.sendall(cmd_frame(0x30)); self.log("[cmd] -> 02 30 03 (开始准备/START)"); time.sleep(1)
        s1.sendall(cmd_frame(0x31)); self.log("[cmd] -> 02 31 03 (开始/BEGIN)"); time.sleep(0.2)

        n = 0
        t0 = time.time()
        with open(self.token_csv) as f:
            next(f, None)  # 跳过表头
            for line in f:
                if self.stop.is_set() or time.time() - t0 > 180:
                    break
                with self.lock:
                    if len(self.answers) >= self.target:
                        break
                p = line.strip().split(",")
                if len(p) < 3:
                    continue
                try:
                    fr = token_frame(int(p[0]), p[1], int(p[2]))
                except ValueError:
                    continue
                s2.sendall(fr)
                n += 1
        self.log(f"[judge] token 发送结束，共 {n} 条")

        for _ in range(20):
            with self.lock:
                if len(self.answers) >= self.target:
                    break
            time.sleep(0.5)
        self.stop.set()

        with self.lock:
            got = list(self.answers)
        print("\n================ 闭环结果 ================", flush=True)
        print(f"就绪帧(7000): {len(self.ready)}  {self.ready[:4] and [x.hex() for x in self.ready[:4]]}")
        print(f"答案帧(7003): {len(got)}")
        for a in got[:5]:
            print("   ", a)
        ts = [a["timestamp"] for a in got]
        if ts:
            print(f"答案 timestamp 单调递增: {all(ts[i] <= ts[i+1] for i in range(len(ts)-1))}  跨度 {ts[0]}..{ts[-1]}")
        print("判定:", "闭环贯通（START→token→答案 全链路）" if len(got) >= 1 else "未收到答案")


def main():
    ap = argparse.ArgumentParser(description="协议级比赛服务器/裁判下发参考实现")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--cmd-port", type=int, default=7001, help="参赛端指令监听端口(本脚本连接)")
    ap.add_argument("--token-port", type=int, default=7002, help="参赛端 token 监听端口(本脚本连接)")
    ap.add_argument("--ready-port", type=int, default=7000, help="本脚本监听: 参赛端就绪/结束")
    ap.add_argument("--answer-port", type=int, default=7003, help="本脚本监听: 参赛端答案上传")
    ap.add_argument("--token-csv", required=True, help="含表头 timestamp,type,token")
    ap.add_argument("--target-answers", type=int, default=300)
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()
    ports = {"cmd": a.cmd_port, "token": a.token_port, "ready": a.ready_port, "answer": a.answer_port}
    Loop(a.host, ports, a.token_csv, a.target_answers, a.quiet).run()


if __name__ == "__main__":
    main()
