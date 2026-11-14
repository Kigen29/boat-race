"""Builds every 3D object in the game in Blender and exports each as a GLB.

    blender -b --factory-startup -P Scripts/make_models.py -- Art/Models [preview-dir]

Run by Scripts/make_models.sh. The models are procedural rather than sculpted
so the whole art set can be regenerated, tweaked and re-imported from one file.

Conventions the game relies on (BoatPawn.cpp, RaceEnvironment.cpp):
  * metres, +X forward, +Z up, origin at the waterline / ground in the middle;
  * every material is named Part_RRGGBB. The game repaints each slot from that
    hex colour, so the look survives any importer, and the weather can tint it;
  * slots named Paint_* take the player's or the buoy's colour, Lamp*_* glow.
"""
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = os.path.abspath(argv[0] if argv else "Art/Models")
PREVIEW = os.path.abspath(argv[1]) if len(argv) > 1 else None
os.makedirs(OUT, exist_ok=True)


# ---------------------------------------------------------------------------
# Small deterministic noise, so rocks and canopies are lumpy but repeatable
# ---------------------------------------------------------------------------
def hash3(x, y, z, seed):
    n = (int(x) * 73856093) ^ (int(y) * 19349663) ^ (int(z) * 83492791) ^ (seed * 2654435761)
    n = (n ^ (n >> 13)) * 1274126177
    return ((n ^ (n >> 16)) & 0xFFFF) / 65535.0


def vnoise(p, seed, freq):
    x, y, z = p.x * freq, p.y * freq, p.z * freq
    xi, yi, zi = math.floor(x), math.floor(y), math.floor(z)
    fx, fy, fz = x - xi, y - yi, z - zi
    sx, sy, sz = (fx * fx * (3 - 2 * fx)), (fy * fy * (3 - 2 * fy)), (fz * fz * (3 - 2 * fz))
    total = 0.0
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                w = (sx if dx else 1 - sx) * (sy if dy else 1 - sy) * (sz if dz else 1 - sz)
                total += w * hash3(xi + dx, yi + dy, zi + dz, seed)
    return total * 2 - 1


# ---------------------------------------------------------------------------
# Model builder: everything goes into one bmesh with a material per part
# ---------------------------------------------------------------------------
class Model:
    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.mats = []

    def mat(self, name):
        if name not in self.mats:
            self.mats.append(name)
        return self.mats.index(name)

    def _finish(self, verts, mat, xf, smooth):
        if xf is not None:
            bmesh.ops.transform(self.bm, matrix=xf, verts=verts)
        index = self.mat(mat)
        faces = {f for v in verts for f in v.link_faces}
        for f in faces:
            f.material_index = index
            f.smooth = smooth
        return verts

    def box(self, size, loc, mat, rot=(0, 0, 0), smooth=False, bevel=0.0):
        r = bmesh.ops.create_cube(self.bm, size=1.0)
        verts = r["verts"]
        bmesh.ops.scale(self.bm, vec=Vector(size), verts=verts)
        return self._finish(verts, mat, xform(loc, rot), smooth)

    def cyl(self, r1, r2, depth, loc, mat, rot=(0, 0, 0), segs=16, smooth=True):
        r = bmesh.ops.create_cone(self.bm, cap_ends=True, cap_tris=False, segments=segs,
                                  radius1=r1, radius2=r2, depth=depth)
        return self._finish(r["verts"], mat, xform(loc, rot), smooth)

    def sphere(self, radius, loc, mat, scale=(1, 1, 1), rot=(0, 0, 0), segs=16, rings=10, smooth=True):
        r = bmesh.ops.create_uvsphere(self.bm, u_segments=segs, v_segments=rings, radius=radius)
        bmesh.ops.scale(self.bm, vec=Vector(scale), verts=r["verts"])
        return self._finish(r["verts"], mat, xform(loc, rot), smooth)

    def blob(self, radius, loc, mat, scale=(1, 1, 1), lump=0.25, seed=1, subdiv=2, smooth=True, freq=1.6):
        r = bmesh.ops.create_icosphere(self.bm, subdivisions=subdiv, radius=radius)
        for v in r["verts"]:
            n = v.co.normalized()
            v.co = v.co * (1.0 + lump * vnoise(n * 3.0, seed, freq))
        bmesh.ops.scale(self.bm, vec=Vector(scale), verts=r["verts"])
        return self._finish(r["verts"], mat, xform(loc, (0, 0, 0)), smooth)

    def poly_prism(self, points, z0, z1, mat_top, mat_side, mat_bottom=None, smooth=False):
        """Extrudes a closed XY outline between two heights."""
        bot = [self.bm.verts.new((x, y, z0)) for x, y in points]
        top = [self.bm.verts.new((x, y, z1)) for x, y in points]
        n = len(points)
        ft = self.bm.faces.new(top)
        ft.material_index = self.mat(mat_top)
        fb = self.bm.faces.new(list(reversed(bot)))
        fb.material_index = self.mat(mat_bottom or mat_side)
        for i in range(n):
            f = self.bm.faces.new((bot[i], bot[(i + 1) % n], top[(i + 1) % n], top[i]))
            f.material_index = self.mat(mat_side)
            f.smooth = smooth
        return bot + top

    def recolour_up(self, verts, mat, threshold=0.55):
        """Faces of a part that look upwards get another material (moss, snow)."""
        index = self.mat(mat)
        for f in {f for v in verts for f in v.link_faces}:
            f.normal_update()
            if f.normal.z > threshold:
                f.material_index = index

    def export(self):
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces[:])
        mesh = bpy.data.meshes.new(self.name)
        self.bm.to_mesh(mesh)
        self.bm.free()
        for name in self.mats:
            mesh.materials.append(material(name))
        obj = bpy.data.objects.new(self.name, mesh)
        bpy.context.scene.collection.objects.link(obj)
        for other in bpy.context.scene.objects:
            other.select_set(False)
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        path = os.path.join(OUT, self.name + ".glb")
        bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True, export_apply=True)
        dims = obj.dimensions
        print("model %-16s %6d tris  %5.1f x %5.1f x %5.1f m  %d materials" % (
            self.name, sum(len(p.vertices) - 2 for p in mesh.polygons), dims.x, dims.y, dims.z, len(self.mats)))
        return obj


def xform(loc, rot):
    return Matrix.Translation(Vector(loc)) @ Matrix.Rotation(math.radians(rot[2]), 4, "Z") \
        @ Matrix.Rotation(math.radians(rot[1]), 4, "Y") @ Matrix.Rotation(math.radians(rot[0]), 4, "X")


_materials = {}


