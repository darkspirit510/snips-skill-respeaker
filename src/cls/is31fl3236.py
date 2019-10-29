from .run_params import RunParams
from .animation import Animation
import smbus
import time

class IS31FL3236:
    addr = 0x3F
    i2c = smbus.SMBus(1)
    run_params = RunParams()
    animation = Animation()
    
    def refresh(self):
        self.send_byte(0x25, 0x00)
    
    def clear_all(self):
        for register in range(1, 37):
            self.send_byte(register, 0x00)
        self.refresh()
        
    def set_color(self, index, color):
        led_addr = 1 + 3 * index
        self.send_byte(led_addr, color.r)
        self.send_byte(led_addr + 1, color.g)
        self.send_byte(led_addr + 2, color.b)

    def __init__(self):
        self.send_byte(0x4F, 0x00)
        self.send_byte(0x00, 0x01)
        self.send_byte(0x4B, 0x01)
        self.send_byte(0x26, 0x07)
        self.send_byte(0x27, 0x07)
        self.send_byte(0x28, 0x07)
        self.send_byte(0x29, 0x07)
        self.send_byte(0x2A, 0x07)
        self.send_byte(0x2B, 0x07)
        self.send_byte(0x2C, 0x07)
        self.send_byte(0x2D, 0x07)
        self.send_byte(0x2E, 0x07)
        self.send_byte(0x2F, 0x07)
        self.send_byte(0x30, 0x07)
        self.send_byte(0x32, 0x07)
        self.send_byte(0x31, 0x07)
        self.send_byte(0x33, 0x07)
        self.send_byte(0x34, 0x07)
        self.send_byte(0x35, 0x07)
        self.send_byte(0x36, 0x07)
        self.send_byte(0x37, 0x07)
        self.send_byte(0x38, 0x07)
        self.send_byte(0x39, 0x07)
        self.send_byte(0x3A, 0x07)
        self.send_byte(0x3B, 0x07)
        self.send_byte(0x3C, 0x07)
        self.send_byte(0x3D, 0x07)
        self.send_byte(0x3E, 0x07)
        self.send_byte(0x3F, 0x07)
        self.send_byte(0x40, 0x07)
        self.send_byte(0x41, 0x07)
        self.send_byte(0x42, 0x07)
        self.send_byte(0x43, 0x07)
        self.send_byte(0x44, 0x07)
        self.send_byte(0x45, 0x07)
        self.send_byte(0x46, 0x07)
        self.send_byte(0x47, 0x07)
        self.send_byte(0x48, 0x07)
        self.send_byte(0x49, 0x07)
        self.clear_all()

    def send_byte(self, register, data):
        self.i2c.write_byte_data(self.addr, register, data)
    
    def run(self):
        while True:
            if self.run_params.curr_state == "ON_LISTEN":
                self.animation.on_listen(self)
            elif self.run_params.curr_state == "ON_SPEAK":
                self.animation.on_speak(self)
            elif self.run_params.curr_state == "ON_IDLE":
                self.animation.on_idle(self)
