import os
import sys
import time
import mss
import mss.tools
import pygetwindow as gw

def capture(window_title, delay_sec, output_path):
    print(f"Waiting for window: '{window_title}'...")
    window = None
    timeout = 30
    start_time = time.time()
    
    while time.time() - start_time < timeout:
        windows = gw.getWindowsWithTitle(window_title)
        if windows:
            window = windows[0]
            break
        time.sleep(0.5)
        
    if not window:
        print(f"Error: Window '{window_title}' not found after {timeout} seconds.", file=sys.stderr)
        sys.exit(1)
        
    print(f"Window found! '{window.title}' at {window.left}, {window.top} ({window.width}x{window.height})")
    
    try:
        window.activate()
    except Exception as e:
        print(f"Warning: could not activate window: {e}")
        
    print(f"Waiting {delay_sec} seconds for scene to load...")
    time.sleep(delay_sec)
    
    rect = {"top": window.top, "left": window.left, "width": window.width, "height": window.height}
    print(f"Capturing region: {rect} to {output_path}")
    
    with mss.mss() as sct:
        im = sct.grab(rect)
        mss.tools.to_png(im.rgb, im.size, output=output_path)
    print(f"Done! Saved to {output_path}")
    
    print("Killing the application...")
    os.system("taskkill /F /IM unit_test_procedural_planet.exe >nul 2>&1")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: python capture_window.py <window_title> <delay_seconds> <output_path>")
        sys.exit(1)
        
    capture(sys.argv[1], int(sys.argv[2]), sys.argv[3])
