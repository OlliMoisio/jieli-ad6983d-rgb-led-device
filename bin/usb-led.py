import hid
import time
from enum import IntEnum

# USB HID device VID and PID
VENDOR_ID = 0x4C4F 
PRODUCT_ID = 0x494C

class LedPowerState(IntEnum):
    OFF = 0
    ON = 1
    BLINKING = 2

# function to create the 4-byte packet
def set_led(index: int, state: LedPowerState, red: int, green: int, blue: int) -> bytes:
    """
    creates a 4-byte sequence for a specific LED.
    byte 0: [1] [state:2bits] [index:5bits]
    byte 1: Red
    byte 2: Green
    byte 3: Blue
    """
    
    # make sure values fit in their assigned bit widths
    index = index & 0x1F  # force index to be 0-31 (5 bits)
    red = red & 0xFF      # force color to 0-255
    green = green & 0xFF
    blue = blue & 0xFF
    
    # creating the header byte
    # bit 7:     always 1 (0x80 or 128)
    # bits 5-6:  power state (shifted left by 5)
    # bits 0-4:  index
    header = 0x80 | ((state & 0x03) << 5) | index

    # return as immutable bytes object
    return bytes([header, red, green, blue])
    

def send_led_data(dev, data):
    
    data_length = len(data)
    
    if (data_length <= 64):
        dev.write([0x00] + data)
    

# print response received through USB from the device
def print_response(dev):
    
    data_received = dev.read(64, 100) # read 64 bytes, 100 ms timeout
            
    if data_received:
        # only print the first byte
        print(f"Received response code: {data_received[0]}")
    else:
        print("data receive timeout")
    

def main():
    
    print(f"Opening device VID:0x{VENDOR_ID:04x} PID:0x{PRODUCT_ID:04x}...")
    
    try:
        dev = hid.device()
        dev.open(VENDOR_ID, PRODUCT_ID)
        print("Device opened successfully!")
        
        led_data = []
        
        led_data += set_led(0, LedPowerState.ON, 255, 0, 0)
        led_data += set_led(1, LedPowerState.BLINKING, 0, 255, 0)
        led_data += set_led(3, LedPowerState.BLINKING, 128, 128, 0)
        led_data += set_led(29, LedPowerState.BLINKING, 128, 0, 128)
        led_data += set_led(31, LedPowerState.ON, 0, 0, 255)
        
        send_led_data(dev, led_data)
        
        # optional, print received response
        # print_response(dev)

    except Exception as e:
        print(f"Error: {e}")
        
    finally:
        dev.close()

if __name__ == "__main__":
    main()
