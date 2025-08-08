import os
import sys
import subprocess
import winreg
from PyQt6 import QtWidgets, QtCore, QtGui

INI_FILENAME = "powercfg.ini"
PROCESS_NAME = "PowerSwitcher.exe"

REG_PATH = r"SYSTEM\CurrentControlSet\Control\Power\PowerSettings\54533251-82be-4824-96c1-47b60b740d00\be337238-0d82-4146-a960-4f3749d470c7"
REG_VALUE_NAME = "Attributes"
REG_VALUE_DESIRED = 2

class BlurBackground(QtWidgets.QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setGeometry(0, 0, 400, 220)
        self.setAttribute(QtCore.Qt.WidgetAttribute.WA_TranslucentBackground)
        self.setStyleSheet("""
            background-color: rgba(30, 30, 30, 180);
            border-radius: 15px;
            border: 1.5px solid #c0c0c0;
        """)
        blur = QtWidgets.QGraphicsBlurEffect(self)
        blur.setBlurRadius(15)
        self.setGraphicsEffect(blur)

class MinimalGrayUI(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()

        self.load_custom_font()

        self.setWindowTitle("PowerCFG Edit")
        self.setWindowFlag(QtCore.Qt.WindowType.FramelessWindowHint)
        self.setAttribute(QtCore.Qt.WidgetAttribute.WA_TranslucentBackground)
        self.resize(400, 220)
        self.opacity = 0.55
        self.setWindowOpacity(self.opacity)

        self.bg_blur = BlurBackground(self)

        self.main_frame = QtWidgets.QFrame(self)
        self.main_frame.setGeometry(0, 0, 400, 220)
        self.main_frame.setStyleSheet("""
            background-color: rgba(30, 30, 30, 80);
            border-radius: 15px;
            border: 1.5px solid #c0c0c0;
        """)

        self.btn_close = QtWidgets.QPushButton("×", self.main_frame)
        self.btn_close.setGeometry(360, 10, 30, 30)
        self.btn_close.setStyleSheet("""
            color: #ccc;
            background: transparent;
            font-size: 20px;
            font-weight: bold;
            border: none;
            font-family: Ethnocentric;
        """)
        self.btn_close.setCursor(QtGui.QCursor(QtCore.Qt.CursorShape.PointingHandCursor))
        self.btn_close.clicked.connect(self.close)

        self.input1 = QtWidgets.QLineEdit(self.main_frame)
        self.input1.setPlaceholderText("No TurboBoost GUID")
        self.input1.setGeometry(30, 50, 340, 40)
        self.input1.setStyleSheet("""
            border: 1.3px solid #aaa;
            border-radius: 10px;
            padding-left: 10px;
            background: #e6e6e6;
            font-size: 16px;
            color: #222;
            font-family: Ethnocentric;
        """)

        self.input2 = QtWidgets.QLineEdit(self.main_frame)
        self.input2.setPlaceholderText("TurboBoost GUID")
        self.input2.setGeometry(30, 100, 340, 40)
        self.input2.setStyleSheet(self.input1.styleSheet())

        self.btn_save = QtWidgets.QPushButton("Save", self.main_frame)
        self.btn_save.setStyleSheet("""
            background-color: #c0c0c0;
            border-radius: 10px;
            font-weight: bold;
            font-size: 16px;
            color: #222;
            font-family: Ethnocentric;
        """)
        self.btn_save.setCursor(QtGui.QCursor(QtCore.Qt.CursorShape.PointingHandCursor))
        self.btn_save.clicked.connect(self.on_save)
        self.btn_save.adjustSize()
        self.btn_save.setMinimumWidth(self.btn_save.width() + 20)
        self.btn_save.setGeometry(70, 160, self.btn_save.width(), 40)

        self.btn_autocfg = QtWidgets.QPushButton("AutoCFG", self.main_frame)
        self.btn_autocfg.setStyleSheet(self.btn_save.styleSheet())
        self.btn_autocfg.setCursor(QtGui.QCursor(QtCore.Qt.CursorShape.PointingHandCursor))
        self.btn_autocfg.clicked.connect(self.on_autocfg)
        self.btn_autocfg.adjustSize()
        self.btn_autocfg.setMinimumWidth(self.btn_autocfg.width() + 20)
        self.btn_autocfg.setGeometry(230, 160, self.btn_autocfg.width(), 40)

        self.offset = None
        self.main_frame.mousePressEvent = self.mouse_press_event
        self.main_frame.mouseMoveEvent = self.mouse_move_event

        self.load_ini_data()

    def load_custom_font(self):
        try:
            base_path = getattr(sys, '_MEIPASS', os.path.abspath("."))
            fonts_path = os.path.join(base_path, "fonts")
            font_path = os.path.join(fonts_path, "ethnocentric.ttf")
            if os.path.exists(font_path):
                font_id = QtGui.QFontDatabase.addApplicationFont(font_path)
                if font_id == -1:
                    print("Не удалось загрузить шрифт ethnocentric.ttf")
                else:
                    family = QtGui.QFontDatabase.applicationFontFamilies(font_id)[0]
                    self.custom_font_family = family
            else:
                print(f"Шрифт не найден по пути: {font_path}")
                self.custom_font_family = None
        except Exception as e:
            print(f"Ошибка загрузки шрифта: {e}")
            self.custom_font_family = None

    def load_ini_data(self):
        try:
            with open(INI_FILENAME, "r", encoding="utf-8") as f:
                lines = f.readlines()
            if len(lines) >= 2:
                self.input1.setText(self.extract_brackets(lines[0]))
                self.input2.setText(self.extract_brackets(lines[1]))
            else:
                self.input1.setText("")
                self.input2.setText("")
        except Exception as e:
            print(f"Не смог загрузить {INI_FILENAME}: {e}")
            self.input1.setText("")
            self.input2.setText("")

    def extract_brackets(self, line):
        start = line.find("{")
        end = line.find("}")
        if start != -1 and end != -1 and end > start:
            return line[start+1:end]
        return ""

    def on_save(self):
        val1 = self.input1.text().strip()
        val2 = self.input2.text().strip()

        if not val1 or not val2:
            QtWidgets.QMessageBox.warning(self, "Ошибка", "Поля не могут быть пустыми!")
            return

        line1 = f"{{{val1}}}=FPS Boost Disabled\n"
        line2 = f"{{{val2}}}=FPS Boost Enabled\n"

        try:
            lines = []
            if os.path.exists(INI_FILENAME):
                with open(INI_FILENAME, "r", encoding="utf-8") as f:
                    lines = f.readlines()
            if len(lines) >= 2:
                lines[0] = line1
                lines[1] = line2
            else:
                lines = [line1, line2]
            with open(INI_FILENAME, "w", encoding="utf-8") as f:
                f.writelines(lines)

            subprocess.run(["taskkill", "/f", "/im", PROCESS_NAME], capture_output=True)

            script_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
            exe_path = os.path.join(script_dir, PROCESS_NAME)
            if os.path.exists(exe_path):
                subprocess.Popen([exe_path])
            else:
                QtWidgets.QMessageBox.warning(self, "Ошибка", f"Не найден {PROCESS_NAME} в папке со скриптом.")

            QtWidgets.QApplication.quit()

        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Ошибка", f"Ошибка при сохранении:\n{e}")

    def on_autocfg(self):
        try:
            reg = winreg.ConnectRegistry(None, winreg.HKEY_LOCAL_MACHINE)
            key = winreg.OpenKey(reg, REG_PATH, 0, winreg.KEY_READ | winreg.KEY_WRITE)
            try:
                val, val_type = winreg.QueryValueEx(key, REG_VALUE_NAME)
                if val != REG_VALUE_DESIRED:
                    winreg.SetValueEx(key, REG_VALUE_NAME, 0, val_type, REG_VALUE_DESIRED)
            except FileNotFoundError:
                winreg.SetValueEx(key, REG_VALUE_NAME, 0, winreg.REG_DWORD, REG_VALUE_DESIRED)
            finally:
                winreg.CloseKey(key)

            turbo_off_guid = self.create_power_scheme("TurboBoost OFF")
            turbo_on_guid = self.create_power_scheme("TurboBoost ON")

            perf_boost_mode_guid = "be337238-0d82-4146-a960-4f3749d470c7"
            subgroup_guid = "54533251-82be-4824-96c1-47b60b740d00"

            subprocess.run([
                "powercfg", "-setacvalueindex", turbo_off_guid, subgroup_guid, perf_boost_mode_guid, "0"
            ], check=True)
            subprocess.run([
                "powercfg", "-setdcvalueindex", turbo_off_guid, subgroup_guid, perf_boost_mode_guid, "0"
            ], check=True)

            subprocess.run([
                "powercfg", "-setacvalueindex", turbo_on_guid, subgroup_guid, perf_boost_mode_guid, "2"
            ], check=True)
            subprocess.run([
                "powercfg", "-setdcvalueindex", turbo_on_guid, subgroup_guid, perf_boost_mode_guid, "2"
            ], check=True)

            subprocess.run(["powercfg", "-S", turbo_off_guid], check=True)

            self.input1.setText(turbo_off_guid)
            self.input2.setText(turbo_on_guid)

            QtWidgets.QMessageBox.information(self, "AutoCFG", "Схемы созданы и настроены, реестр обновлен!")

        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Ошибка AutoCFG", f"Ошибка:\n{e}")

    def create_power_scheme(self, name):
        base_guid = self.get_high_performance_guid()
        if not base_guid:
            raise RuntimeError("Не удалось получить GUID схемы 'Максимальная производительность'")

        before_guids = self.list_power_schemes()

        subprocess.run(["powercfg", "-duplicatescheme", base_guid], check=True, capture_output=True)

        after_guids = self.list_power_schemes()

        new_guids = set(after_guids) - set(before_guids)
        if not new_guids:
            raise RuntimeError("Не удалось определить GUID новой схемы")

        new_guid = new_guids.pop()

        subprocess.run(["powercfg", "-changename", new_guid, name], check=True)
        return new_guid

    def list_power_schemes(self):
        result = subprocess.run(["powercfg", "-list"], capture_output=True, text=True, check=True)
        guids = []
        for line in result.stdout.splitlines():
            parts = line.strip().split()
            for part in parts:
                if part.count("-") == 4 and len(part) == 36:
                    guids.append(part.lower())
        return guids

    def get_high_performance_guid(self):
        return "8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c"

    def mouse_press_event(self, event):
        if event.button() == QtCore.Qt.MouseButton.LeftButton:
            self.offset = event.pos()

    def mouse_move_event(self, event):
        if self.offset is not None and event.buttons() == QtCore.Qt.MouseButton.LeftButton:
            self.move(self.pos() + event.pos() - self.offset)

if __name__ == "__main__":
    if not sys.platform.startswith("win"):
        print("Чо смотришь.")
        sys.exit(1)

    app = QtWidgets.QApplication(sys.argv)
    window = MinimalGrayUI()
    window.show()
    sys.exit(app.exec())
