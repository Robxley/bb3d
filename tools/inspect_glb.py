import struct
import json
import argparse
import os
import sys

def dump_glb(filename):
    if not os.path.exists(filename):
        print(f"Error: File '{filename}' not found.")
        return

    with open(filename, 'rb') as f:
        magic = f.read(4)
        if magic != b'glTF':
            print("Error: Not a valid GLB file (magic header mismatch).")
            return
        
        f.read(8) # Skip version and length
        chunk_length = struct.unpack('<I', f.read(4))[0]
        chunk_type = f.read(4) # Skip chunk type
        
        if chunk_type != b'JSON':
            print("Error: First chunk is not JSON.")
            return

        json_data = f.read(chunk_length).decode('utf-8')
        data = json.loads(json_data)
        
        print(f"--- Inspection of {filename} ---")
        print("Meshes:")
        for mesh in data.get('meshes', []):
            print(f"  - {mesh.get('name', 'Unnamed Mesh')}")
            for prim in mesh.get('primitives', []):
                print(f"    Prim: {prim.get('attributes')}")
        
        print("\nAccessors:")
        for i, acc in enumerate(data.get('accessors', [])):
            print(f"  {i}: {acc}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Inspect GLB file structure.")
    parser.add_argument("-in", "--input", dest="input_file", required=True, help="Path to the .glb file to inspect")
    
    args = parser.parse_args()
    dump_glb(args.input_file)
