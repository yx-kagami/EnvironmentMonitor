"""Modbus TCP虚拟从站（pymodbus 3.6.9）
保持寄存器映射
    寄存器0 = 温度x10
    寄存器1 = 湿度x10
    寄存器2 = 光照ADC原始值（0~4095）
"""
import logging
import random
import threading
import time

from pymodbus.datastore import (
    ModbusSequentialDataBlock,
    ModbusServerContext,
    ModbusSlaveContext,
)
from pymodbus.server import StartTcpServer

HOST, PORT = "127.0.0.1", 5020  # 502 是特权端口，demo 用 5020

logging.basicConfig(format="%(asctime)s %(message)s", level=logging.INFO)
log = logging.getLogger("tcp_slave")

INITIAL = [253, 610, 256, 300, 200, 700, 500, 200, 0, 0, 0, 0, 0, 0]

store = ModbusSlaveContext(
    hr=ModbusSequentialDataBlock(0, INITIAL),
    zero_mode=True,
)
context = ModbusServerContext(slaves=store, single=True)

def update_loop():
    """每 1 秒随机改一次寄存器值。"""
    t, h, l = 253.0, 610.0, 256.0
    while True:
        time.sleep(1)
        sw = store.getValues(3, 8, 6)   #开关控制指令状态
        t += (253 - t) * 0.05 + random.uniform(-3, 3) - (4 if sw[0] else 0) + (4 if sw[1] else 0)
        h += (610 - h) * 0.05 + random.uniform(-4, 4) + (15 if sw[2] else 0) - (15 if sw[3] else 0)
        l += (256 - l) * 0.1 + random.uniform(-6, 6) + (300 if sw[4] else 0)
        vals = [max(0, int(round(v))) for v in (t, h, l)]
        store.setValues(3, 0, vals)  # 3 = 保持寄存器对应的功能码
        log.info("寄存器更新 -> 温度 %.1f℃  湿度 %.1f%%  光照 %d", t / 10, h / 10, vals[2])

if __name__ == "__main__":
    threading.Thread(target=update_loop, daemon=True).start()
    log.info("TCP 从站启动：%s:%d，单元 ID 01", HOST, PORT)
    StartTcpServer(context=context, address=(HOST, PORT))
