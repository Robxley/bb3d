import struct
import json

def dump_glb(filename):
    with open(filename, 'rb') as f:
        magic = f.read(4)
        if magic != b'glTF':
            return
        f.read(8) # Skip version and length
        chunk_length = struct.unpack('<I', f.read(4))[0]
        f.read(4) # Skip chunk type
        json_data = f.read(chunk_length).decode('utf-8')
        data = json.loads(json_data)
        
        print("Meshes:")
        for mesh in data.get('meshes', []):
            print(f"  - {mesh.get('name')}")
            for prim in mesh.get('primitives', []):
                print(f"    Prim: {prim.get('attributes')}")
        
        print("\nAccessors:")
        for i, acc in enumerate(data.get('accessors', [])):
            print(f"  {i}: {acc}")

if __name__ == "__main__":
    dump_glb("assets/models/ant.glb")