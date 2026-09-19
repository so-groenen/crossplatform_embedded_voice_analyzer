import sys
import struct
from collections import deque
import numpy as np
import serial
from scipy.interpolate import interp1d

from PyQt6.QtCore import QThread, pyqtSignal, QTimer, Qt
from PyQt6.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QWidget
import pyqtgraph as pg

# --- Configuration Parameters ---
PORT          = "/dev/ttyACM1"
BAUDRATE      = 921_600
SAMPLE_RATE   = 20_000
FFT_SIZE      = 1024
MAG_SIZE      = FFT_SIZE // 2

DISPLAY_WIDTH = 240  # Rolling buffer width
MIN_FREQ      = 50
MAX_FREQ      = 450
MAX_AMP       = 0.1

HEADER_PEAK   = b"\xab\xbc\xcd\xde"
HEADER_MAG    = b"\xff\xee\xdd\xcc"
HEADER_BYTES  = 4
SIZE_PEAK     = 4
SIZE_MAG      = MAG_SIZE * 4

# Frequency Axis and Interpolation setup
full_freq_axis   = np.fft.rfftfreq(FFT_SIZE, d=1.0 / SAMPLE_RATE)
K_MIN            = np.searchsorted(full_freq_axis, MIN_FREQ)
K_MAX            = np.searchsorted(full_freq_axis, MAX_FREQ)
freq_axis_sliced = full_freq_axis[K_MIN:K_MAX]

INTERP_SAMPLES   = 100
freq_axis_interp = np.linspace(freq_axis_sliced[0], freq_axis_sliced[-1], INTERP_SAMPLES)


# --- Serial Reader Worker Thread ---
class SerialReaderThread(QThread):
    data_received = pyqtSignal(str, object)  # (mode, payload)

    def __init__(self, port, baudrate):
        super().__init__()
        self.port = port
        self.baudrate = baudrate
        self.running = True

    def read_exact(self, ser, nbytes):
        buf = bytearray()
        while len(buf) < nbytes and self.running:
            chunk = ser.read(nbytes - len(buf))
            if not chunk:
                return None
            buf.extend(chunk)
        return bytes(buf)

    def run(self):
        try:
            ser = serial.Serial(self.port, self.baudrate, timeout=0.1)
        except serial.SerialException as e:
            print(f"Error opening serial port {self.port}: {e}")
            return

        while self.running:
            if ser.in_waiting >= HEADER_BYTES:
                header = ser.read(HEADER_BYTES)

                if header == HEADER_PEAK:
                    raw_bytes = self.read_exact(ser, SIZE_PEAK)
                    if raw_bytes:
                        val = struct.unpack("<f", raw_bytes)[0]
                        if np.isfinite(val):
                            self.data_received.emit("peak", val)

                elif header == HEADER_MAG:
                    raw_bytes = self.read_exact(ser, SIZE_MAG)
                    if raw_bytes:
                        unpacked = struct.unpack("<512f", raw_bytes)
                        mag_array = np.array(unpacked, dtype=np.float32)
                        self.data_received.emit("spectrogram", mag_array)

        ser.close()

    def stop(self):
        self.running = False
        self.wait()


# --- Main Application Window ---
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("RP2350 Audio Visualizer (PyQt6)")
        self.resize(800, 400)

        # Main Widget & Layout
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)

        # PyQtGraph Plot Widget Setup
        self.plot_widget = pg.PlotWidget()
        layout.addWidget(self.plot_widget)

        # Pre-configure Display Buffers
        self.display_buffer = np.zeros((DISPLAY_WIDTH, INTERP_SAMPLES), dtype=np.float32)
        self.peak_history = deque([MIN_FREQ] * DISPLAY_WIDTH, maxlen=DISPLAY_WIDTH)

        # Visualizer Graphics Items
        self.img_item = pg.ImageItem()
        self.curve_item = pg.PlotCurveItem(pen=pg.mkPen(color='#ff7f0e', width=2))

        # Colormap for Spectrogram
        cmap = pg.colormap.get('inferno')
        self.img_item.setLookupTable(cmap.getLookupTable())
        self.img_item.setLevels([0, MAX_AMP])

        self.current_rendered_mode = None

        # Start Background Reader Thread
        self.serial_thread = SerialReaderThread(PORT, BAUDRATE)
        self.serial_thread.data_received.connect(self.on_data_received)
        self.serial_thread.start()

        # UI Refresh Timer (~60 FPS)
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_gui)
        self.timer.start(16)

    def set_view_mode(self, mode):
        if self.current_rendered_mode == mode:
            return

        self.plot_widget.clear()
        self.plot_widget.showGrid(x=True, y=True, alpha=0.3)

        if mode == "spectrogram":
            self.plot_widget.setTitle("Scrolling Spectrogram (Magnitude)")
            self.plot_widget.setLabel('left', 'Frequency', units='Hz')
            self.plot_widget.setLabel('bottom', 'Time History (Frames)')
            
            # Add Image Item and Set Scale to match Frequency Bounds
            self.plot_widget.addItem(self.img_item)
            scale_y = (MAX_FREQ - MIN_FREQ) / INTERP_SAMPLES
            self.img_item.setRect(0, MIN_FREQ, DISPLAY_WIDTH, MAX_FREQ - MIN_FREQ)

        elif mode == "peak":
            self.plot_widget.setTitle("Horizontal Scrolling Peak Frequency Tracker")
            self.plot_widget.setLabel('left', 'Frequency', units='Hz')
            self.plot_widget.setLabel('bottom', 'Time History (Frames)')
            self.plot_widget.setYRange(MIN_FREQ, MAX_FREQ)
            self.plot_widget.setXRange(0, DISPLAY_WIDTH)
            
            self.plot_widget.addItem(self.curve_item)

        self.current_rendered_mode = mode

    def on_data_received(self, mode, payload):
        self.set_view_mode(mode)

        if mode == "spectrogram":
            mag_sliced = np.nan_to_num(payload[K_MIN:K_MAX], nan=0.0, posinf=MAX_AMP, neginf=0.0)
            try:
                quad_interp = interp1d(
                    freq_axis_sliced,
                    mag_sliced,
                    kind="quadratic",
                    fill_value="extrapolate",
                )
                mag_interp = np.clip(quad_interp(freq_axis_interp), 0, None)
            except Exception:
                mag_interp = np.zeros_like(freq_axis_interp)

            # Shift display buffer left and insert vector
            self.display_buffer[:-1, :] = self.display_buffer[1:, :]
            self.display_buffer[-1, :] = mag_interp

        elif mode == "peak":
            self.peak_history.append(payload)

    def update_gui(self):
        """Timer callback for rendering offloaded buffers"""
        if self.current_rendered_mode == "spectrogram":
            self.img_item.setImage(self.display_buffer, autoLevels=False)
        elif self.current_rendered_mode == "peak":
            self.curve_item.setData(np.arange(DISPLAY_WIDTH), list(self.peak_history))

    def closeEvent(self, event):
        self.thread.stop()
        event.accept()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())