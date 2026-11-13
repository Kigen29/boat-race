"""
Imports the Higgsfield assets into /Game/Higgsfield and builds the water material.

Runs inside Unreal Engine's own Python (the PythonScriptPlugin that ships with
the engine), either headless through Scripts/import_assets.sh or from the editor
via Tools > Execute Python Script.

Produces the asset names the game looks for at startup:
    /Game/Higgsfield/SM_Boat        static mesh  (from SM_Boat.glb)
    /Game/Higgsfield/T_Title        texture      (from T_Title.png)
    /Game/Higgsfield/T_WaterNormal  texture      (from T_WaterNormal.png)
    /Game/Higgsfield/M_Water        material     (built here from T_WaterNormal)
    /Game/Higgsfield/M_Explosion    material     (built here, for blasts)

It also imports the Blender models (Art/Models/*.glb, from Scripts/make_models.sh)
as /Game/Models/SM_<Name>, and builds the materials the scenery and effects use:
    /Game/Models/M_VertexColor   lit, coloured per vertex (the land)
    /Game/Models/M_Glow          unlit emissive with a Color parameter (lamps)
    /Game/Models/M_Soft          translucent puff with Color and Opacity (smoke,
                                 spray, rain and snow)

Every step is independent: if one fails it is reported and the rest carry on,
and the game falls back to its built-in placeholder for whatever is missing.
"""

import os
import traceback

import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE_DIR = os.path.join(PROJECT_DIR, "HiggsfieldAssets")
DEST = "/Game/Higgsfield"
MODELS_SOURCE = os.path.join(PROJECT_DIR, "Art", "Models")
MODELS_DEST = "/Game/Models"
BOAT_IMPORT_DIR = DEST + "/BoatImport"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
editor_assets = unreal.EditorAssetLibrary
materials = unreal.MaterialEditingLibrary


def log(message):
    unreal.log("[BoatRace import] " + message)


def warn(message):
    unreal.log_warning("[BoatRace import] " + message)


