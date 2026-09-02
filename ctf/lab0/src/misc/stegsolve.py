# python "c:\Users\35418\Desktop\CTF\lab0\src\stegsolve.py"
from PIL import Image
import numpy as np

img = Image.open(r'C:\Users\35418\.easyclaw\media\inbound\4fe5e169-185b-4296-b370-ffa683e3dd6d.png')
pixels = np.array(img)
h, w = pixels.shape[:2]

for ch_name, ch in [('R', 0), ('G', 1), ('B', 2)]:
    lsb_img = np.zeros((h, w), dtype=np.uint8)
    for r in range(h):
        for c in range(w):
            lsb_img[r, c] = (pixels[r, c, ch] & 1) * 255
    
    Image.fromarray(lsb_img, 'L').save(f'lsb_{ch_name}.png')

