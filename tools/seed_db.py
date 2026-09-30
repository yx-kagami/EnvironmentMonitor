# -*- coding: utf-8 -*-
"""
seed_db.py — 往 EnvironmentMonitor 的 sensor_history 表灌测试数据（默认 100 万条）
用法:
    python seed_db.py                  # 当前目录的 sensor_data.db，灌 100 万条
    python seed_db.py <db路径> <条数>  # 自定义
说明:
  - 若 sensor_history 表不存在会自动创建（schema 与 databasemanager.cpp 一致）；
  - timestamp 按 Qt SQLite 驱动的 ISO 格式写入（yyyy-MM-ddTHH:mm:ss.zzz），
    保证程序里 BETWEEN 字符串比较行为一致；
  - 数据带昼夜周期（正弦+噪声），查询/图表效果接近真实上报；
  - 分批 executemany（5 万条/批）：100 万条攒一个 list 会吃下近 1GB 内存。
"""
import sqlite3
import random
import math
import sys
import time
import datetime

DB_PATH = "sensor_data.db"              # 默认当前目录；程序的库在 exe 同目录
N = 1_000_000
BATCH = 50_000
STEP = datetime.timedelta(seconds=1)    # 每秒一条，100 万条约 11.6 天
# 数据结束于运行时刻：app 退出时会清理 30 天前的旧数据，写死日期灌进去会被清掉
START = datetime.datetime.now() - N * STEP
CLEAN_EXISTING = True                   # 灌入前清空旧数据

SCHEMA = """
CREATE TABLE IF NOT EXISTS sensor_history (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  timestamp DATETIME NOT NULL,
  temperature REAL NOT NULL,
  humidity REAL NOT NULL,
  light_intensity REAL NOT NULL,
  temp_upper REAL NOT NULL,
  temp_lower REAL NOT NULL,
  hum_upper REAL NOT NULL,
  hum_lower REAL NOT NULL,
  light_threshold REAL NOT NULL,
  switch1_state INTEGER NOT NULL,
  switch2_state INTEGER NOT NULL,
  switch3_state INTEGER NOT NULL,
  switch4_state INTEGER NOT NULL,
  switch5_state INTEGER NOT NULL,
  switch6_state INTEGER NOT NULL
)"""

INSERT_SQL = (
    "INSERT INTO sensor_history ("
    "timestamp, temperature, humidity, light_intensity,"
    " temp_upper, temp_lower, hum_upper, hum_lower, light_threshold,"
    " switch1_state, switch2_state, switch3_state,"
    " switch4_state, switch5_state, switch6_state)"
    " VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)")


def main():
    db_path = sys.argv[1] if len(sys.argv) > 1 else DB_PATH
    n = int(sys.argv[2]) if len(sys.argv) > 2 else N
    random.seed(42)  # 固定种子，数据可复现

    conn = sqlite3.connect(db_path)
    cur = conn.cursor()
    cur.execute(SCHEMA)
    # 索引与 app 端 DatabaseManager::createIndices() 保持一致：
    # 脚本灌的库若没跑过 app 就没有索引，状态栏 MIN/MAX 和分页查询会全表扫描
    cur.execute("CREATE INDEX IF NOT EXISTS idx_sensor_history_timestamp ON sensor_history(timestamp)")
    cur.execute("CREATE INDEX IF NOT EXISTS idx_sensor_history_temperature ON sensor_history(temperature)")

    if CLEAN_EXISTING:
        cur.execute("DELETE FROM sensor_history")
        print(f"已清空旧数据: {cur.rowcount} 行")

    rows = []
    t0 = time.perf_counter()
    for i in range(n):
        ts = (START + i * STEP).strftime("%Y-%m-%dT%H:%M:%S") + ".000"
        # 昼夜周期：一天 86400 秒
        phase = 2 * math.pi * (i % 86400) / 86400
        temp = 25 + 5 * math.sin(phase) + random.uniform(-0.5, 0.5)
        hum = 55 - 10 * math.sin(phase) + random.uniform(-2, 2)
        light = max(0.0, 700 * math.sin(phase - math.pi / 2)) + random.uniform(0, 20)
        rows.append((
            ts,
            round(temp, 2), round(hum, 2), round(light, 2),
            30.0, 15.0, 70.0, 40.0, 500.0,      # 五个阈值固定
            int(temp > 28),                      # 制冷
            int(temp < 16),                      # 加热
            int(hum < 45),                       # 加湿
            int(hum > 65),                       # 除湿
            int(light < 300),                    # 补光
            int(temp > 30 or temp < 15),         # 报警
        ))
        if len(rows) >= BATCH:
            cur.executemany(INSERT_SQL, rows)
            rows.clear()
    if rows:
        cur.executemany(INSERT_SQL, rows)
    conn.commit()

    cur.execute("SELECT COUNT(*), MIN(timestamp), MAX(timestamp) FROM sensor_history")
    count, tmin, tmax = cur.fetchone()
    conn.close()
    print(f"灌入完成: {count} 行, 范围 {tmin} ~ {tmax}, 耗时 {time.perf_counter()-t0:.1f}s")
    print(f"数据库文件: {db_path}")

if __name__ == "__main__":
    main()