def import_file(filename, destination_path, destination_name=None, source_dir=None):
    """Import one file with default settings. Returns the imported object paths."""
    path = os.path.join(source_dir or SOURCE_DIR, filename)
    if not os.path.isfile(path):
        warn("missing file, skipped: " + path)
        return []

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", destination_path)
    if destination_name:
        task.set_editor_property("destination_name", destination_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    asset_tools.import_asset_tasks([task])

    imported = [str(p) for p in task.get_editor_property("imported_object_paths")]
    log("imported %s -> %s" % (filename, imported if imported else "(nothing reported)"))
    return imported


def import_texture(filename, name, is_normal_map):
    import_file(filename, DEST, name)
    asset_path = "%s/%s" % (DEST, name)
    texture = editor_assets.load_asset(asset_path)
    if texture is None:
        warn("texture did not import: " + asset_path)
        return None
    if is_normal_map:
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("srgb", False)
        editor_assets.save_asset(asset_path)
    return texture


def import_boat():
    """glTF import names meshes after the nodes inside the file, so import into
    a sub-folder, pick the biggest static mesh and publish it as SM_Boat."""
    import_file("SM_Boat.glb", BOAT_IMPORT_DIR)

    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([BOAT_IMPORT_DIR], True)

    best_mesh = None
    best_size = -1.0
    for asset_data in registry.get_assets_by_path(BOAT_IMPORT_DIR, recursive=True):
        asset = asset_data.get_asset()
        if isinstance(asset, unreal.StaticMesh):
            extent = asset.get_bounds().box_extent
            size = extent.x * extent.y * extent.z
            if size > best_size:
                best_mesh, best_size = asset, size

    if best_mesh is None:
        warn("no static mesh came out of SM_Boat.glb; the game will use the placeholder boat")
        return

    target = DEST + "/SM_Boat"
    source = best_mesh.get_path_name().split(".")[0]
    if source != target:
        if editor_assets.does_asset_exist(target):
            editor_assets.delete_asset(target)
        if editor_assets.duplicate_asset(source, target) is None:
            warn("could not publish %s as %s" % (source, target))
            return
    editor_assets.save_asset(target)
    log("boat model ready: " + target)


def build_water_material(normal_texture):
    """Dark, glossy water whose ripples are two copies of the Higgsfield normal
    map drifting across each other at different sizes. With Lumen reflections it
    mirrors the sky, sun and clouds."""
    name = "M_Water"
    path = "%s/%s" % (DEST, name)
    if editor_assets.does_asset_exist(path):
        editor_assets.delete_asset(path)

    material = asset_tools.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        warn("could not create " + path)
        return

    def node(expression_class, x, y):
        return materials.create_material_expression(material, expression_class, x, y)

    def constant(value, x, y):
        expression = node(unreal.MaterialExpressionConstant, x, y)
        expression.set_editor_property("r", value)
        return expression

    def colour(r, g, b, x, y):
        expression = node(unreal.MaterialExpressionConstant3Vector, x, y)
        expression.set_editor_property("constant", unreal.LinearColor(r, g, b, 1.0))
        return expression

    # Tile by world position so the pattern is the same size however big the plane is.
    world = node(unreal.MaterialExpressionWorldPosition, -1700, 0)
    world_xy = node(unreal.MaterialExpressionComponentMask, -1500, 0)
    world_xy.set_editor_property("r", True)
    world_xy.set_editor_property("g", True)
    world_xy.set_editor_property("b", False)
    world_xy.set_editor_property("a", False)
    materials.connect_material_expressions(world, "", world_xy, "")

    def ripple_layer(tile_size_cm, speed_x, speed_y, y):
        size = constant(tile_size_cm, -1500, y + 140)
        uv = node(unreal.MaterialExpressionDivide, -1300, y)
        materials.connect_material_expressions(world_xy, "", uv, "A")
        materials.connect_material_expressions(size, "", uv, "B")

        panner = node(unreal.MaterialExpressionPanner, -1100, y)
        panner.set_editor_property("speed_x", speed_x)
        panner.set_editor_property("speed_y", speed_y)
        materials.connect_material_expressions(uv, "", panner, "Coordinate")

        sample = node(unreal.MaterialExpressionTextureSample, -900, y)
        sample.set_editor_property("texture", normal_texture)
        sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        materials.connect_material_expressions(panner, "", sample, "UVs")
        return sample

    fine = ripple_layer(1100.0, 0.035, 0.012, -260)     # 11 m tiles: wind ripples
    broad = ripple_layer(4700.0, -0.012, 0.019, 160)    # 47 m tiles: swell

    combined = node(unreal.MaterialExpressionAdd, -650, -60)
    materials.connect_material_expressions(fine, "RGB", combined, "A")
    materials.connect_material_expressions(broad, "RGB", combined, "B")

    # Soften the slopes (x, y) and re-normalise.
    softness = colour(0.45, 0.45, 1.0, -650, 120)
    softened = node(unreal.MaterialExpressionMultiply, -450, 0)
    materials.connect_material_expressions(combined, "", softened, "A")
    materials.connect_material_expressions(softness, "", softened, "B")
    normal = node(unreal.MaterialExpressionNormalize, -250, 0)
    materials.connect_material_expressions(softened, "", normal, "")
    materials.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

    # Colour: the weather's deep-water tint (the "Color" parameter), lightened
    # towards the crests where light shines through the thin water, as in a
    # real tropical sea; then foam where the game marks it in the vertex
    # colour (along the shore) and on the steepest crests.
    g = Graph(material)
    deep = g.vparam("Color", 0.006, 0.02, 0.025)
    height = g.node(unreal.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False)
    g.link(g.node(unreal.MaterialExpressionWorldPosition), height)
    crest = g.sat(g.mul(height, g.const(1.0 / 90.0)))
    shallow = g.add(g.mul(deep, g.const(3.2)), g.vec(0.0, 0.035, 0.03))
    water = g.lerp(deep, shallow, g.mul(crest, g.const(0.65)))
    # Towards the horizon water reflects the sky: blend to the "Sky" colour
    # (set per weather by the game) as the view grazes the surface.
    fres = g.node(unreal.MaterialExpressionFresnel, exponent=5.0, base_reflect_fraction=0.02)
    water = g.lerp(water, g.vparam("Sky", 0.35, 0.5, 0.65), g.mul(fres, g.const(0.7)))
    n1, n2 = g.noise(0.006), g.noise(0.04)
    patches = g.sat(g.mul(g.sub(g.add(g.mul(n1, g.const(0.5)), g.mul(n2, g.const(0.5))), g.const(0.35)), g.const(3.0)))
    vertex = g.node(unreal.MaterialExpressionVertexColor)
    shore = g.node(unreal.MaterialExpressionMultiply)
    materials.connect_material_expressions(vertex, "R", shore, "A")
    g.link(g.sparam("FoamFromVertex", 1.0), shore, "B")
    crest_foam = g.sat(g.mul(g.sub(crest, g.const(0.8)), g.const(4.0)))
    foam = g.mul(g.sat(g.add(shore, crest_foam)), patches)
    g.out(g.lerp(water, g.vec(0.8, 0.84, 0.86), foam), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.lerp(g.const(0.04), g.const(0.55), foam), unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(g.const(0.5), unreal.MaterialProperty.MP_SPECULAR)

    materials.recompile_material(material)
    editor_assets.save_asset(path)
    log("water material ready: " + path)


def build_explosion_material():
    """An unlit, additive blob used for the explosion flash and its shockwave.
    Without it the blasts still show, just as solid orange spheres."""
    name = "M_Explosion"
    path = "%s/%s" % (DEST, name)
    if editor_assets.does_asset_exist(path):
        editor_assets.delete_asset(path)

    material = asset_tools.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        warn("could not create " + path)
        return

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    colour = materials.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -400, 0)
    colour.set_editor_property("parameter_name", "Color")
    colour.set_editor_property("default_value", unreal.LinearColor(6.0, 2.2, 0.3, 1.0))

    # Brightest at the centre, fading at the silhouette, so it reads as a ball
    # of light rather than a hard-edged sphere.
    fresnel = materials.create_material_expression(material, unreal.MaterialExpressionFresnel, -400, 200)
    fresnel.set_editor_property("exponent", 1.6)
    invert = materials.create_material_expression(material, unreal.MaterialExpressionOneMinus, -250, 200)
    materials.connect_material_expressions(fresnel, "", invert, "")

    glow = materials.create_material_expression(material, unreal.MaterialExpressionMultiply, -150, 60)
    materials.connect_material_expressions(colour, "", glow, "A")
    materials.connect_material_expressions(invert, "", glow, "B")

    materials.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    materials.connect_material_property(invert, "", unreal.MaterialProperty.MP_OPACITY)

    materials.recompile_material(material)
    editor_assets.save_asset(path)
    log("explosion material ready: " + path)


def publish_biggest_mesh(folder, target):
    """Interchange names meshes after the nodes in the file, so pick the
    biggest static mesh it made and publish it under the name the game uses."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([folder], True)
    best_mesh, best_size = None, -1.0
    for asset_data in registry.get_assets_by_path(folder, recursive=True):
        asset = asset_data.get_asset()
        if isinstance(asset, unreal.StaticMesh):
            extent = asset.get_bounds().box_extent
            size = extent.x * extent.y * extent.z
            if size > best_size:
                best_mesh, best_size = asset, size
    if best_mesh is None:
        warn("no static mesh came out of " + folder)
        return False
    source = best_mesh.get_path_name().split(".")[0]
    if source != target:
        if editor_assets.does_asset_exist(target):
            editor_assets.delete_asset(target)
        if editor_assets.duplicate_asset(source, target) is None:
            warn("could not publish %s as %s" % (source, target))
            return False
    editor_assets.save_asset(target)
    return True


def import_models():
    if not os.path.isdir(MODELS_SOURCE):
        warn("no Art/Models folder (run Scripts/make_models.sh); the game will use simple shapes")
        return
    done = 0
    for filename in sorted(os.listdir(MODELS_SOURCE)):
        if not filename.lower().endswith(".glb"):
            continue
        name = os.path.splitext(filename)[0]
        folder = "%s/Import/%s" % (MODELS_DEST, name)
        if editor_assets.does_directory_exist(folder):
            editor_assets.delete_directory(folder)
        import_file(filename, folder, source_dir=MODELS_SOURCE)
        if publish_biggest_mesh(folder, "%s/SM_%s" % (MODELS_DEST, name)):
            done += 1
    log("models ready: %d in %s" % (done, MODELS_DEST))


def new_material(name, folder=None):
    folder = folder or MODELS_DEST
    path = "%s/%s" % (folder, name)
    if editor_assets.does_asset_exist(path):
        editor_assets.delete_asset(path)
    material = asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        warn("could not create " + path)
        return None, path
    # Rain, snow, clouds, rope floats and trees are drawn as instances. A
    # packaged game cannot compile a material for a use it was not flagged
    # for, and draws the grey default material instead.
    for flag in ("used_with_instanced_static_meshes", "used_with_static_lighting"):
        try:
            material.set_editor_property(flag, True)
        except Exception:
            pass
    return material, path


TEXTURES_SOURCE = os.path.join(PROJECT_DIR, "Art", "Textures")


class Graph:
    """Small helpers for wiring material graphs in a readable way."""

    def __init__(self, material):
        self.m = material
        self.x = -1200

    def node(self, cls, **props):
        self.x += 40
        n = materials.create_material_expression(self.m, cls, self.x, 0)
        for key, value in props.items():
            n.set_editor_property(key, value)
        return n

    def link(self, a, b, pin="", out=""):
        materials.connect_material_expressions(a, out, b, pin)

    def const(self, value):
        return self.node(unreal.MaterialExpressionConstant, r=value)

    def vec(self, r, g, b):
        return self.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(r, g, b, 1.0))

    def vparam(self, name, r, g, b):
        return self.node(unreal.MaterialExpressionVectorParameter, parameter_name=name,
                         default_value=unreal.LinearColor(r, g, b, 1.0))

    def sparam(self, name, value):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=value)

    def op(self, cls, a, b):
        n = self.node(cls)
        self.link(a, n, "A")
        self.link(b, n, "B")
        return n

    def mul(self, a, b):
        return self.op(unreal.MaterialExpressionMultiply, a, b)

    def add(self, a, b):
        return self.op(unreal.MaterialExpressionAdd, a, b)

    def sub(self, a, b):
        return self.op(unreal.MaterialExpressionSubtract, a, b)

    def lerp(self, a, b, alpha):
        n = self.node(unreal.MaterialExpressionLinearInterpolate)
        self.link(a, n, "A")
        self.link(b, n, "B")
        self.link(alpha, n, "Alpha")
        return n

    def sat(self, a):
        n = self.node(unreal.MaterialExpressionSaturate)
        self.link(a, n)
        return n

    def noise(self, scale):
        # Scale multiplies world position: 0.01 gives features about a metre across.
        return self.node(unreal.MaterialExpressionNoise, scale=scale, levels=4, output_min=0.0, output_max=1.0)

    def out(self, node, prop, pin=""):
        materials.connect_material_property(node, pin, prop)


def finish(material, path, label):
    materials.recompile_material(material)
    editor_assets.save_asset(path)
    log("%s ready: %s" % (label, path))


def build_realistic_materials():
    """Clear-coat paint, livery, general surfaces with grain, glass and foam."""
    P = unreal.MaterialProperty

    # Grain: two octaves of world-space noise that break up flat colour on
    # rock, wood, concrete and canvas. 0 on paint and chrome.
    def grain(g, colour, amount):
        n1, n2 = g.noise(0.004), g.noise(0.03)
        mix = g.add(g.mul(n1, g.const(0.6)), g.mul(n2, g.const(0.4)))
        centred = g.sub(mix, g.const(0.5))
        return g.mul(colour, g.add(g.const(1.0), g.mul(centred, g.mul(amount, g.const(0.9))))), n2

    material, path = new_material("M_Surface")
    if material is not None:
        g = Graph(material)
        colour, amount = g.vparam("Color", 0.5, 0.5, 0.5), g.sparam("Grain", 0.0)
        base, fine = grain(g, colour, amount)
        g.out(base, P.MP_BASE_COLOR)
        rough = g.add(g.sparam("Roughness", 0.6), g.mul(g.sub(fine, g.const(0.5)), g.mul(amount, g.const(0.3))))
        g.out(rough, P.MP_ROUGHNESS)
        g.out(g.sparam("Metallic", 0.0), P.MP_METALLIC)
        g.out(g.sparam("Specular", 0.5), P.MP_SPECULAR)
        finish(material, path, "surface material")

    # Gelcoat and paint: a glossy clear coat over the colour, as on a real hull.
    material, path = new_material("M_Paint")
    if material is not None:
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLEAR_COAT)
        g = Graph(material)
        g.out(g.vparam("Color", 0.8, 0.1, 0.1), P.MP_BASE_COLOR)
        g.out(g.sparam("Metallic", 0.15), P.MP_METALLIC)
        g.out(g.sparam("Roughness", 0.35), P.MP_ROUGHNESS)
        g.out(g.const(1.0), P.MP_CUSTOM_DATA0)
        g.out(g.sparam("CoatRoughness", 0.03), P.MP_CUSTOM_DATA1)
        finish(material, path, "paint material")

    # Livery: a mask texture (R own colour, G accent, B white) under a clear coat.
    for tex_name in ("T_Livery", "T_LiveryCruise"):
        if os.path.isfile(os.path.join(TEXTURES_SOURCE, tex_name + ".png")):
            import_file(tex_name + ".png", MODELS_DEST, tex_name, source_dir=TEXTURES_SOURCE)
            texture = editor_assets.load_asset("%s/%s" % (MODELS_DEST, tex_name))
            if texture is not None:
                texture.set_editor_property("srgb", False)
                texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
                editor_assets.save_asset("%s/%s" % (MODELS_DEST, tex_name))
    mask = editor_assets.load_asset(MODELS_DEST + "/T_Livery")
    material, path = new_material("M_Livery")
    if material is not None and mask is not None:
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLEAR_COAT)
        g = Graph(material)
        sample = g.node(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name="Mask", texture=mask,
                        sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        own = g.mul(g.vparam("Color", 0.8, 0.1, 0.1), sample)
        materials.connect_material_expressions(sample, "R", own, "B")
        accent = g.node(unreal.MaterialExpressionMultiply)
        g.link(g.vparam("Accent", 0.02, 0.02, 0.025), accent, "A")
        materials.connect_material_expressions(sample, "G", accent, "B")
        white = g.node(unreal.MaterialExpressionMultiply)
        g.link(g.vparam("White", 0.85, 0.86, 0.88), white, "A")
        materials.connect_material_expressions(sample, "B", white, "B")
        g.out(g.add(g.add(own, accent), white), P.MP_BASE_COLOR)
        g.out(g.sparam("Metallic", 0.25), P.MP_METALLIC)
        g.out(g.sparam("Roughness", 0.3), P.MP_ROUGHNESS)
        g.out(g.const(1.0), P.MP_CUSTOM_DATA0)
        g.out(g.const(0.03), P.MP_CUSTOM_DATA1)
        finish(material, path, "livery material")

    # Tinted glass: see-through head-on, mirror-like at a glancing angle.
    material, path = new_material("M_Glass")
    if material is not None:
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        material.set_editor_property("two_sided", True)
        try:
            material.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
        except Exception:
            pass
        g = Graph(material)
        g.out(g.vparam("Color", 0.05, 0.08, 0.1), P.MP_BASE_COLOR)
        g.out(g.const(0.05), P.MP_ROUGHNESS)
        g.out(g.const(1.0), P.MP_SPECULAR)
        fres = g.node(unreal.MaterialExpressionFresnel, exponent=3.0)
        base = g.sparam("Opacity", 0.4)
        g.out(g.lerp(base, g.const(0.95), fres), P.MP_OPACITY)
        finish(material, path, "glass material")

    # Foam: white water broken into patches by noise; the game fades it with
    # vertex alpha (wake trails, bow spray) and the Opacity parameter.
    material, path = new_material("M_Foam")
    if material is not None:
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        material.set_editor_property("two_sided", True)
        g = Graph(material)
        n1, n2 = g.noise(0.008), g.noise(0.05)
        patches = g.sat(g.mul(g.sub(g.add(g.mul(n1, g.const(0.55)), g.mul(n2, g.const(0.45))), g.const(0.3)), g.const(2.6)))
        vertex = g.node(unreal.MaterialExpressionVertexColor)
        alpha = g.node(unreal.MaterialExpressionMultiply)
        materials.connect_material_expressions(vertex, "A", alpha, "A")
        g.link(g.sparam("Opacity", 0.8), alpha, "B")
        g.out(g.mul(alpha, patches), P.MP_OPACITY)
        g.out(g.mul(g.vparam("Color", 1.0, 1.0, 1.0), g.add(g.const(0.85), g.mul(n2, g.const(0.15)))), P.MP_EMISSIVE_COLOR)
        finish(material, path, "foam material")


def build_scenery_materials():
    # Land: the colour is baked into the mesh per vertex by BayTerrain.cpp.
    material, path = new_material("M_VertexColor")
    if material is not None:
        vertex = materials.create_material_expression(material, unreal.MaterialExpressionVertexColor, -400, 0)
        # Break the baked colour up with world-space noise: patches of lighter
        # and darker grass, streaks in the rock, ripples in the sand.
        g = Graph(material)
        broad, fine = g.noise(0.0035), g.noise(0.027)
        shade = g.mul(g.add(g.const(0.87), g.mul(broad, g.const(0.26))), g.add(g.const(0.95), g.mul(fine, g.const(0.1))))
        tinted = g.node(unreal.MaterialExpressionMultiply)
        materials.connect_material_expressions(vertex, "", tinted, "A")
        g.link(shade, tinted, "B")
        materials.connect_material_property(tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)
        rough = materials.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 200)
        rough.set_editor_property("r", 0.85)
        materials.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
        materials.recompile_material(material)
        editor_assets.save_asset(path)
        log("terrain material ready: " + path)

    # Lamps, windows at night, start lights.
    material, path = new_material("M_Glow")
    if material is not None:
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        colour = materials.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -400, 0)
        colour.set_editor_property("parameter_name", "Color")
        colour.set_editor_property("default_value", unreal.LinearColor(4.0, 3.0, 1.0, 1.0))
        materials.connect_material_property(colour, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        materials.recompile_material(material)
        editor_assets.save_asset(path)
        log("glow material ready: " + path)

    # Smoke, spray, rain and snow: soft-edged translucent puffs whose colour
    # and opacity the game animates.
    material, path = new_material("M_Soft")
    if material is not None:
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        material.set_editor_property("two_sided", True)
        colour = materials.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -500, -100)
        colour.set_editor_property("parameter_name", "Color")
        colour.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
        opacity = materials.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, 150)
        opacity.set_editor_property("parameter_name", "Opacity")
        opacity.set_editor_property("default_value", 0.6)
        fresnel = materials.create_material_expression(material, unreal.MaterialExpressionFresnel, -500, 300)
        fresnel.set_editor_property("exponent", 1.5)
        soft = materials.create_material_expression(material, unreal.MaterialExpressionOneMinus, -330, 300)
        materials.connect_material_expressions(fresnel, "", soft, "")
        alpha = materials.create_material_expression(material, unreal.MaterialExpressionMultiply, -180, 200)
        materials.connect_material_expressions(opacity, "", alpha, "A")
        materials.connect_material_expressions(soft, "", alpha, "B")
        materials.connect_material_property(colour, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        materials.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)
        materials.recompile_material(material)
        editor_assets.save_asset(path)
        log("soft material ready: " + path)


def run_step(label, step):
    try:
        return step()
    except Exception:
        warn("%s FAILED:\n%s" % (label, traceback.format_exc()))
        return None


def main():
    run_step("Blender models", import_models)
    run_step("scenery materials", build_scenery_materials)
    run_step("realistic materials", build_realistic_materials)
    run_step("save models", lambda: editor_assets.save_directory(MODELS_DEST, False, True))

    if not os.path.isdir(SOURCE_DIR):
        warn("no HiggsfieldAssets folder at %s (run Scripts/fetch_assets.sh)" % SOURCE_DIR)
        return

    run_step("boat model", import_boat)
    run_step("title image", lambda: import_texture("T_Title.png", "T_Title", False))
    normal_texture = run_step("water normal map", lambda: import_texture("T_WaterNormal.png", "T_WaterNormal", True))
    if normal_texture is not None:
        run_step("water material", lambda: build_water_material(normal_texture))
    else:
        warn("no water normal map, so M_Water was not built; the game will use plain water")

    run_step("explosion material", build_explosion_material)

    run_step("save", lambda: editor_assets.save_directory(DEST, False, True))
    log("done")


main()
