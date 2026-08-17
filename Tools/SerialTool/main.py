"""
Servo Limit Detect — 串口接收 & YAML 导出工具

接收 STM32 标定报告 (servo_limits[])，解析后合并到参考 YAML 模板导出。
"""

import sys
import re
import yaml
from datetime import datetime
from pathlib import Path

from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QGroupBox, QLabel, QComboBox, QPushButton, QTextEdit,
    QMessageBox, QFileDialog, QGridLayout, QLineEdit
)
from PyQt5.QtCore import QThread, QTimer, pyqtSignal
from PyQt5.QtSerialPort import QSerialPort, QSerialPortInfo


# ─── 接收性能参数 ────────────────────────────────────────────────
# 防止长时间接收后 received_text 无限增长，拖慢每次的 UI 刷新与正则检索。
_RECEIVED_TEXT_LIMIT = 256 * 1024   # 超过此大小开始截断
_RECEIVED_TEXT_KEEP  = 64 * 1024    # 截断后仅保留尾部（足以容纳完整表格）
_UI_FLUSH_INTERVAL_MS = 80          # QTextEdit 批量刷盘间隔，合并多次 readyRead


# ─── 串口接收线程 ────────────────────────────────────────────────
class SerialWorker(QThread):
    data_received = pyqtSignal(bytes)

    def __init__(self, port_name: str, baudrate: int):
        super().__init__()
        self.port_name = port_name
        self.baudrate = baudrate
        self.serial: QSerialPort | None = None

    def run(self):
        self.serial = QSerialPort(self.port_name)
        self.serial.setBaudRate(self.baudrate)
        self.serial.setDataBits(QSerialPort.Data8)
        self.serial.setParity(QSerialPort.NoParity)
        self.serial.setStopBits(QSerialPort.OneStop)
        self.serial.setFlowControl(QSerialPort.NoFlowControl)
        self.serial.setReadBufferSize(0)  # unlimited buffer

        # Use event-driven readyRead signal (more reliable than polling)
        self.serial.readyRead.connect(self._on_ready_read)

        if not self.serial.open(QSerialPort.ReadOnly):
            self.data_received.emit(
                f"[ERROR] 无法打开串口 {self.port_name}\n".encode())
            return

        # Start thread event loop — required for signal/slot to work
        self.exec_()

    def _on_ready_read(self):
        """Read ALL available bytes (may be called multiple times as data arrives)."""
        data = self.serial.readAll()
        if not data.isEmpty():
            self.data_received.emit(data.data())

    def stop(self):
        if self.serial and self.serial.isOpen():
            self.serial.close()
        self.quit()
        self.wait(1000)


