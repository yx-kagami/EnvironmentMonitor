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

store = ModbusSlaveContext(
    hr=ModbusSequentialDataBlock(0, [253, 610, 2048]),
    zero_mode=True,
)
context = ModbusServerContext(slaves=store, single=True)

def update_loop():
    """每 2 秒随机改一次寄存器值。"""
    while True:
        time.sleep(2)
        t = 250 + random.randint(-20, 20)
        h = 600 + random.randint(-50, 50)
        light = random.randint(0, 4095)
        store.setValues(3, 0, [t, h, light])  # 3 = 保持寄存器对应的功能码
        log.info("寄存器更新 -> 温度 %.1f℃  湿度 %.1f%%  光照 %d", t / 10, h / 10, light)

if __name__ == "__main__":
    threading.Thread(target=update_loop, daemon=True).start()
    log.info("TCP 从站启动：%s:%d，单元 ID 01", HOST, PORT)
    StartTcpServer(context=context, address=(HOST, PORT))
    