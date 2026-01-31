import bpy
import os

# 1. Clean scene
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete()

# Dimensions
WIDTH = 10
DEPTH = 8
WALL_HEIGHT = 3
ROOF_HEIGHT = 2 

# Paths
BASE_ASSET_PATH = "C:/dev/bb3d/assets/PBR/"
WOOD_PATH = BASE_ASSET_PATH + "Wood066/"
TILES_PATH = BASE_ASSET_PATH + "Tiles008/"

def create_pbr_material(name, base_color_path, normal_path=None, roughness_path=None, ao_path=None):
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()

    # Principal BSDF
    node_principled = nodes.new(type='ShaderNodeBsdfPrincipled')
    node_principled.location = (0, 0)

    # Output
    node_output = nodes.new(type='ShaderNodeOutputMaterial')
    node_output.location = (400, 0)
    links.new(node_principled.outputs['BSDF'], node_output.inputs['Surface'])

    # Albedo
    if base_color_path and os.path.exists(base_color_path):
        node_albedo = nodes.new(type='ShaderNodeTexImage')
        node_albedo.image = bpy.data.images.load(base_color_path)
        node_albedo.location = (-400, 300)
        links.new(node_albedo.outputs['Color'], node_principled.inputs['Base Color'])

    # Normal
    if normal_path and os.path.exists(normal_path):
        node_normal_map = nodes.new(type='ShaderNodeNormalMap')
        node_normal_map.location = (-200, -200)
        
        node_normal_tex = nodes.new(type='ShaderNodeTexImage')
        node_normal_tex.image = bpy.data.images.load(normal_path)
        node_normal_tex.image.colorspace_settings.name = 'Non-Color'
        node_normal_tex.location = (-500, -200)
        
        links.new(node_normal_tex.outputs['Color'], node_normal_map.inputs['Color'])
        links.new(node_normal_map.outputs['Normal'], node_principled.inputs['Normal'])

    # Roughness
    if roughness_path and os.path.exists(roughness_path):
        node_rough = nodes.new(type='ShaderNodeTexImage')
        node_rough.image = bpy.data.images.load(roughness_path)
        node_rough.image.colorspace_settings.name = 'Non-Color'
        node_rough.location = (-400, 0)
        links.new(node_rough.outputs['Color'], node_principled.inputs['Roughness'])

    return mat

# Create Materials
wood_mat = create_pbr_material("WoodMat", 
    WOOD_PATH + "Wood066_1K-JPG_Color.jpg",
    WOOD_PATH + "Wood066_1K-JPG_NormalGL.jpg",
    WOOD_PATH + "Wood066_1K-JPG_Roughness.jpg")

tiles_mat = create_pbr_material("RoofMat", 
    TILES_PATH + "Tiles008_1K-JPG_Color.jpg",
    TILES_PATH + "Tiles008_1K-JPG_NormalGL.jpg",
    TILES_PATH + "Tiles008_1K-JPG_Roughness.jpg",
    TILES_PATH + "Tiles008_1K-JPG_AmbientOcclusion.jpg")

# 2. House Body
bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, WALL_HEIGHT / 2))
house = bpy.context.active_object
house.name = "HouseBody"
house.scale = (WIDTH, DEPTH, WALL_HEIGHT)
bpy.ops.object.transform_apply(scale=True)
house.data.materials.append(wood_mat)

# UV Unwrap
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project()
bpy.ops.object.mode_set(mode='OBJECT')

# 3. Improved Ridge Roof (Gable Roof)
bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, WALL_HEIGHT + ROOF_HEIGHT / 2))
roof = bpy.context.active_object
roof.name = "HouseRoof"
roof.scale = (WIDTH + 0.5, DEPTH + 0.5, ROOF_HEIGHT) # Slight overhang
bpy.ops.object.transform_apply(scale=True)

bpy.ops.object.mode_set(mode='EDIT')
import bmesh
bm = bmesh.from_edit_mesh(roof.data)
bm.verts.ensure_lookup_table()
top_verts = [v for v in bm.verts if v.co.z > 0.1]
for v in top_verts:
    v.co.y = 0
bmesh.ops.remove_doubles(bm, verts=top_verts, dist=0.001)
bmesh.update_edit_mesh(roof.data)

# UV Unwrap Roof
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project()
bpy.ops.object.mode_set(mode='OBJECT')
roof.data.materials.append(tiles_mat)

# 4. Details
# Door
bpy.ops.mesh.primitive_cube_add(size=1, location=(0, -DEPTH/2, 1))
door = bpy.context.active_object
door.scale = (1.5, 0.1, 2)
door.data.materials.append(create_pbr_material("DoorMat", None)) # Placeholder

# Window
bpy.ops.mesh.primitive_cube_add(size=1, location=(WIDTH/4, -DEPTH/2, 2))
window = bpy.context.active_object
window.scale = (1.5, 0.1, 1.5)
window.data.materials.append(create_pbr_material("GlassMat", None)) # Placeholder

# 5. Export
output_path = os.path.abspath("assets/models/house.glb")
os.makedirs(os.path.dirname(output_path), exist_ok=True)
bpy.ops.export_scene.gltf(filepath=output_path, export_format='GLB')

print(f"SUCCESS: {output_path}")