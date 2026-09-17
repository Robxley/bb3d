import os
import sys

def flip_winding_order(file_path):
    out_path = file_path.replace('.obj', '_ccw.obj')
    with open(file_path, 'r') as f:
        lines = f.readlines()
        
    with open(out_path, 'w') as f:
        for line in lines:
            line = line.strip()
            if line.startswith('f '):
                parts = line.split()
                # Reverse the elements after 'f'
                reversed_faces = parts[1:][::-1]
                f.write('f ' + ' '.join(reversed_faces) + '\n')
            else:
                f.write(line + '\n')
    print(f"Flipped {file_path} -> {out_path}")

planes = [
    "assets/models/planes/Plane01/Plane01.obj",
    "assets/models/planes/Plane02/Plane02.obj",
    "assets/models/planes/Plane03/Plane03.obj",
    "assets/models/planes/Plane05/Plane05.obj",
    "assets/models/planes/Plane06/Plane06.obj"
]

for p in planes:
    if os.path.exists(p):
        flip_winding_order(p)