def material(name):
    if name in _materials:
        return _materials[name]
    hexcode = name.rsplit("_", 1)[-1]
    srgb = [int(hexcode[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    lin = [c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in srgb]
    m = bpy.data.materials.new(name)
    m.diffuse_color = (lin[0], lin[1], lin[2], 1.0)
    try:
        m.use_nodes = True
    except Exception:
        pass
    bsdf = m.node_tree.nodes.get("Principled BSDF") if m.node_tree else None
    if bsdf is not None:
        bsdf.inputs["Base Color"].default_value = (lin[0], lin[1], lin[2], 1.0)
        bsdf.inputs["Roughness"].default_value = 0.15 if name.startswith(("Glass", "Paint")) else 0.7
        if name.startswith(("Lamp", "Glow")):
            for key in ("Emission Color", "Emission"):
                if key in bsdf.inputs:
                    bsdf.inputs[key].default_value = (lin[0], lin[1], lin[2], 1.0)
            if "Emission Strength" in bsdf.inputs:
                bsdf.inputs["Emission Strength"].default_value = 4.0
    _materials[name] = m
    return m


# ---------------------------------------------------------------------------
# The race boat: a 7 m deep-V sport hull, lofted from cross-sections
# ---------------------------------------------------------------------------
def hull_loft(m, length, beam, paint, bottom, deck, stations=None, height=1.0):
    half = length / 2
    xs = stations or [-1.0, -0.86, -0.57, -0.29, 0.0, 0.29, 0.51, 0.71, 0.86, 0.96, 1.0]
    rings = []
    for f in xs:
        x = f * half
        u = max(0.0, (f - 0.14) / 0.86)
        wf = max(0.03, math.cos(min(u, 1.0) * math.pi / 2) ** 0.75)
        w = beam / 2 * wf
        keel = (-0.55 + (max(0.0, (f - 0.28) / 0.72) ** 2) * 0.75) * height
        chine = (-0.28 + (max(0.0, (f - 0.43) / 0.57) ** 2) * 0.45) * height
        gun = (0.45 + (max(0.0, (f - 0.57) / 0.43) ** 2) * 0.18) * height
        rings.append([m.bm.verts.new(c) for c in (
            (x, w, gun), (x, w * 0.93, chine), (x, 0, keel), (x, -w * 0.93, chine), (x, -w, gun))])
    ip, ib, idk = m.mat(paint), m.mat(bottom), m.mat(deck)
    for a, b in zip(rings, rings[1:]):
        for k in range(4):
            f = m.bm.faces.new((a[k], b[k], b[k + 1], a[k + 1]))
            f.material_index = ip if k in (0, 3) else ib
            f.smooth = True
        f = m.bm.faces.new((a[4], b[4], b[0], a[0]))
        f.material_index = idk
    f = m.bm.faces.new(list(reversed(rings[0])))  # transom
    f.material_index = ip
    tip = rings[-1]
    f = m.bm.faces.new(tip)
    f.material_index = ip
    bmesh.ops.remove_doubles(m.bm, verts=m.bm.verts[:], dist=0.004)


def race_boat():
    m = Model("RaceBoat")
    hull_loft(m, 7.0, 2.4, "Paint_E53935", "Hull_F5F5F5", "Deck_ECEFF1")
    # A white boot stripe along the waterline, and the cockpit sunk into the deck.
    m.box((4.6, 1.7, 0.04), (-1.0, 0, 0.47), "Cockpit_263238")
    for side in (1, -1):
        m.box((4.2, 0.06, 0.12), (-1.3, side * 1.2, 0.08), "Stripe_FFFFFF")
        # bucket seats
        m.box((0.7, 0.6, 0.18), (-1.35, side * 0.42, 0.62), "Seat_37474F", bevel=0)
        m.box((0.16, 0.6, 0.75), (-1.75, side * 0.42, 0.95), "Seat_37474F", rot=(0, -12, 0))
        m.box((0.6, 0.5, 0.06), (-1.35, side * 0.42, 0.73), "Cushion_B0BEC5")
        # bow rails
        m.cyl(0.025, 0.025, 1.7, (1.85, side * (0.95 - 0.25), 0.82), "Chrome_CFD8DC", rot=(0, 90, -side * 14), segs=8)
        for x in (1.1, 1.85, 2.6):
            m.cyl(0.02, 0.02, 0.3, (x, side * (0.95 - (x - 1.0) * 0.32), 0.68), "Chrome_CFD8DC", segs=6)
    # Dashboard console and wheel
    m.box((0.5, 1.5, 0.45), (0.0, 0, 0.7), "Console_ECEFF1", bevel=0)
    m.cyl(0.17, 0.17, 0.04, (-0.25, 0.42, 0.98), "Trim_212121", rot=(0, 60, 0), segs=16)
    # Wraparound windscreen: a raked, bent sheet of tinted glass
    pts = []
    for k in range(7):
        a = -1 + k / 3.0
        y = a * 0.82
        x = 0.35 - 0.22 * a * a
        pts.append((x, y))
    lower = [m.bm.verts.new((x, y, 0.92)) for x, y in pts]
    upper = [m.bm.verts.new((x - 0.42, y * 0.92, 1.42)) for x, y in pts]
    gi = m.mat("Glass_1B3A57")
    for k in range(6):
        f = m.bm.faces.new((lower[k], lower[k + 1], upper[k + 1], upper[k]))
        f.material_index = gi
        f.smooth = True
    # Deck stripes and a race-number roundel on the foredeck
    m.box((2.6, 0.32, 0.03), (1.6, 0, 0.64), "Paint_E53935")
    m.cyl(0.45, 0.45, 0.03, (2.15, 0, 0.67), "Roundel_FFFFFF", segs=24)
    # Outboard: cowling, leg, cavitation plate and propeller
    m.sphere(0.42, (-3.75, 0, 0.85), "Engine_1A1A1A", scale=(1.05, 0.8, 1.15), segs=20, rings=12)
    m.box((0.5, 0.06, 0.2), (-3.75, 0, 1.25), "Paint_E53935")
    m.box((0.32, 0.16, 1.2), (-3.85, 0, 0.0), "Engine_1A1A1A", bevel=0)
    m.box((0.6, 0.45, 0.04), (-3.85, 0, -0.42), "Engine_1A1A1A")
    for k in range(3):
        m.box((0.05, 0.42, 0.12), (-4.05, 0, -0.55), "Prop_B0BEC5", rot=(k * 60, 0, 0))
    # Roll cage behind the seats, in the boat's colour
    for side in (1, -1):
        m.cyl(0.05, 0.05, 1.3, (-2.1, side * 0.75, 1.1), "Paint_E53935", rot=(side * 12, 0, 0), segs=8)
    m.cyl(0.05, 0.05, 1.3, (-2.1, 0, 1.72), "Paint_E53935", rot=(90, 0, 0), segs=8)
    m.box((0.1, 1.4, 0.35), (-2.1, 0, 1.55), "Sponsor_FFD600")
    return m.export()


# ---------------------------------------------------------------------------
# Vegetation, in the rounded storybook style of the reference video
# ---------------------------------------------------------------------------
def round_tree(name, leaf, seed, height):
    m = Model(name)
    random.seed(seed)
    trunk_h = height * 0.45
    m.cyl(0.5, 0.3, trunk_h + 0.6, (0, 0, (trunk_h + 0.6) / 2), "Bark_6D4C41", segs=10)
    for k in range(2):
        a = random.uniform(0, 360)
        m.cyl(0.12, 0.07, 1.6, (0.4 * math.cos(math.radians(a)), 0.4 * math.sin(math.radians(a)), trunk_h * 0.8),
              "Bark_6D4C41", rot=(0, 45, a), segs=8)
    blobs = [(0, 0, trunk_h + 1.3, 2.4)]
    for k in range(4):
        a = k * 90 + random.uniform(-25, 25)
        r = random.uniform(1.2, 1.8)
        blobs.append((r * math.cos(math.radians(a)), r * math.sin(math.radians(a)),
                      trunk_h + random.uniform(0.2, 1.6), random.uniform(1.4, 1.9)))
    blobs.append((0.2, -0.1, trunk_h + 2.8, 1.7))
    for i, (x, y, z, r) in enumerate(blobs):
        v = m.blob(r, (x, y, z), leaf, scale=(1, 1, 0.85), lump=0.18, seed=seed * 10 + i, subdiv=2, freq=1.2)
        m.recolour_up(v, leaf.split("_")[0] + "Light_" + lighten(leaf), 0.7)
    return m.export()


def lighten(matname, amount=0.18):
    h = matname.rsplit("_", 1)[-1]
    c = [min(255, int(int(h[i:i + 2], 16) * (1 + amount) + 12)) for i in (0, 2, 4)]
    return "%02X%02X%02X" % tuple(c)


def pine():
    m = Model("Pine")
    m.cyl(0.25, 0.15, 3.0, (0, 0, 1.5), "Bark_5D4037", segs=8)
    for k in range(4):
        r = 2.4 - k * 0.5
        m.cyl(r, 0.0, 2.6, (0, 0, 2.2 + k * 1.55), "Needle_2E7D32" if k % 2 == 0 else "Needle_388E3C", segs=10, smooth=False)
    return m.export()


def palm():
    m = Model("Palm")
    random.seed(7)
    x = 0.0
    for k in range(7):
        lean = k * 3.0
        m.cyl(0.24 - k * 0.012, 0.22 - k * 0.012, 1.2, (x, 0, 0.6 + k * 1.12), "Bark_8D6E63" if k % 2 else "Bark_A1887F",
              rot=(0, lean, 0), segs=10)
        x += math.sin(math.radians(lean)) * 1.12
    top = (x, 0, 8.2)
    for k in range(9):
        a = k * 40 + random.uniform(-8, 8)
        v = m.sphere(1.0, (0, 0, 0), "Frond_43A047" if k % 2 else "Frond_66BB6A", scale=(2.2, 0.38, 0.07), segs=12, rings=6)
        xf = Matrix.Translation(Vector(top)) @ Matrix.Rotation(math.radians(a), 4, "Z") \
            @ Matrix.Rotation(math.radians(28), 4, "Y") @ Matrix.Translation(Vector((1.9, 0, 0)))
        bmesh.ops.transform(m.bm, matrix=xf, verts=v)
    for k in range(3):
        m.sphere(0.22, (top[0] + 0.25 * math.cos(k * 2.1), 0.25 * math.sin(k * 2.1), top[2] - 0.35), "Coconut_5D4037", segs=8, rings=6)
    return m.export()


def bush():
    m = Model("Bush")
    for i, (x, y, r) in enumerate([(0, 0, 1.0), (0.9, 0.3, 0.75), (-0.7, 0.5, 0.7), (0.2, -0.8, 0.65)]):
        v = m.blob(r, (x, y, r * 0.55), "Leaf_558B2F", scale=(1, 1, 0.8), lump=0.2, seed=40 + i, subdiv=2)
        m.recolour_up(v, "LeafLight_7CB342", 0.7)
    for k in range(5):
        a = k * 1.3
        m.sphere(0.12, (0.9 * math.cos(a), 0.9 * math.sin(a), 0.9), "Flower_F06292" if k % 2 else "Flower_FFF176", segs=6, rings=4)
    return m.export()


def rock(name, seed, scale, moss):
    m = Model(name)
    v = m.blob(1.0, (0, 0, 0.35), "Rock_8D8A84", scale=scale, lump=0.32, seed=seed, subdiv=2, smooth=False, freq=1.4)
    if moss:
        m.recolour_up(v, "Moss_7CA347", 0.6)
    return m.export()


def sea_stack():
    """A tall weathered rock pillar: one craggy column with strata and grass on
    top, unit radius at the base and about 6.5 m tall."""
    m = Model("SeaStack")
    rings, segs, height = 14, 16, 6.5
    strata = ["Rock_6D6A65", "Rock_8C7B6B", "Rock_75716A", "Rock_9A8A78"]
    grid = []
    for i in range(rings + 1):
        t = i / rings
        z = t * height
        base_r = 1.15 - 0.45 * t + 0.08 * math.sin(t * 9.0)
        row = []
        for k in range(segs):
            a = k / segs * math.tau
            n = vnoise(Vector((math.cos(a) * 2, math.sin(a) * 2, z * 0.8)), 61, 1.3)
            ledge = 0.12 * (1 if (i % 4 == 0) else 0)
            r = base_r * (1 + 0.22 * n) + ledge
            row.append(m.bm.verts.new((r * math.cos(a), r * math.sin(a), z)))
        grid.append(row)
    for i in range(rings):
        mi = m.mat(strata[(i // 2) % len(strata)])
        for k in range(segs):
            f = m.bm.faces.new((grid[i][k], grid[i][(k + 1) % segs], grid[i + 1][(k + 1) % segs], grid[i + 1][k]))
            f.material_index = mi
    top = m.bm.verts.new((0, 0, height + 0.35))
    gi = m.mat("Grass_6FAF46")
    for k in range(segs):
        f = m.bm.faces.new((grid[-1][k], grid[-1][(k + 1) % segs], top))
        f.material_index = gi
    m.blob(1.0, (0, 0, 0.05), "Foam_E3F2FD", scale=(1.5, 1.5, 0.1), lump=0.1, seed=70, subdiv=2)
    for k in range(3):
        a = k * 2.2
        m.blob(0.45, (1.2 * math.cos(a), 1.2 * math.sin(a), 0.2), "Rock_6D6A65", scale=(1, 0.8, 0.7), lump=0.3,
               seed=80 + k, smooth=False)
    return m.export()


# ---------------------------------------------------------------------------
# Course furniture
# ---------------------------------------------------------------------------
def pylon():
    """Inflatable turn-mark pylon in the style of circuit powerboat racing."""
    m = Model("Pylon")
    m.cyl(1.45, 1.45, 0.55, (0, 0, 0.0), "Collar_212121", segs=24)
    m.cyl(1.25, 0.42, 3.3, (0, 0, 1.9), "Paint_FF6F00", segs=24)
    m.cyl(1.0, 0.9, 0.42, (0, 0, 1.55), "Band_FFFFFF", segs=24)
    m.cyl(0.62, 0.55, 0.3, (0, 0, 2.85), "Band_FFFFFF", segs=24)
    m.sphere(0.46, (0, 0, 3.65), "Paint_FF6F00", segs=16, rings=10)
    return m.export()


def rope_float():
    m = Model("Float")
    m.sphere(0.32, (0, 0, 0.05), "Paint_E53935", scale=(1.25, 1, 0.9), segs=12, rings=8)
    m.cyl(0.05, 0.05, 0.9, (0, 0, 0.05), "Rope_D7CCC8", rot=(0, 90, 0), segs=6)
    return m.export()


def ramp():
    """Jump ramp: a sloping deck on pontoons. Top surface follows the 13 degree
    collision box the game uses: -0.56 m at the back, 4.06 m at the front."""
    m = Model("Ramp")
    slope = math.tan(math.radians(13))
    pts = [(-10, -12), (10, -12), (10, 12), (-10, 12)]
    vs = []
    for x, y in pts:
        vs.append(m.bm.verts.new((x, y, 1.75 + x * slope)))
    bottom = [m.bm.verts.new((x, y, -0.9)) for x, y in pts]
    deck = m.bm.faces.new(vs)
    deck.material_index = m.mat("Deck_37474F")
    for i in range(4):
        f = m.bm.faces.new((bottom[i], bottom[(i + 1) % 4], vs[(i + 1) % 4], vs[i]))
        f.material_index = m.mat("Side_FFC107")
    f = m.bm.faces.new(list(reversed(bottom)))
    f.material_index = m.mat("Side_FFC107")
    # Chevrons painted up the deck, pointing the way to jump
    for k in range(5):
        x = -7.5 + k * 3.6
        for side in (1, -1):
            v = m.box((1.0, 6.5, 0.06), (x, side * 3.2, 1.79 + x * slope), "Chevron_FFFFFF" if k % 2 else "Chevron_FF1744",
                      rot=(0, -13, side * -35))
    # Side rails and posts
    for side in (1, -1):
        m.cyl(0.08, 0.08, 20.6, (0, side * 11.8, 2.95), "Rail_ECEFF1", rot=(0, 90 - 13, 0), segs=8)
        for x in (-8, -3, 2, 7):
            m.cyl(0.07, 0.07, 1.2, (x, side * 11.8, 1.75 + x * slope + 0.6), "Rail_ECEFF1", segs=6)
    # Pontoon floats underneath
    for side in (1, -1):
        m.cyl(1.0, 1.0, 21, (0, side * 9.5, -0.6), "Pontoon_FF6F00", rot=(0, 90, 0), segs=14)
    # Flags on the lip
    for side in (1, -1):
        m.cyl(0.05, 0.05, 3.5, (9.8, side * 12, 5.6), "Rail_ECEFF1", segs=6)
        m.box((1.4, 0.05, 0.8), (9.1, side * 12, 6.9), "Flag_FF1744" if side > 0 else "Flag_2979FF")
    return m.export()


def start_gantry():
    """Start / finish gantry: two towers 100 m apart on pontoons, a truss with a
    chequered banner and five start lights (Lamp1..Lamp5) the game switches."""
    m = Model("StartGantry")
    span = 50.0
    for side in (1, -1):
        y = side * span
        m.box((5, 5, 1.2), (0, y, 0), "Pontoon_263238")
        for dx in (-1, 1):
            for dy in (-1, 1):
                m.box((0.35, 0.35, 19), (dx * 1.1, y + dy * 1.1, 10), "Tower_ECEFF1")
        for z in range(2, 19, 3):
            m.box((2.4, 0.15, 0.15), (0, y - 1.1, z), "Tower_ECEFF1", rot=(0, 0, 0))
            m.box((2.4, 0.15, 0.15), (0, y + 1.1, z), "Tower_ECEFF1")
            m.box((0.15, 2.4, 0.15), (-1.1, y, z), "Tower_ECEFF1")
            m.box((0.15, 2.4, 0.15), (1.1, y, z), "Tower_ECEFF1")
        m.box((0.3, 2.6, 9), (1.4, y, 9), "Sponsor_00B0FF" if side > 0 else "Sponsor_FF4081")
    # Truss beam
    for z in (17.5, 20.0):
        for x in (-0.9, 0.9):
            m.box((0.3, 2 * span + 2.5, 0.3), (x, 0, z), "Truss_B0BEC5")
    for k in range(-24, 25):
        m.box((0.18, 0.18, 3.0), (0.9 * (1 if k % 2 else -1), k * 2.05, 18.75), "Truss_B0BEC5", rot=(0, 0, 0))
    # Chequered banner hanging from the truss
    for k in range(-14, 15):
        for row in range(2):
            black = (k + row) % 2 == 0
            m.box((0.12, 3.0, 1.3), (0, k * 3.0, 16.2 - row * 1.3), "Check_111111" if black else "Check_FAFAFA")
    # Start lights
    m.box((0.6, 9.0, 1.6), (0.6, 0, 21.6), "Panel_111111")
    for k in range(5):
        m.sphere(0.55, (1.0, (k - 2) * 1.7, 21.6), "Lamp%d_3A0000" % (k + 1), segs=12, rings=8)
    return m.export()


def barge():
    m = Model("Barge")
    hull_loft(m, 40.0, 12.0, "Hull_B23A2E", "Hull_5D2A22", "Deck_455A64",
              stations=[-1.0, -0.95, -0.5, 0.0, 0.5, 0.88, 0.96, 1.0], height=2.4)
    colours = ["Box_1E88E5", "Box_FB8C00", "Box_43A047", "Box_FDD835", "Box_8E24AA", "Box_E53935"]
    k = 0
    for x in (-10, -3.5, 3.0, 9.5):
        for y in (-2.6, 2.6):
            for z in (2.7, 5.3) if (x + y) % 2 else (2.7,):
                m.box((6.0, 2.4, 2.5), (x, y, z), colours[k % len(colours)])
                m.box((6.05, 2.45, 0.12), (x, y, z + 1.0), "Rib_ECEFF1")
                k += 1
    m.box((5.5, 8.0, 3.2), (-16.0, 0, 3.0), "Cabin_FAFAFA")
    m.box((4.0, 6.0, 2.2), (-16.5, 0, 5.7), "Cabin_FAFAFA")
    m.box((4.05, 6.05, 0.7), (-16.5, 0, 6.0), "Window_1B3A57")
    m.cyl(0.35, 0.35, 4.0, (-17.5, 0, 8.5), "Mast_ECEFF1", segs=8)
    m.cyl(0.6, 0.6, 2.2, (-14.8, 2.5, 7.4), "Funnel_212121", segs=12)
    m.box((0.7, 0.5, 0.4), (-16.5, 0, 10.3), "Lamp_FFEB3B")
    return m.export()


def cottage():
    """The little cottage from the reference video: cream walls, blue roof."""
    m = Model("Cottage")
    m.box((8.4, 6.4, 0.6), (0, 0, 0.3), "Stone_9E9E9E")
    m.box((8.0, 6.0, 3.4), (0, 0, 2.3), "Wall_F5ECD7")
    # Gable roof as a prism
    ridge = 6.4
    p = [(-4.5, -3.5, 4.0), (4.5, -3.5, 4.0), (4.5, 3.5, 4.0), (-4.5, 3.5, 4.0), (-4.5, 0, ridge), (4.5, 0, ridge)]
    v = [m.bm.verts.new(c) for c in p]
    ri = m.mat("Roof_2F6FBF")
    for quad in ((v[0], v[1], v[5], v[4]), (v[2], v[3], v[4], v[5])):
        f = m.bm.faces.new(quad); f.material_index = ri; f.smooth = False
    wi = m.mat("Gable_F5ECD7")
    for tri in ((v[1], v[2], v[5]), (v[3], v[0], v[4])):
        f = m.bm.faces.new(tri); f.material_index = wi
    m.box((1.0, 1.0, 2.6), (2.4, 1.6, 6.2), "Brick_A1887F")
    m.box((1.3, 0.12, 2.2), (0, -3.02, 1.7), "Door_795548")
    for x in (-2.6, 2.6):
        m.box((1.2, 0.12, 1.1), (x, -3.02, 2.6), "Window_FFCA28")
        m.box((1.45, 0.14, 0.12), (x, -3.04, 2.0), "Trim_FFFFFF")
        m.box((1.2, 0.12, 1.1), (x, 3.02, 2.6), "Window_FFCA28")
    for y in (-1.5, 1.5):
        m.box((0.12, 1.1, 1.1), (4.02, y, 2.6), "Window_FFCA28")
    m.box((3.0, 1.6, 0.15), (0, -3.9, 3.3), "Roof_2F6FBF", rot=(-15, 0, 0))
    for x in (-1.3, 1.3):
        m.cyl(0.08, 0.08, 2.6, (x, -4.5, 1.6), "Trim_FFFFFF", segs=6)
    return m.export()


def lighthouse():
    m = Model("Lighthouse")
    m.blob(1.0, (0, 0, 0.6), "Rock_8D8A84", scale=(5, 5, 1.6), lump=0.25, seed=90, smooth=False)
    m.cyl(3.4, 3.4, 2.0, (0, 0, 1.6), "Base_ECEFF1", segs=20)
    h, r0, r1 = 17.0, 2.4, 1.6
    bands = 6
    for k in range(bands):
        z0 = 2.6 + k * h / bands
        ra = r0 + (r1 - r0) * k / bands
        rb = r0 + (r1 - r0) * (k + 1) / bands
        m.cyl(ra, rb, h / bands, (0, 0, z0 + h / bands / 2), "Band_D32F2F" if k % 2 == 0 else "Band_FAFAFA", segs=20)
    top = 2.6 + h
    m.cyl(2.3, 2.3, 0.3, (0, 0, top + 0.15), "Rail_263238", segs=20)
    m.cyl(1.3, 1.3, 1.8, (0, 0, top + 1.2), "Glass_90CAF9", segs=16)
    m.sphere(0.7, (0, 0, top + 1.2), "Lamp_FFF59D", segs=12, rings=8)
    m.cyl(1.5, 0.2, 1.4, (0, 0, top + 2.8), "Band_D32F2F", segs=16)
    for k in range(4):
        m.box((0.9, 0.15, 1.4), (r0 * math.cos(k * 1.57 + 0.5) * 0.99, r0 * math.sin(k * 1.57 + 0.5) * 0.99, 7 + k * 3),
              "Window_263238", rot=(0, 0, math.degrees(k * 1.57 + 0.5)))
    return m.export()


def pier():
    """One 30 m bay of a wooden jetty; the game lines several up."""
    m = Model("Pier")
    for k in range(20):
        m.box((1.45, 3.4, 0.16), (-14.25 + k * 1.5, 0, 1.6), "Plank_A1887F" if k % 2 else "Plank_8D6E63")
    for x in range(-15, 16, 5):
        for side in (1, -1):
            m.cyl(0.18, 0.18, 4.5, (x, side * 1.6, -0.6), "Post_5D4037", segs=8)
            m.cyl(0.05, 0.05, 1.1, (x, side * 1.6, 2.2), "Post_5D4037", segs=6)
    for side in (1, -1):
        m.box((30, 0.1, 0.1), (0, side * 1.6, 2.7), "Rail_D7CCC8")
    m.cyl(0.18, 0.12, 0.5, (6, 1.3, 1.9), "Bollard_37474F", segs=10)
    m.cyl(0.18, 0.12, 0.5, (-6, -1.3, 1.9), "Bollard_37474F", segs=10)
    return m.export()


def grandstand():
    m = Model("Grandstand")
    teams = ["Seat_E53935", "Seat_1E88E5", "Seat_FDD835", "Seat_43A047", "Seat_FB8C00", "Seat_8E24AA"]
    for tier in range(7):
        z = 0.8 + tier * 0.75
        x = -tier * 1.1
        m.box((1.1, 40, 0.75), (x, 0, z - 0.375), "Concrete_B0BEC5")
        for k in range(16):
            m.box((0.6, 2.3, 0.45), (x + 0.2, -18.75 + k * 2.5, z + 0.22), teams[(k // 2 + tier) % len(teams)])
    m.box((1.0, 40, 6.5), (-7.8, 0, 3.25), "Wall_37474F")
    for k in range(5):
        m.box((0.2, 7.5, 1.6), (-7.2, -16 + k * 8, 5.4), ["Sponsor_00B0FF", "Sponsor_FF4081", "Sponsor_FFD600", "Sponsor_00E676", "Sponsor_FF6D00"][k])
    for y in (-19, -6.5, 6.5, 19):
        m.cyl(0.18, 0.18, 9.0, (-7.6, y, 4.5), "Post_ECEFF1", segs=8)
        m.cyl(0.14, 0.14, 7.0, (-3.0, y, 7.6), "Post_ECEFF1", rot=(0, 70, 0), segs=8)
    m.box((9.5, 42, 0.25), (-3.5, 0, 9.2), "Canopy_FAFAFA", rot=(0, 8, 0))
    m.box((9.6, 42.1, 0.3), (-3.5, 0, 9.05), "CanopyEdge_E53935", rot=(0, 8, 0))
    for y in range(-20, 21, 5):
        m.cyl(0.05, 0.05, 3.0, (-8.0, y, 10.8), "Post_ECEFF1", segs=6)
        m.box((0.05, 1.3, 0.8), (-8.0, y + 0.65, 11.8), ["Flag_E53935", "Flag_1E88E5", "Flag_FDD835", "Flag_43A047"][(y // 5) % 4])
    return m.export()


def tent():
    m = Model("Tent")
    m.box((6, 6, 2.4), (0, 0, 1.2), "Canvas_FAFAFA")
    apex = m.bm.verts.new((0, 0, 4.6))
    corners = [m.bm.verts.new(c) for c in ((-3.3, -3.3, 2.4), (3.3, -3.3, 2.4), (3.3, 3.3, 2.4), (-3.3, 3.3, 2.4))]
    for i in range(4):
        a, b = corners[i], corners[(i + 1) % 4]
        for s in range(4):
            t0, t1 = s / 4, (s + 1) / 4
            p0 = m.bm.verts.new(a.co.lerp(b.co, t0))
            p1 = m.bm.verts.new(a.co.lerp(b.co, t1))
            f = m.bm.faces.new((p0, p1, apex))
            f.material_index = m.mat("Stripe_E53935" if s % 2 == 0 else "Stripe_FAFAFA")
    m.cyl(0.05, 0.05, 1.6, (0, 0, 5.2), "Post_ECEFF1", segs=6)
    m.box((0.05, 0.9, 0.5), (0, 0.45, 5.8), "Flag_FFD600")
    return m.export()


def flag():
    m = Model("Flag")
    m.cyl(0.07, 0.05, 9, (0, 0, 4.5), "Post_ECEFF1", segs=8)
    # A swallow-tailed pennant, thin enough to read as cloth, flying along +X.
    outline = [(0, 8.8), (3.4, 8.4), (2.6, 7.8), (3.4, 7.1), (0, 6.6)]
    front = [m.bm.verts.new((x, 0.03, z)) for x, z in outline]
    back = [m.bm.verts.new((x, -0.03, z)) for x, z in outline]
    paint = m.mat("Paint_FFD600")
    for face in (front, list(reversed(back))):
        f = m.bm.faces.new(face); f.material_index = paint
    for i in range(5):
        f = m.bm.faces.new((front[i], back[i], back[(i + 1) % 5], front[(i + 1) % 5])); f.material_index = paint
    return m.export()


def banner():
    m = Model("Banner")
    m.box((2.0, 14, 1.0), (0, 0, 0), "Pontoon_263238")
    for y in (-6.5, 6.5):
        m.cyl(0.15, 0.15, 5.5, (0, y, 3.0), "Post_ECEFF1", segs=8)
    m.box((0.25, 13.6, 3.6), (0, 0, 4.0), "Panel_FFFFFF")
    stripes = ["Band_00B0FF", "Band_FF4081", "Band_FFD600"]
    for k, c in enumerate(stripes):
        m.box((0.3, 4.4, 3.2), (0, -4.5 + k * 4.5, 4.0), c)
    return m.export()


def bridge():
    """Red steel arch bridge across the narrows: 180 m between abutments."""
    m = Model("Bridge")
    span = 90.0
    deck_z = 26.0
    m.box((12, 2 * span + 20, 1.6), (0, 0, deck_z), "Deck_607D8B")
    m.box((12.2, 2 * span + 20.2, 0.5), (0, 0, deck_z + 1.0), "Road_37474F")
    for side in (1, -1):
        m.box((0.3, 2 * span + 20, 1.1), (side * 5.9, 0, deck_z + 1.6), "Rail_ECEFF1")
    # Two arch ribs
    segs = 24
    for side in (1, -1):
        prev = None
        for k in range(segs + 1):
            t = -1 + 2 * k / segs
            y = t * span
            z = deck_z + 30 * (1 - t * t)
            if prev is not None:
                py, pz = prev
                mid = ((py + y) / 2, (pz + z) / 2)
                length = math.hypot(y - py, z - pz)
                ang = math.degrees(math.atan2(z - pz, y - py))
                m.box((1.4, length + 0.4, 1.8), (side * 5.0, mid[0], mid[1]), "Steel_D32F2F", rot=(ang, 0, 0))
            prev = (y, z)
        for k in range(1, segs):
            t = -1 + 2 * k / segs
            z = 30 * (1 - t * t)
            if z > 2:
                m.cyl(0.12, 0.12, z, (side * 5.0, t * span, deck_z + z / 2), "Hanger_ECEFF1", segs=6)
    for k in range(1, 12):
        t = -1 + 2 * k / 12
        z = deck_z + 30 * (1 - t * t)
        m.box((10, 0.6, 0.6), (0, t * span, z), "Steel_D32F2F")
    for side in (1, -1):
        m.box((14, 12, deck_z + 8), (0, side * (span + 6), (deck_z - 8) / 2), "Stone_9E9E9E")
    return m.export()


def yacht():
    m = Model("Yacht")
    hull_loft(m, 14.0, 4.2, "Hull_FAFAFA", "Hull_ECEFF1", "Deck_D7CCC8", height=1.6)
    m.box((14.1, 0.1, 0.3), (0, 0, 0), "Stripe_1565C0")
    for side in (1, -1):
        m.box((11.5, 0.08, 0.25), (-0.2, side * 2.08, 0.1), "Stripe_1565C0")
    m.box((6.5, 3.4, 1.8), (-1.5, 0, 1.6), "Cabin_FAFAFA", bevel=0)
    m.box((6.6, 3.45, 0.6), (-1.5, 0, 1.8), "Window_263238")
    m.box((3.5, 2.8, 1.1), (-2.2, 0, 2.95), "Cabin_FAFAFA")
    m.box((3.6, 2.85, 0.4), (-2.0, 0, 3.0), "Window_263238")
    m.cyl(0.06, 0.06, 2.5, (-2.6, 0, 4.5), "Mast_ECEFF1", segs=6)
    return m.export()


def duck():
    """A giant inflatable duck, like the one moored in the reference video."""
    m = Model("Duck")
    m.sphere(1.0, (0, 0, 0.55), "Duck_FFD600", scale=(1.35, 1.0, 0.75), segs=24, rings=14)
    m.sphere(0.62, (0.95, 0, 1.45), "Duck_FFD600", segs=20, rings=12)
    m.sphere(0.32, (1.55, 0, 1.38), "Beak_FF8F00", scale=(1.1, 0.95, 0.38), segs=14, rings=8)
    for side in (1, -1):
        m.sphere(0.1, (1.38, side * 0.32, 1.65), "Eye_111111", segs=8, rings=6)
        m.sphere(0.6, (-0.1, side * 0.82, 0.75), "Duck_FDD835", scale=(1.2, 0.35, 0.6), segs=14, rings=8)
    m.sphere(0.4, (-1.35, 0, 0.95), "Duck_FFD600", scale=(0.8, 0.7, 0.55), segs=12, rings=8)
    return m.export()


# ---------------------------------------------------------------------------
# Weapons and pickups
# ---------------------------------------------------------------------------
def torpedo():
    m = Model("Torpedo")
    m.cyl(0.18, 0.18, 2.2, (0, 0, 0), "Body_455A64", rot=(0, 90, 0), segs=16)
    m.sphere(0.18, (1.1, 0, 0), "Nose_E53935", scale=(2.0, 1, 1), segs=16, rings=8)
    m.cyl(0.18, 0.1, 0.4, (-1.3, 0, 0), "Body_455A64", rot=(0, -90, 0), segs=16)
    for k in range(4):
        m.box((0.35, 0.03, 0.5), (-1.25, 0, 0), "Fin_FFC107", rot=(k * 90, 0, 0))
    m.cyl(0.19, 0.19, 0.12, (0.3, 0, 0), "Band_FFC107", rot=(0, 90, 0), segs=16)
    return m.export()


def mine():
    m = Model("Mine")
    m.sphere(0.6, (0, 0, 0), "Shell_263238", segs=20, rings=12)
    dirs = [(1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0.7, 0.7, 0.4), (-0.7, 0.7, 0.4), (0.7, -0.7, 0.4), (-0.7, -0.7, 0.4)]
    for d in dirs:
        n = Vector(d).normalized()
        rot = n.to_track_quat("Z", "Y").to_euler()
        m.cyl(0.07, 0.05, 0.4, tuple(n * 0.72), "Horn_B0BEC5",
              rot=(math.degrees(rot.x), math.degrees(rot.y), math.degrees(rot.z)), segs=8)
    m.cyl(0.62, 0.62, 0.1, (0, 0, -0.05), "Band_FFC107", segs=20)
    m.sphere(0.13, (0, 0, 0.66), "Lamp_FF1744", segs=8, rings=6)
    return m.export()


def crate():
    m = Model("Crate")
    m.box((1.6, 1.6, 1.6), (0, 0, 0), "Wood_B57B45")
    for axis in range(3):
        for s in (1, -1):
            for t in (1, -1):
                size = [1.7, 1.7, 1.7]
                size[axis] = 1.7
                other = [i for i in range(3) if i != axis]
                size[other[0]] = 0.18
                size[other[1]] = 0.18
                loc = [0, 0, 0]
                loc[other[0]] = s * 0.78
                loc[other[1]] = t * 0.78
                m.box(tuple(size), tuple(loc), "Frame_6D4C41")
    for s in (1, -1):
        m.box((1.0, 0.05, 0.4), (0, s * 0.83, 0.1), "Mark_FFD600")
        m.box((0.05, 1.0, 0.4), (s * 0.83, 0, 0.1), "Mark_FFD600")
    m.cyl(1.25, 1.25, 0.35, (0, 0, -0.75), "Collar_FF6D00", segs=20)
    return m.export()


def barrel():
    m = Model("Barrel")
    m.cyl(0.45, 0.45, 1.3, (0, 0, 0), "Paint_1E88E5", segs=18)
    for z in (-0.4, 0.0, 0.4):
        m.cyl(0.47, 0.47, 0.06, (0, 0, z), "Rib_CFD8DC", segs=18)
    m.cyl(0.08, 0.08, 0.05, (0.2, 0.1, 0.67), "Rib_CFD8DC", segs=8)
    m.box((0.3, 0.02, 0.3), (0, 0.455, 0.15), "Mark_FFD600", rot=(0, 45, 0))
    return m.export()


def log():
    m = Model("Log")
    v = m.cyl(0.38, 0.33, 5.0, (0, 0, 0), "Bark_6D4C41", rot=(0, 90, 0), segs=12)
    for f in {f for x in v for f in x.link_faces}:
        f.normal_update()
        if abs(f.normal.x) > 0.9:
            f.material_index = m.mat("Wood_D7B98E")
    m.cyl(0.1, 0.06, 0.9, (0.8, 0.2, 0.35), "Bark_6D4C41", rot=(-40, 20, 0), segs=6)
    return m.export()


def ice_floe():
    m = Model("IceFloe")
    random.seed(5)
    pts = []
    for k in range(14):
        a = k / 14 * math.tau
        r = random.uniform(0.75, 1.1)
        pts.append((r * math.cos(a) * 1.3, r * math.sin(a)))
    m.poly_prism(pts, -0.35, 0.18, "Snow_FAFDFF", "Ice_B3E5FC")
    return m.export()


# The detailed boats live in make_boats.py; it replaces RaceBoat and Yacht.
exec(compile(open(os.path.join(os.path.dirname(os.path.abspath(globals().get("__file__") or sys.argv[sys.argv.index("-P") + 1])), "make_boats.py")).read(), "make_boats.py", "exec"))

def hangar():
    """The garage: an open-fronted steel boathouse built over its own water
    slip, like a race team's workshop on a marina. +X is the open front to
    the sea; the origin is the waterline at the middle of the slip mouth.
    30 m deep, 28 m wide, a 9 m slip with concrete quays either side, portal
    frames, corrugated walls and roof with skylights, a travelling crane over
    the slip, strip lights, benches, tool walls, drums, tyres and fenders."""
    m = Model("Hangar")
    D, HW, SLIP, QZ, EAVE, RIDGE = 30.0, 14.0, 4.5, 0.9, 9.0, 11.5
    x0 = -D  # back wall
    # Quays and the back floor: concrete slabs on piles, edged in yellow.
    for side in (1, -1):
        m.box((D, HW - SLIP, 0.5), (x0 / 2, side * (SLIP + HW) / 2, QZ - 0.25), "Concrete_A7ADB3")
        m.box((D, 0.25, 0.04), (x0 / 2, side * (SLIP + 0.3), QZ + 0.02), "Paint_FFC107")
        m.box((D, 0.35, 0.5), (x0 / 2, side * (SLIP + 0.12), QZ - 0.2), "Concrete_8E959C")  # quay edge
        for x in range(int(x0) + 2, 0, 4):
            m.cyl(0.32, 0.32, 4.0, (x, side * (SLIP + 0.5), QZ - 2.4), "Concrete_7D848B", segs=10)
            # marina fenders: blue sausages hanging off the quay face on a line
            m.cyl(0.2, 0.2, 0.8, (x + 2, side * (SLIP - 0.18), QZ - 0.75), "Rubber_1E5AA8", segs=16)
            m.sphere(0.2, (x + 2, side * (SLIP - 0.18), QZ - 0.35), "Rubber_1E5AA8", segs=16, rings=8)
            m.sphere(0.2, (x + 2, side * (SLIP - 0.18), QZ - 1.15), "Rubber_1E5AA8", segs=16, rings=8)
            m.box((0.03, 0.03, 0.5), (x + 2, side * (SLIP - 0.12), QZ - 0.05), "Rope_ECEFF1")
        for x in range(int(x0) + 4, 4, 6):
            m.cyl(0.17, 0.14, 0.42, (x, side * (SLIP + 0.75), QZ + 0.21), "Steel_8D99A3", segs=20)
            m.cyl(0.22, 0.22, 0.07, (x, side * (SLIP + 0.75), QZ + 0.45), "Steel_FFC107", segs=20)
    m.box((6.0, 2 * SLIP, 0.5), (x0 + 3.0, 0, QZ - 0.25), "Concrete_A7ADB3")
    m.box((0.25, 2 * SLIP, 0.04), (x0 + 6.1, 0, QZ + 0.02), "Paint_FFC107")
    slip_back = x0 + 6.0
    # Slipway ramp down into the water at the back of the slip.
    m.box((4.0, 2 * SLIP - 0.4, 0.3), (slip_back + 1.6, 0, -0.4), "Concrete_8E959C", rot=(0, 14, 0))

    # Portal frames every 6 m: I-section columns and pitched rafters.
    def ibeam(a, b, depth=0.5, width=0.3, mat="Steel_37474F"):
        a, b = Vector(a), Vector(b)
        d = b - a
        rot = d.to_track_quat("Z", "Y").to_euler()
        r = (math.degrees(rot.x), math.degrees(rot.y), math.degrees(rot.z))
        mid = tuple((a + b) / 2)
        m.box((depth, width, d.length), mid, mat, rot=r)
    for k in range(6):
        x = x0 + k * 6.0
        for side in (1, -1):
            ibeam((x, side * HW, QZ), (x, side * HW, EAVE))
            ibeam((x, side * HW, EAVE), (x, 0, RIDGE), depth=0.45)
            ibeam((x, side * (HW - 0.2), EAVE - 1.6), (x, side * (HW - 2.6), EAVE - 0.05), depth=0.25, width=0.2)  # haunch
    # Purlins and the eaves beams.
    for side in (1, -1):
        for t in (0.0, 0.33, 0.66, 1.0):
            y = side * HW * (1 - t)
            z = EAVE + (RIDGE - EAVE) * t + 0.35
            m.box((D, 0.15, 0.22), (x0 / 2, y, z), "Steel_546E7A")
    # Roof: corrugated sheets with two skylight strips each side.
    pitch = math.degrees(math.atan2(RIDGE - EAVE, HW))
    run = math.hypot(HW, RIDGE - EAVE) + 0.6
    for side in (1, -1):
        for k in range(10):
            x = x0 + 1.5 + k * 3.0
            mat = "Window_B3E5FC" if k in (2, 7) else ("Roof_9EA7AD" if k % 2 else "Roof_A9B2B8")
            m.box((3.0, run, 0.08), (x, side * HW / 2, (EAVE + RIDGE) / 2 + 0.55), mat, rot=(side * -pitch, 0, 0))
        m.box((D + 0.6, 0.3, 0.3), (x0 / 2, side * (HW + 0.25), EAVE + 0.45), "Trim_ECEFF1")  # gutter
    m.box((D + 0.6, 0.6, 0.18), (x0 / 2, 0, RIDGE + 0.62), "Trim_ECEFF1")  # ridge cap
    # Walls: vertical corrugated cladding on three sides; a high window band.
    for side in (1, -1):
        for k in range(30):
            x = x0 + 0.5 + k * 1.0
            m.box((0.5, 0.12, EAVE - QZ - 2.5), (x, side * (HW + 0.1), QZ + (EAVE - QZ - 2.5) / 2), "Wall_CFD8DC")
            m.box((0.5, 0.16, EAVE - QZ - 2.5), (x + 0.5, side * (HW + 0.14), QZ + (EAVE - QZ - 2.5) / 2), "Wall_B0BEC5")
        m.box((D, 0.1, 2.0), (x0 / 2, side * (HW + 0.12), EAVE - 1.3), "Window_90CAF9")
        for k in range(10):  # window mullions
            m.box((0.12, 0.2, 2.0), (x0 + k * 3.0, side * (HW + 0.15), EAVE - 1.3), "Frame_263238")
    for k in range(28):
        y = -HW + 0.5 + k * 1.0
        m.box((0.12, 0.5, EAVE - QZ), (x0 - 0.1, y, QZ + (EAVE - QZ) / 2), "Wall_CFD8DC" if k % 2 else "Wall_B0BEC5")
    # Gable above the back wall, and the team banner across it.
    gi = m.mat("Wall_B0BEC5")
    for side in (1, -1):
        for x, flip in ((x0 - 0.05, False), (x0 - 0.2, True)):  # both faces, so it reads from inside and out
            v = [m.bm.verts.new(c) for c in ((x, 0, RIDGE + 0.5), (x, side * HW, EAVE), (x, 0, EAVE))]
            f = m.bm.faces.new(v if (side > 0) != flip else v[::-1])
            f.material_index = gi
    m.box((0.1, 14.0, 2.2), (x0 + 0.05, 0, 6.6), "Sponsor_E53935")
    m.box((0.12, 13.0, 1.6), (x0 + 0.08, 0, 6.6), "Sponsor_111111")
    for k in range(6):  # chequer strip under the banner
        m.box((0.13, 1.0, 0.4), (x0 + 0.1, -6.0 + k * 2.4, 5.3), "Sponsor_FAFAFA")
    # The open front: a rolled-up door drum and its guide rails.
    m.box((1.1, 2 * HW, 1.1), (0.4, 0, EAVE - 0.3), "Steel_455A64")
    m.cyl(0.5, 0.5, 2 * HW - 1.0, (0.2, 0, EAVE - 0.9), "Steel_78909C", rot=(90, 0, 0), segs=20)
    for side in (1, -1):
        m.box((0.3, 0.3, EAVE - QZ), (0.15, side * (HW - 0.2), QZ + (EAVE - QZ) / 2), "Paint_FFC107")
        for k in range(8):  # hazard stripes on the door posts
            m.box((0.32, 0.32, 0.35), (0.15, side * (HW - 0.2), QZ + 0.5 + k * 1.0), "Paint_212121")

    # Travelling crane over the slip: two runway beams, a bridge and a hoist.
    for side in (1, -1):
        m.box((D - 3.0, 0.4, 0.6), (x0 / 2 - 1.0, side * (SLIP + 1.6), EAVE - 1.2), "Paint_FFB300")
    m.box((0.8, 2 * SLIP + 3.4, 0.7), (-11.0, 0, EAVE - 0.6), "Paint_FFB300")
    m.box((1.2, 1.0, 0.9), (-11.0, 0.8, EAVE - 1.3), "Steel_37474F")
    for dy in (-0.25, 0.25):
        m.cyl(0.03, 0.03, 4.5, (-11.0, 0.8 + dy, EAVE - 3.9), "Steel_90A4AE", segs=6)
    m.box((0.5, 0.8, 0.35), (-11.0, 0.8, EAVE - 6.2), "Paint_E53935")
    for dx in (-1.6, 1.6):  # lifting slings
        m.cyl(0.04, 0.04, 3.2, (-11.0 + dx * 0.5, 0.8, EAVE - 7.7), "Rope_FFC107", rot=(0, dx * 9, 0), segs=6)
    # Strip lights under every rafter bay.
    for k in range(5):
        x = x0 + 3.0 + k * 6.0
        for y in (-8.5, -3.0, 3.0, 8.5):
            m.box((2.4, 0.22, 0.1), (x, y, EAVE - 0.2), "Steel_263238")
            m.box((2.3, 0.16, 0.04), (x, y, EAVE - 0.27), "Lamp_FFFFFF")
    # Workshop: benches and tool walls along the starboard quay, stores to port.
    for k in range(3):
        x = x0 + 4.0 + k * 7.5
        m.box((3.0, 0.9, 0.08), (x, HW - 1.0, QZ + 0.92), "Wood_8D6E63")
        m.box((2.9, 0.8, 0.7), (x, HW - 1.0, QZ + 0.45), "Panel_C62828")
        for dy in range(4):
            m.box((0.55, 0.75, 0.05), (x - 1.1 + dy * 0.73, HW - 1.0, QZ + 0.6), "Trim_9E9E9E")  # drawers
        m.box((3.0, 0.06, 1.8), (x, HW - 0.1, QZ + 2.2), "Panel_37474F")  # tool board
        for t in range(9):
            m.box((0.05, 0.06, 0.3 + 0.2 * (t % 3)), (x - 1.3 + t * 0.32, HW - 0.16, QZ + 2.3), "Chrome_B0BEC5")
        m.cyl(0.06, 0.06, 0.05, (x + 1.1, HW - 1.1, QZ + 1.0), "Lamp_FFE0B2", segs=10)
    # A spare outboard on a stand and a stack of tyres beside the benches.
    m.box((0.8, 0.8, 1.0), (x0 + 22.0, HW - 1.4, QZ + 0.5), "Steel_455A64")
    m.box((0.75, 0.55, 0.65), (x0 + 22.0, HW - 1.4, QZ + 1.35), "Cowl_111111")
    m.box((0.6, 0.02, 0.08), (x0 + 22.0, HW - 1.68, QZ + 1.4), "Paint_E53935")
    for k in range(4):
        m.cyl(0.45, 0.45, 0.28, (x0 + 25.5, HW - 1.4, QZ + 0.14 + k * 0.29), "Rubber_1A1A1A", segs=18)
    # Port side: oil drums, crates and a fork-lift.
    drums = ["Paint_1565C0", "Paint_FFC107", "Paint_C62828", "Paint_1565C0", "Paint_2E7D32", "Paint_FFC107"]
    for k, mat in enumerate(drums):
        x = x0 + 2.0 + (k % 3) * 0.8
        y = -HW + 1.2 + (k // 3) * 0.8
        m.cyl(0.29, 0.29, 0.88, (x, y, QZ + 0.44), mat, segs=14)
        m.cyl(0.3, 0.3, 0.03, (x, y, QZ + 0.6), "Steel_37474F", segs=14)
    for k in range(5):
        m.box((1.1, 1.1, 0.9), (x0 + 8.0 + (k % 3) * 1.2, -HW + 1.3, QZ + 0.45 + (k // 3) * 0.9), "Box_A1887F")
    fx, fy = x0 + 16.0, -HW + 3.0
    m.box((2.2, 1.1, 1.0), (fx, fy, QZ + 0.75), "Paint_FF8F00")
    m.box((0.8, 1.0, 0.9), (fx - 0.6, fy, QZ + 1.65), "Paint_FF8F00")
    for dy in (-0.45, 0.45):
        m.box((0.08, 0.08, 2.3), (fx - 0.2, fy + dy, QZ + 1.95), "Steel_263238")  # roof posts
        m.box((0.08, 0.08, 2.6), (fx + 1.2, fy + dy * 0.6, QZ + 1.5), "Steel_263238")  # mast
        m.box((1.2, 0.12, 0.05), (fx + 1.85, fy + dy * 0.6, QZ + 0.3), "Steel_263238")  # forks
    m.box((1.3, 1.2, 0.06), (fx - 0.2, fy, QZ + 3.1), "Steel_263238")
    for dx, dy in ((0.7, 0.58), (0.7, -0.58), (-0.8, 0.58), (-0.8, -0.58)):
        m.cyl(0.3, 0.3, 0.22, (fx + dx, fy + dy, QZ + 0.3), "Rubber_1A1A1A", rot=(90, 0, 0), segs=14)
    # Life rings on the columns and a ladder down into the slip.
    for x in (x0 + 6.0, x0 + 18.0):
        for side in (1, -1):
            m.cyl(0.38, 0.38, 0.12, (x + 0.3, side * (HW - 0.5), QZ + 1.8), "Paint_FF5722", rot=(0, 90, 0), segs=16)
    for k in range(6):
        m.box((0.05, 0.6, 0.05), (-6.0, SLIP - 0.05, QZ - 0.1 - k * 0.35), "Chrome_B0BEC5")
    for dy in (-0.3, 0.3):
        m.box((0.06, 0.06, 2.2), (-6.0, SLIP - 0.05 + dy, QZ - 0.9), "Chrome_B0BEC5")
    obj = m.export()
    return obj


BUILD = [lambda: round_tree("TreeRound", "Leaf_5CB646", 3, 9.0),
         lambda: round_tree("TreeTall", "Leaf_3F9B3A", 11, 12.0), pine, palm, bush,
         lambda: rock("RockA", 21, (1.4, 1.0, 0.8), True), lambda: rock("RockB", 22, (1.0, 1.1, 1.2), False),
         lambda: rock("RockC", 23, (1.8, 1.2, 0.6), True), sea_stack, pylon, rope_float, ramp, start_gantry,
         barge, cottage, lighthouse, pier, grandstand, tent, flag, banner, bridge, duck,
         torpedo, mine, crate, barrel, log, ice_floe, hangar] + BOATS


def render_previews(objects):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "MATERIAL"
    scene.display.shading.show_shadows = True
    scene.display.shading.show_cavity = True
    scene.render.resolution_x, scene.render.resolution_y = 520, 360
    scene.render.film_transparent = False
    world = bpy.data.worlds.new("Sky") if scene.world is None else scene.world
    scene.world = world
    world.color = (0.55, 0.75, 0.95)
    cam_data = bpy.data.cameras.new("Cam")
    cam = bpy.data.objects.new("Cam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    os.makedirs(PREVIEW, exist_ok=True)
    for obj in objects:
        for other in objects:
            other.hide_render = other is not obj
        size = max(obj.dimensions)
        centre = sum((obj.matrix_world @ Vector(c) for c in obj.bound_box), Vector()) / 8
        d = Vector((1.0, -1.15, 0.6)).normalized() * size * 1.55
        cam.location = centre + d
        cam.rotation_euler = (centre - cam.location).to_track_quat("-Z", "Y").to_euler()
        cam_data.lens = 35
        scene.render.filepath = os.path.join(PREVIEW, obj.name + ".png")
        bpy.ops.render.render(write_still=True)


bpy.ops.wm.read_factory_settings(use_empty=True)
made = [build() for build in BUILD]
print("exported %d models to %s" % (len(made), OUT))
if PREVIEW:
    render_previews(made)
    print("previews in %s" % PREVIEW)