# ─── 主窗口 ──────────────────────────────────────────────────────
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Servo Limit Detect — 串口工具")
        self.setMinimumSize(760, 650)

        self.worker: SerialWorker | None = None
        self.received_text = ""
        self._rx_pending: list[str] = []   # 待批量刷入 QTextEdit 的文本
        self._rx_flush_timer = QTimer(self)
        self._rx_flush_timer.setInterval(_UI_FLUSH_INTERVAL_MS)
        self._rx_flush_timer.timeout.connect(self._flush_rx_text)
        self.parsed_servos: dict[int, dict] = {}   # idx → {channel, center, lower, upper}
        self.total_time = ""
        self.template_data: dict | None = None

        self._build_ui()
        self._refresh_ports()
        self._status("就绪 — 请先加载参考 YAML 模板")

    # ── UI 构建 ────────────────────────────────────────────────
    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)

        # ── 模板加载 ──
        gb_tpl = QGroupBox("YAML 模板")
        tpl_row = QHBoxLayout(gb_tpl)
        self.lbl_tpl = QLineEdit()
        self.lbl_tpl.setReadOnly(True)
        self.lbl_tpl.setPlaceholderText("加载参考 servo_config.yaml（未加载）")
        tpl_row.addWidget(self.lbl_tpl)
        self.btn_tpl = QPushButton("加载模板")
        self.btn_tpl.clicked.connect(self._load_template)
        tpl_row.addWidget(self.btn_tpl)
        root.addWidget(gb_tpl)

        # ── 串口设置 ──
        gb_serial = QGroupBox("串口设置")
        grid = QGridLayout(gb_serial)

        grid.addWidget(QLabel("端口:"), 0, 0)
        self.cmb_port = QComboBox()
        self.btn_refresh = QPushButton("刷新")
        self.btn_refresh.clicked.connect(self._refresh_ports)
        grid.addWidget(self.cmb_port, 0, 1)
        grid.addWidget(self.btn_refresh, 0, 2)

        grid.addWidget(QLabel("波特率:"), 0, 3)
        self.cmb_baud = QComboBox()
        self.cmb_baud.addItems(["9600", "19200", "38400", "57600", "115200",
                                "230400", "460800", "921600"])
        self.cmb_baud.setCurrentText("115200")
        grid.addWidget(self.cmb_baud, 0, 4)

        self.btn_open = QPushButton("打开串口")
        self.btn_open.clicked.connect(self._toggle_serial)
        grid.addWidget(self.btn_open, 0, 5)

        root.addWidget(gb_serial)

        # ── 接收区 ──
        gb_rx = QGroupBox("接收数据")
        rx_layout = QVBoxLayout(gb_rx)
        self.text_rx = QTextEdit()
        self.text_rx.setReadOnly(True)
        self.text_rx.setFontFamily("Consolas")
        # 限制文档块数，避免长时间接收后 QTextEdit 渲染越来越卡
        self.text_rx.document().setMaximumBlockCount(5000)
        rx_layout.addWidget(self.text_rx)

        btn_row = QHBoxLayout()
        self.btn_clear = QPushButton("清空")
        self.btn_clear.clicked.connect(self._clear_rx)
        btn_row.addWidget(self.btn_clear)

        btn_row.addStretch()

        self.btn_export = QPushButton("导出 YAML")
        self.btn_export.setEnabled(False)
        self.btn_export.clicked.connect(self._export_yaml)
        btn_row.addWidget(self.btn_export)

        rx_layout.addLayout(btn_row)
        root.addWidget(gb_rx)

    # ── 加载模板 YAML ──────────────────────────────────────────
    def _load_template(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "加载参考 servo_config.yaml", "",
            "YAML Files (*.yaml *.yml);;All Files (*)"
        )
        if not path:
            return

        try:
            with open(path, "r", encoding="utf-8") as f:
                self.template_data = yaml.safe_load(f)
            self.lbl_tpl.setText(path)
            self._status(f"模板已加载: {Path(path).name}")
        except Exception as e:
            QMessageBox.critical(self, "加载失败", str(e))

    # ── 端口刷新 ───────────────────────────────────────────────
    def _refresh_ports(self):
        self.cmb_port.clear()
        for info in QSerialPortInfo.availablePorts():
            self.cmb_port.addItem(info.portName())

    # ── 打开/关闭串口 ───────────────────────────────────────────
    def _toggle_serial(self):
        if self.worker and self.worker.isRunning():
            self._flush_rx_text()   # 关闭前把残留缓冲刷完
            self.worker.stop()
            self.worker = None
            self.btn_open.setText("打开串口")
            self._status("串口已关闭")
            return

        port = self.cmb_port.currentText()
        if not port:
            QMessageBox.warning(self, "提示", "请选择串口")
            return

        baud = int(self.cmb_baud.currentText())
        self.worker = SerialWorker(port, baud)
        self.worker.data_received.connect(self._on_data)
        self.worker.start()
        self.btn_open.setText("关闭串口")
        self._status(f"已连接 {port} @ {baud}")

    # ── 接收数据处理 ────────────────────────────────────────────
    def _on_data(self, data: bytes):
        try:
            text = data.decode("utf-8", errors="replace")
        except Exception:
            return

        # 原始缓冲仅用于正则解析，限制其大小避免无限增长
        self.received_text += text
        if len(self.received_text) > _RECEIVED_TEXT_LIMIT:
            self.received_text = self.received_text[-_RECEIVED_TEXT_KEEP:]

        # 文本先入待刷缓冲，按固定间隔批量写入 QTextEdit（减少重绘次数）
        self._rx_pending.append(text)
        if not self._rx_flush_timer.isActive():
            self._rx_flush_timer.start()

        self._try_parse()

    def _flush_rx_text(self):
        """把攒下的文本一次性写入 QTextEdit，避免逐小段插入触发大量重绘。"""
        self._rx_flush_timer.stop()
        if not self._rx_pending:
            return

        text = "".join(self._rx_pending)
        self._rx_pending.clear()

        self.text_rx.moveCursor(self.text_rx.textCursor().End)
        self.text_rx.insertPlainText(text)
        bar = self.text_rx.verticalScrollBar()
        bar.setValue(bar.maximum())   # 跟随最新数据

    def _clear_rx(self):
        self._rx_flush_timer.stop()
        self._rx_pending.clear()
        self.received_text = ""
        self.text_rx.clear()

    # ── 解析 servo_limits 表格 ──────────────────────────────────
    def _try_parse(self):
        pattern = (
            r"={5,}\s*\r?\n"
            r"\s*Servo Limit Sweep Results \(servo_limits\[\]\)\s*\r?\n"
            r"-{5,}\s*\r?\n"
            r"\s*Idx\s+CH\s+Center\s+Lower\s+Upper\s*\r?\n"
            r"-{5,}\s*\r?\n"
            r"((?:\s*\d+\s+\d+\s+\d+\s+\d+\s+\d+\s*\r?\n)+)"
            r"={5,}\s*\r?\n"
            r"\s*Total:\s*\d+\s+servos\s*\r?\n"
            r"={5,}"
        )

        # 快速失败：缓冲区里没有表格起始标记就不做完整正则
        if "=====" not in self.received_text:
            return

        match = re.search(pattern, self.received_text)
        if not match:
            return

        table_block = match.group(1)
        servos = {}
        for line in table_block.strip().splitlines():
            parts = line.strip().split()
            if len(parts) >= 5:
                idx = int(parts[0])
                servos[idx] = {
                    "channel": int(parts[1]),
                    "center":  int(parts[2]),
                    "lower":   int(parts[3]),
                    "upper":   int(parts[4]),
                }

        if servos:
            self.parsed_servos = servos
            self.btn_export.setEnabled(True)
            self._status(f"已解析 {len(servos)} 个舵机数据，可以导出 YAML")

            time_match = re.search(r"Sweep total time:\s*(\d+)s", self.received_text)
            if time_match:
                self.total_time = time_match.group(1)

            # 解析成功即可收窄原始缓冲，之后的表格只需小范围正则
            self.received_text = self.received_text[match.end():]

    # ── 导出 YAML ───────────────────────────────────────────────
    def _export_yaml(self):
        if not self.parsed_servos:
            QMessageBox.warning(self, "提示", "没有可导出的数据。")
            return

        if not self.template_data:
            QMessageBox.warning(self, "提示", "请先加载参考 YAML 模板。")
            return

        servo_info = self.template_data.get("SERVO_INFO", {})
        if not servo_info:
            QMessageBox.warning(self, "提示", "模板中没有 SERVO_INFO 节点。")
            return

        # 按 channel_idx 合并标定结果
        for key, entry in servo_info.items():
            ch = entry.get("channel_idx")
            if ch is None:
                continue
            calib = self._find_by_channel(ch)
            if calib is None:
                continue

            orig_start = entry.get("start_deg", 0)
            orig_end   = entry.get("end_deg", 0)

            # 保持原始的大小方向: start>end → start=upper, end=lower
            #                     start<end → start=lower, end=upper
            if orig_start > orig_end:
                entry["start_deg"] = calib["upper"]
                entry["end_deg"]   = calib["lower"]
            else:
                entry["start_deg"] = calib["lower"]
                entry["end_deg"]   = calib["upper"]

            entry["temp_deg"] = calib["center"]

        # 更新时间戳
        hw_info = self.template_data.setdefault("HW_INFO", {})
        hw_info["calib_date"] = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        if self.total_time:
            hw_info["calib_time_s"] = int(self.total_time)

        # 保持顶层键序
        doc = {}
        if "HW_INFO" in self.template_data:
            doc["HW_INFO"] = self.template_data["HW_INFO"]
        if "SERVO_INFO" in self.template_data:
            doc["SERVO_INFO"] = self.template_data["SERVO_INFO"]

        default_name = f"servo_config_{datetime.now().strftime('%Y%m%d_%H%M%S')}.yaml"
        path, _ = QFileDialog.getSaveFileName(
            self, "导出 YAML", default_name, "YAML Files (*.yaml *.yml);;All Files (*)"
        )
        if not path:
            return

        try:
            with open(path, "w", encoding="utf-8") as f:
                yaml.dump(doc, f, allow_unicode=True, default_flow_style=False, sort_keys=False)
            QMessageBox.information(self, "导出成功", f"已保存到:\n{path}")
            self._status(f"YAML 已导出 → {Path(path).name}")
        except Exception as e:
            QMessageBox.critical(self, "导出失败", str(e))

    def _find_by_channel(self, channel: int) -> dict | None:
        for entry in self.parsed_servos.values():
            if entry["channel"] == channel:
                return entry
        return None

    # ── 状态栏 ──────────────────────────────────────────────────
    def _status(self, msg: str):
        self.statusBar().showMessage(msg)


# ─── 入口 ────────────────────────────────────────────────────────
def main():
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    win = MainWindow()
    win.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
