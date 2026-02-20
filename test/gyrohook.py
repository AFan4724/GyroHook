"""
GyroHook Python Client
对应 C++ GyroHook 的 Python 版本，支持 socket 和 file 两种模式。
"""

import socket
import struct
import time
import math
import xml.etree.ElementTree as ET
from pathlib import Path

DEFAULT_IP = "127.0.0.1"
DEFAULT_PORT = 16384
DEFAULT_FILE_PATH = "/data/user/0/com.example.gyrohook/shared_prefs/gyro_settings.xml"


class GyroClient:
    """Socket 客户端，连接到 GyroHook Android 应用的 Socket 服务器。"""

    def __init__(self, ip: str = DEFAULT_IP, port: int = DEFAULT_PORT):
        self.ip = ip
        self.port = port
        self._sock: socket.socket | None = None

    def connect(self) -> bool:
        try:
            self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self._sock.connect((self.ip, self.port))
            print(f"GyroClient: 已连接到服务器 {self.ip}:{self.port}")
            return True
        except OSError as e:
            print(f"GyroClient: 连接失败 {self.ip}:{self.port} - {e}")
            self._sock = None
            return False

    def disconnect(self):
        if self._sock:
            self._sock.close()
            self._sock = None
            print("GyroClient: 已断开连接")

    def is_connected(self) -> bool:
        return self._sock is not None

    def send_gyro_data(self, x: float, y: float, z: float) -> bool:
        if not self._sock:
            print("GyroClient: 未连接到服务器")
            return False
        try:
            message = f"{x},{y},{z}\n"
            self._sock.sendall(message.encode())
            return True
        except OSError as e:
            print(f"GyroClient: 发送数据失败 - {e}")
            return False

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, *_):
        self.disconnect()


class GyroFileUtils:
    """直接读写 SharedPreferences XML 文件。"""

    @staticmethod
    def update(file_path: str, x: float, y: float, z: float, port: int = DEFAULT_PORT) -> bool:
        try:
            xml = (
                "<?xml version='1.0' encoding='utf-8' standalone='yes' ?>\n"
                "<map>\n"
                f'    <float name="x" value="{x:.6f}" />\n'
                f'    <float name="y" value="{y:.6f}" />\n'
                f'    <float name="z" value="{z:.6f}" />\n'
                f'    <int name="socket_port" value="{port}" />\n'
                "</map>"
            )
            Path(file_path).write_text(xml, encoding="utf-8")
            print(f"GyroFileUtils: 成功更新文件: {file_path}")
            print(f"    新设置: X={x}, Y={y}, Z={z}, Port={port}")
            return True
        except OSError as e:
            print(f"GyroFileUtils: 写入文件失败: {file_path} - {e}")
            return False

    @staticmethod
    def read(file_path: str) -> dict | None:
        try:
            tree = ET.parse(file_path)
            root = tree.getroot()
            result = {}
            for elem in root:
                name = elem.get("name")
                value = elem.get("value")
                if elem.tag == "float":
                    result[name] = float(value)
                elif elem.tag == "int":
                    result[name] = int(value)
            return result
        except Exception as e:
            print(f"GyroFileUtils: 读取文件失败: {file_path} - {e}")
            return None


def simulate_gyro(client: GyroClient, interval: float = 0.1):
    """模拟持续发送陀螺仪数据（Ctrl+C 停止）。"""
    print("Socket 模式: 开始模拟持续发送陀螺仪数据... (按 Ctrl+C 停止)")
    x = 0.0
    step = 0.1
    try:
        while client.is_connected():
            x += step
            if x > 5.0:
                x = -5.0
            y = 2.0 * math.sin(x)
            z = 1.5 * math.cos(x)
            print(f"发送: X={x:.4f}, Y={y:.4f}, Z={z:.4f}")
            if not client.send_gyro_data(x, y, z):
                break
            time.sleep(interval)
    except KeyboardInterrupt:
        print("\n已停止")


if __name__ == "__main__":
    import sys

    def usage():
        print(f"用法: python {sys.argv[0]} <mode> [options]")
        print("  socket [ip] [port] [x y z]   通过 Socket 发送数据")
        print("  file [path] <x> <y> <z> [port]  直接修改配置文件")
        print("  read [path]                   读取当前配置文件")

    if len(sys.argv) < 2:
        usage()
        sys.exit(1)

    mode = sys.argv[1]

    if mode == "socket":
        ip = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_IP
        port = int(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_PORT
        client = GyroClient(ip, port)
        if not client.connect():
            sys.exit(1)
        if len(sys.argv) >= 7:
            x, y, z = float(sys.argv[4]), float(sys.argv[5]), float(sys.argv[6])
            print(f"Socket 模式: 发送一次固定数据: X={x}, Y={y}, Z={z}")
            client.send_gyro_data(x, y, z)
            client.disconnect()
        else:
            simulate_gyro(client)
            client.disconnect()

    elif mode == "file":
        args = sys.argv[2:]
        # 判断第一个参数是否是路径
        if args and ("/" in args[0] or args[0].endswith(".xml")):
            file_path = args[0]
            args = args[1:]
        else:
            file_path = DEFAULT_FILE_PATH
        if len(args) < 3:
            print("文件模式: 至少需要 x y z 三个值")
            usage()
            sys.exit(1)
        x, y, z = float(args[0]), float(args[1]), float(args[2])
        port = int(args[3]) if len(args) > 3 else DEFAULT_PORT
        GyroFileUtils.update(file_path, x, y, z, port)

    elif mode == "read":
        file_path = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_FILE_PATH
        data = GyroFileUtils.read(file_path)
        if data:
            print(f"当前配置: {data}")

    else:
        print(f"未知模式: {mode}")
        usage()
        sys.exit(1)
