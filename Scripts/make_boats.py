"""Detailed boats for make_models.py: smooth subdivision hulls with a real
deep-V bottom, lifting strakes, a reverse chine, twin steps, a recessed
cockpit, and a livery texture mapped along the topsides.

Shapes follow real offshore and centre-console powerboats (deep-V stepped
hulls, multiple outboards, wraparound screens), not any game's models.
Run through make_models.py, which execs this file into its namespace.
"""
import math

import bmesh
import bpy
from mathutils import Vector

TEXTURES = os.path.join(os.path.dirname(OUT), "Textures")
os.makedirs(TEXTURES, exist_ok=True)


def smoothstep(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


# ---------------------------------------------------------------------------
# Livery: a mask texture. R = the boat's own colour, G = the dark accent,
# B = white pinstripes and roundel. M_Livery in Unreal mixes the colours.
# ---------------------------------------------------------------------------
def make_livery(name, width=1024, height=256, style=0):
    path = os.path.join(TEXTURES, name + ".png")
    img = bpy.data.images.new(name, width, height, alpha=True)
    px = [0.0] * (width * height * 4)
    aspect = width / height / 4.0  # hull side is ~4x longer than tall in UV
    for j in range(height):
        v = (j + 0.5) / height
        for i in range(width):
            u = (i + 0.5) / width          # 0 stern .. 1 bow
            r, g, b = 1.0, 0.0, 0.0
            if style == 0:
                # A sweeping dark band rising towards the bow, edged in white.
                c = 0.22 + 0.5 * (u ** 1.6)
                h = 0.2 * (1.0 - 0.55 * u)
                d = abs(v - c)
                if d < h:
                    r, g = 0.0, 1.0
                elif d < h + 0.035:
                    r, b = 0.0, 1.0
                # boot stripe at the chine
                if v < 0.07:
                    r, g, b = 0.0, 1.0, 0.0
                elif v < 0.1:
                    r, g, b = 0.0, 0.0, 1.0
                # chequer band near the transom
                if 0.035 < u < 0.11 and v > 0.55:
                    check = (int(u * 90) + int(v * 22)) % 2
                    r, g, b = 0.0, float(check), float(1 - check)
                # race roundel forward
                du, dv = (u - 0.72) * aspect * 4.0, v - 0.6
                rr = math.hypot(du * 0.25, dv)
                if rr < 0.15:
                    r, g, b = 0.0, 0.0, 1.0
                elif rr < 0.18:
                    r, g, b = 0.0, 1.0, 0.0
            else:
                # Cruising boats: plain colour with a double cove stripe.
                if 0.62 < v < 0.68 or 0.72 < v < 0.75:
                    r, g, b = 0.0, 1.0, 0.0
                if v < 0.08:
                    r, g, b = 0.0, 0.0, 1.0
            k = (j * width + i) * 4
            px[k:k + 4] = (r, g, b, 1.0)
    img.pixels[:] = px
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    return img


def livery_material(slot_name, image):
    """A Blender material whose glTF export carries the livery image."""
    m = material(slot_name)
    if image is not None and m.node_tree is not None:
        nodes = m.node_tree.nodes
        tex = nodes.new("ShaderNodeTexImage")
        tex.image = image
        bsdf = nodes.get("Principled BSDF")
        if bsdf is not None:
            m.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    return m


# ---------------------------------------------------------------------------
# Objects: build in bmesh, turn into objects, subdivide, join, export
# ---------------------------------------------------------------------------
def to_object(name, bm, mats, smooth=True):
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    for mname in mats:
        mesh.materials.append(material(mname) if not isinstance(mname, bpy.types.Material) else mname)
    if smooth:
        for poly in mesh.polygons:
            poly.use_smooth = True
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def activate(obj):
    for other in bpy.context.scene.objects:
        other.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


def subdivide(obj, levels=2):
    activate(obj)
    mod = obj.modifiers.new("Subsurf", "SUBSURF")
    mod.levels = levels
    mod.render_levels = levels
    try:
        mod.uv_smooth = "PRESERVE_BOUNDARIES"
    except Exception:
        pass
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj


def soften(obj, width=0.04, segments=3):
    """Rounds the edges of a hard-surface part (cowls, cushions, cabins) while
    keeping its shape, which subdividing a plain box would melt into a blob."""
    activate(obj)
    mod = obj.modifiers.new("Bevel", "BEVEL")
    mod.width = width
    mod.segments = segments
    mod.limit_method = "ANGLE"
    bpy.ops.object.modifier_apply(modifier=mod.name)
    for poly in obj.data.polygons:
        poly.use_smooth = True
    try:
        activate(obj)
        bpy.ops.object.shade_auto_smooth(angle=math.radians(40))
    except Exception:
        pass
    return obj


def join_and_export(name, parts):
    parts = [p for p in parts if p is not None]
    main = parts[0]
    for other in bpy.context.scene.objects:
        other.select_set(False)
    for p in parts:
        p.select_set(True)
    bpy.context.view_layer.objects.active = main
    if len(parts) > 1:
        bpy.ops.object.join()
    main.name = name
    main.data.name = name
    # Merge material slots that ended up duplicated by the join.
    path = os.path.join(OUT, name + ".glb")
    kwargs = dict(filepath=path, export_format="GLB", use_selection=True, export_apply=True)
    try:
        bpy.ops.export_scene.gltf(export_vertex_color="ACTIVE", **kwargs)
    except TypeError:
        bpy.ops.export_scene.gltf(**kwargs)
    dims = main.dimensions
    tris = sum(len(p.vertices) - 2 for p in main.data.polygons)
    print("model %-16s %6d tris  %5.1f x %5.1f x %5.1f m  %d materials" % (
        name, tris, dims.x, dims.y, dims.z, len(main.data.materials)))
    return main


def crease_layer(bm):
    layer = bm.edges.layers.float.get("crease_edge")
    return layer if layer is not None else bm.edges.layers.float.new("crease_edge")


# ---------------------------------------------------------------------------
# The hull
# ---------------------------------------------------------------------------
def smooth_hull(name, L, B, deck_z, keel_z, deadrise=24.0, bow_start=0.25, rocker=0.75, sheer=0.22,
                crown=0.07, steps=(), stations=26, cockpit=None, livery="Livery_FFFFFF", deck="Deck_F2F2F2",
                bottom="Bottom_F5F5F5", tub="Sole_303030", strakes=True, livery_image=None, transom_rake=0.0):
    """Returns a subdivided hull object. cockpit = (x_aft, x_fwd, half_width_fraction, depth)."""
    bm = bmesh.new()
    crease = crease_layer(bm)
    uv = bm.loops.layers.uv.new("UVMap")
    mats = [livery, deck, bottom, tub]
    I_LIV, I_DECK, I_BOT, I_TUB = 0, 1, 2, 3
    tan_dr = math.tan(math.radians(deadrise))
    half = L / 2.0

    xs = [(-1.0 + 2.0 * k / (stations - 1)) * half for k in range(stations)]
    for s in steps:  # a pair of stations a few mm apart makes each step sharp
        xs += [s - 0.004, s + 0.004]
    xs = sorted(xs)

    def step_lift(x):
        return 0.035 * sum(1 for s in steps if s > x)

    rings, ring_uv = [], []
    for x in xs:
        f = x / half
        u = max(0.0, min(1.0, (f - bow_start) / (1.0 - bow_start)))
        # A fine entry: full beam aft, then a long taper to a sharp stem.
        wf = max(0.012, (1.0 - u ** 1.35)) ** 0.8
        hw = B / 2.0 * wf * (1.0 - 0.04 * max(0.0, -f) ** 3)
        zk = keel_z + rocker * u ** 2.2 + step_lift(x)
        zd = deck_z + sheer * u ** 1.6
        zc = zk + 0.86 * hw * tan_dr + 0.02
        zc = min(zc, zd - 0.18)
        pts = [(0.0, zd + crown), (0.45 * hw, zd + crown * 0.72), (0.9 * hw, zd + 0.012), (hw, zd - 0.05)]
        for t in (0.25, 0.5, 0.75):
            pts.append((hw * (1.0 - 0.025 * t), zd - 0.05 + (zc + 0.03 - (zd - 0.05)) * t))
        pts.append((0.985 * hw, zc + 0.03))          # bottom of topsides
        pts.append((0.97 * hw, zc))                  # chine
        pts.append((0.86 * hw, zc - 0.02))           # reverse chine flat
        bottom_pts = []
        if strakes:
            for frac in (0.62, 0.36):
                bottom_pts.append((frac * hw + 0.012, zk + (frac * hw + 0.012) * tan_dr))
                bottom_pts.append((frac * hw, zk + frac * hw * tan_dr - 0.018))
        pts += bottom_pts
        pts.append((0.0, zk))
        # ring: deck centre, then the starboard half, keel, then port half back up
        right = [Vector((x, y, z)) for (y, z) in pts]
        left = [Vector((x, -y, z)) for (y, z) in pts[1:-1]][::-1]
        ring = right + left
        rings.append([bm.verts.new(p) for p in ring])
        # livery UVs per profile point
        ring_uv.append([((x + half) / L, max(0.0, min(1.0, (p.z - zc) / max(zd - zc, 0.05)))) for p in ring])

    n = len(rings[0])
    npts = len(pts)
    # Index ranges in the ring for each kind of surface.
    def jof(k):
        return k if k < npts else (n - k)  # mirror index back onto the starboard profile

    def kind(k):
        j = jof(k)
        return "deck" if j <= 2 else ("side" if j <= 8 else "bottom")

    def face_kind(k, k2):
        # A face between profile points j and j+1 is named by its upper edge.
        j = min(jof(k), jof(k2))
        return "deck" if j <= 1 else ("side" if j <= 7 else "bottom")

    cx = cockpit
    for a in range(len(rings) - 1):
        r0, r1 = rings[a], rings[a + 1]
        xm = (xs[a] + xs[a + 1]) / 2
        for k in range(n):
            k2 = (k + 1) % n
            kd = face_kind(k, k2)
            j = k if k < npts else n - k
            j2 = k2 if k2 < npts else n - k2
            if cx and kd == "deck" and cx[0] <= xm <= cx[1] and max(j, j2) <= 2 and min(j, j2) <= 1:
                continue  # the cockpit opening
            f = bm.faces.new((r0[k], r1[k], r1[k2], r0[k2]))
            f.material_index = {"deck": I_DECK, "side": I_LIV, "bottom": I_BOT}[kd]
            for loop in f.loops:
                ai = 0 if loop.vert in r0 else 1
                kk = (r0 if ai == 0 else r1).index(loop.vert)
                loop[uv].uv = ring_uv[a + ai][kk] if kd == "side" else (0.0, 0.0)
    # Transom: a fan from its centre, raked slightly.
    stern = rings[0]
    centre = bm.verts.new(sum((v.co for v in stern), Vector()) / len(stern))
    for k in range(n):
        f = bm.faces.new((stern[(k + 1) % n], stern[k], centre))
        f.material_index = I_LIV if kind(k) == "side" else (I_DECK if kind(k) == "deck" else I_BOT)
    # Bow: close to a point.
    bow = rings[-1]
    tip = bm.verts.new(Vector((xs[-1] + 0.04, 0.0, sum(v.co.z for v in bow) / len(bow) + 0.05)))
    for k in range(n):
        f = bm.faces.new((bow[k], bow[(k + 1) % n], tip))
        f.material_index = I_LIV if kind(k) == "side" else (I_DECK if kind(k) == "deck" else I_BOT)
    # A sharp stem: crease the bow's last ring and the edges into the tip.
    for e in tip.link_edges:
        e[crease] = 0.9

    bm.edges.ensure_lookup_table()
    # Sharp lines: chine, reverse chine, strakes, steps and the transom edge.
    sharp_j = {8, 9, 10} | ({11, 12, 13, 14} if strakes else set())
    for a in range(len(rings) - 1):
        for k in range(n):
            j = k if k < npts else n - k
            if j in sharp_j:
                e = bm.edges.get((rings[a][k], rings[a + 1][k]))
                if e is not None:
                    e[crease] = 1.0
    # The keel and the deck centreline stay sharp in the forward third.
    for a in range(len(rings) - 1):
        if xs[a] > 0.2 * half:
            for k in (0, npts - 1):
                e = bm.edges.get((rings[a][k], rings[a + 1][k]))
                if e is not None:
                    e[crease] = max(e[crease], 0.6 if k == 0 else 1.0)
    for k in range(n):
        e = bm.edges.get((stern[k], stern[(k + 1) % n]))
        if e is not None:
            e[crease] = 1.0
    for s in steps:
        for a, x in enumerate(xs):
            if abs(x - (s - 0.004)) < 1e-6 or abs(x - (s + 0.004)) < 1e-6:
                for k in range(n):
                    j = k if k < npts else n - k
                    if j >= 9:
                        e = bm.edges.get((rings[a][k], rings[a][(k + 1) % n]))
                        if e is not None:
                            e[crease] = 1.0

    # The cockpit: drop the opening's rim down into a tub with a floor.
    if cx:
        rim = [e for e in bm.edges if e.is_boundary]
        if rim:
            res = bmesh.ops.extrude_edge_only(bm, edges=rim)
            new_verts = [v for v in res["geom"] if isinstance(v, bmesh.types.BMVert)]
            bmesh.ops.translate(bm, vec=Vector((0, 0, -cx[3])), verts=new_verts)
            for v in new_verts:  # tuck the walls in a little so they slope
                v.co.y *= 0.96
            new_edges = [e for e in res["geom"] if isinstance(e, bmesh.types.BMEdge) and e.is_boundary]
            for f in [f for f in res["geom"] if isinstance(f, bmesh.types.BMFace)]:
                f.material_index = I_TUB
            filled = bmesh.ops.holes_fill(bm, edges=new_edges, sides=0)
            for f in filled.get("faces", []):
                f.material_index = I_TUB
            for e in rim:
                if e.is_valid:
                    e[crease] = 0.7
            for e in new_edges:
                if e.is_valid:
                    e[crease] = 1.0
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    mats[0] = livery_material(livery, livery_image)
    obj = to_object(name + "_Hull", bm, mats)
    return subdivide(obj, 2)


# ---------------------------------------------------------------------------
# Parts
# ---------------------------------------------------------------------------
def tube(m, points, radius, mat, segs=8):
    """A pipe through a list of points: rails, T-top frames, wheel rims."""
    for a, b in zip(points, points[1:]):
        a, b = Vector(a), Vector(b)
        d = b - a
        length = d.length
        if length < 1e-4:
            continue
        rot = d.to_track_quat("Z", "Y").to_euler()
        m.cyl(radius, radius, length + radius * 1.2, tuple((a + b) / 2), mat,
              rot=(math.degrees(rot.x), math.degrees(rot.y), math.degrees(rot.z)), segs=segs)
    for p in points[1:-1]:
        m.sphere(radius * 1.02, tuple(p), mat, segs=segs, rings=max(4, segs // 2))


def rounded(m, size, loc, mat, rot=(0, 0, 0), lump=0.0):
    """A soft cushion or cowling: a box that is later subdivided with its part."""
    return m.box(size, loc, mat, rot=rot, smooth=True)


def outboard(m, x, y, z, scale=1.0, cowl="Cowl_111111", stripe="Paint_E53935", trim=8.0):
    """A modern V8 outboard: rounded cowl, midsection, gearcase, skeg, prop."""
    s = scale
    parts = Model("tmp")  # build in its own mesh to subdivide the cowl softly
    parts.box((0.78 * s, 0.56 * s, 0.62 * s), (0, 0, 0.52 * s), cowl)
    parts.box((0.5 * s, 0.5 * s, 0.22 * s), (-0.06 * s, 0, 0.92 * s), cowl)
    obj = to_object("cowl", parts.bm, parts.mats)
    soften(obj, 0.09 * s, 4)
    obj.location = (x, y, z)
    obj.rotation_euler = (0, math.radians(trim), 0)
    # stripe and badge on the cowl sides
    for side in (1, -1):
        m.box((0.7 * s, 0.01, 0.07 * s), (x - 0.02, y + side * 0.285 * s, z + 0.6 * s), stripe)
        m.box((0.25 * s, 0.012, 0.035 * s), (x + 0.1, y + side * 0.287 * s, z + 0.73 * s), "Decal_FAFAFA")
    # midsection and lower unit
    m.box((0.28 * s, 0.2 * s, 0.75 * s), (x + 0.04, y, z - 0.25 * s), "Leg_1A1A1A", rot=(0, trim, 0))
    m.box((0.62 * s, 0.42 * s, 0.03 * s), (x + 0.05, y, z - 0.58 * s), "Leg_1A1A1A", rot=(0, trim, 0))
    m.cyl(0.075 * s, 0.075 * s, 0.5 * s, (x + 0.12, y, z - 0.74 * s), "Leg_1A1A1A", rot=(0, 90 + trim, 0), segs=12)
    m.sphere(0.075 * s, (x + 0.37, y, z - 0.74 * s), "Leg_1A1A1A", segs=12, rings=8)
    m.box((0.22 * s, 0.025, 0.22 * s), (x + 0.05, y, z - 0.9 * s), "Leg_1A1A1A", rot=(0, trim - 20, 0))
    # three-bladed stainless prop, blades twisted
    for k in range(3):
        a = k * 120
        m.box((0.05 * s, 0.17 * s, 0.11 * s), (x - 0.16, y + 0.11 * s * math.cos(math.radians(a)),
               z - 0.74 * s + 0.11 * s * math.sin(math.radians(a))), "Prop_D0D4D8", rot=(a, 25, 0))
    m.cyl(0.04 * s, 0.03 * s, 0.12 * s, (x - 0.16, y, z - 0.74 * s), "Prop_D0D4D8", rot=(0, 90, 0), segs=10)
    return obj


def seat(m, x, y, z, w=0.55, colour="Seat_1C1C1C", accent="Paint_E53935"):
    """A bolster seat: base, curved back and headrest, with a coloured insert."""
    p = Model("tmp")
    p.box((0.55, w, 0.16), (0, 0, 0.08), colour)
    p.box((0.14, w, 0.62), (-0.26, 0, 0.42), colour, rot=(0, -10, 0))
    p.box((0.16, w * 0.62, 0.18), (-0.3, 0, 0.82), colour, rot=(0, -10, 0))
    p.box((0.05, w * 0.55, 0.38), (-0.18, 0, 0.42), accent, rot=(0, -10, 0))
    p.box((0.32, w * 0.55, 0.04), (0.02, 0, 0.17), accent)
    obj = to_object("seat", p.bm, p.mats)
    soften(obj, 0.05, 3)
    obj.location = (x, y, z)
    return obj


def windscreen(m, x_front, half_w, z_base, height=0.42, rake=0.42, sweep=0.55, frame="Frame_111111"):
    """A wraparound screen: a curved band of tinted glass with a black frame."""
    gi = m.mat("Glass_2A3F55")
    fi = m.mat("Frame_111111")
    n = 14
    lower, upper = [], []
    for k in range(n + 1):
        t = -1 + 2 * k / n
        y = t * half_w
        x = x_front - sweep * t * t
        lower.append(m.bm.verts.new((x, y, z_base)))
        upper.append(m.bm.verts.new((x - rake, y * 0.97, z_base + height)))
    for k in range(n):
        f = m.bm.faces.new((lower[k], lower[k + 1], upper[k + 1], upper[k]))
        f.material_index = gi
        f.smooth = True
        # One face is enough: M_Glass in Unreal is two-sided.
    tube(m, [tuple(v.co) for v in upper], 0.02, "Frame_111111", segs=6)
    tube(m, [tuple(v.co) for v in lower], 0.02, "Frame_111111", segs=6)


def nav_lights(m, x_bow, y, z):
    m.sphere(0.035, (x_bow, y, z), "Lamp_FF1744", segs=8, rings=6)
    m.sphere(0.035, (x_bow, -y, z), "Lamp_00E676", segs=8, rings=6)


# ---------------------------------------------------------------------------
# People: built from rounded parts and bevelled, so they read as figures
# rather than stacked blocks. Origin at the seat surface (seated) or the
# soles (standing); facing +X.
# ---------------------------------------------------------------------------
def person(x, y, z, pose="seated", skin="Skin_C68B59", shirt="Paint_E53935", legs="Pants_2B3A55",
           head="hair", hands=None, helmet="Paint_E53935"):
    """A figure from capsules (limbs run continuously through the joints) and
    a softened torso. hands=(x, y, z) in boat space for a driver's grip."""
    p = Model("tmp")
    lift = 0.92 if pose == "standing" else 0.0
    glove = "Glove_151515" if head == "helmet" else skin
    for side in (1, -1):
        hip = (0.0, side * 0.1, lift + 0.08)
        if pose == "standing":
            knee, ankle = (0.02, side * 0.11, 0.48), (0.0, side * 0.11, 0.08)
            toe = (0.16, side * 0.11, 0.04)
        else:
            knee, ankle = (0.42, side * 0.11, 0.1), (0.48, side * 0.12, -0.34)
            toe = (0.62, side * 0.12, -0.38)
        tube(p, [hip, knee, ankle], 0.075, legs, segs=10)
        tube(p, [ankle, toe], 0.055, "Shoe_202020", segs=8)
        shoulder = (-0.03, side * 0.22, lift + 0.66)
        if hands is not None:
            grip = (hands[0] - x, hands[1] - y + side * 0.12, hands[2] - z)
            elbow = ((shoulder[0] + grip[0]) * 0.5 - 0.02, side * 0.27, (shoulder[2] + grip[2]) * 0.5 - 0.12)
        elif pose == "standing":
            grip = (0.12, side * 0.24, lift + 0.12)
            elbow = (0.0, side * 0.26, lift + 0.38)
        else:
            grip = (0.3, side * 0.18, 0.2)
            elbow = (0.02, side * 0.25, 0.4)
        tube(p, [shoulder, elbow], 0.055, shirt, segs=10)
        tube(p, [elbow, grip], 0.045, skin if head != "helmet" else shirt, segs=10)
        p.sphere(0.055, grip, glove, segs=10, rings=6)
    torso = Model("tmp2")
    torso.box((0.24, 0.4, 0.52), (-0.03, 0, lift + 0.42), shirt, rot=(0, -5, 0))
    torso.box((0.24, 0.34, 0.2), (-0.01, 0, lift + 0.12), legs)
    tobj = to_object("torso", torso.bm, torso.mats)
    soften(tobj, 0.09, 4)
    p.cyl(0.055, 0.055, 0.12, (-0.03, 0, lift + 0.72), skin, segs=10)
    if head == "helmet":
        p.sphere(0.16, (-0.02, 0, lift + 0.88), helmet, scale=(1.05, 0.95, 1.0), segs=18, rings=12)
        p.sphere(0.13, (0.05, 0, lift + 0.87), "Glass_0B0F14", scale=(0.75, 0.95, 0.5), segs=16, rings=8)
    else:
        p.sphere(0.115, (-0.02, 0, lift + 0.86), skin, scale=(1.0, 0.9, 1.1), segs=16, rings=10)
        p.sphere(0.12, (-0.045, 0, lift + 0.9), "Hair_2B1D14", scale=(1.0, 0.95, 0.75), segs=16, rings=8)
    obj = to_object("person", p.bm, p.mats)
    for o in (obj, tobj):
        o.location = (x, y, z)
    return [obj, tobj]


# ---------------------------------------------------------------------------
# The race boat: a 7.2 m twin-step offshore performance hull, twin outboards
# ---------------------------------------------------------------------------
def offshore_boat():
    liv = make_livery("T_Livery", style=0)
    hull = smooth_hull("RaceBoat", L=7.2, B=2.35, deck_z=0.5, keel_z=-0.58, deadrise=24, bow_start=0.18,
                       rocker=0.8, sheer=0.12, crown=0.06, steps=(-0.6, 0.7), stations=28,
                       cockpit=(-2.75, -0.15, 0.0, 0.55), livery="Livery_FFFFFF", livery_image=liv,
                       deck="Deck_EDEFF1", bottom="Bottom_E8EAED", tub="Sole_2B2E33")
    m = Model("RaceBoatParts")
    # Windscreen just ahead of the cockpit, and the dash under it.
    windscreen(m, x_front=0.15, half_w=0.98, z_base=0.6, height=0.36, rake=0.5, sweep=0.5)
    m.box((0.3, 1.5, 0.32), (-0.25, 0, 0.36), "Dash_17191C", rot=(0, -25, 0))
    for k, y in enumerate((-0.45, -0.25, 0.25, 0.45)):
        m.cyl(0.055, 0.055, 0.02, (-0.15, y, 0.47), "Lamp_80DEEA", rot=(0, 65, 0), segs=12)
    tube(m, [(-0.32, 0.42 + 0.17 * math.cos(a / 8 * math.tau), 0.62 + 0.17 * math.sin(a / 8 * math.tau)) for a in range(9)],
         0.018, "Wheel_111111", segs=6)
    m.box((0.08, 0.04, 0.2), (-0.3, -0.35, 0.55), "Chrome_C7CCD1", rot=(0, -30, 0))
    m.box((0.08, 0.04, 0.2), (-0.3, -0.45, 0.55), "Chrome_C7CCD1", rot=(0, -30, 0))
    seats = [seat(m, -0.95, y, 0.0, w=0.55, accent="Paint_E53935") for y in (0.42, -0.42)]
    seats += [seat(m, -1.9, y, 0.0, w=0.55, accent="Paint_E53935") for y in (0.42, -0.42)]
    # Stainless grab rails round the cockpit and a bow rail.
    for side in (1, -1):
        tube(m, [(-2.6, side * 1.0, 0.62), (-1.6, side * 1.03, 0.66), (-0.4, side * 1.0, 0.68)], 0.018, "Chrome_C7CCD1")
        for x in (-2.4, -1.4, -0.5):
            tube(m, [(x, side * 1.0, 0.52), (x, side * 1.0, 0.65)], 0.015, "Chrome_C7CCD1")
        tube(m, [(1.8, side * 0.62, 0.7), (2.7, side * 0.38, 0.72), (3.2, side * 0.12, 0.74)], 0.016, "Chrome_C7CCD1")
        m.box((0.16, 0.05, 0.05), (1.2, side * 0.95, 0.56), "Chrome_C7CCD1")  # cleats
        m.box((0.16, 0.05, 0.05), (-3.1, side * 0.95, 0.53), "Chrome_C7CCD1")
    nav_lights(m, 3.3, 0.16, 0.66)
    m.cyl(0.02, 0.012, 1.1, (-3.1, 0.0, 1.05), "Chrome_C7CCD1", segs=6)        # stern light pole
    m.sphere(0.04, (-3.1, 0.0, 1.6), "Lamp_FFFFFF", segs=8, rings=6)
    # Engine-hatch sunpad and a hatch outline on the foredeck.
    m.box((1.2, 1.7, 0.1), (-3.0, 0, 0.6), "Cushion_1C1C1C")
    m.box((1.15, 0.12, 0.11), (-3.0, 0, 0.6), "Paint_E53935")
    m.box((0.9, 0.8, 0.012), (1.55, 0, 0.6), "Hatch_C9CDD2")
    # Transom bracket, trim tabs and twin outboards.
    m.box((0.3, 1.5, 0.55), (-3.72, 0, 0.12), "Bracket_1A1C1F")
    for side in (1, -1):
        m.box((0.3, 0.32, 0.02), (-3.74, side * 0.75, -0.42), "Chrome_C7CCD1")
    motors = [outboard(m, -4.0, y, 0.3, scale=1.0, stripe="Paint_E53935") for y in (0.42, -0.42)]
    # The crew: a driver at the wheel and a throttleman beside, helmeted in
    # the team colour.
    crew = person(-0.98, 0.42, 0.18, hands=(-0.3, 0.42, 0.62), head="helmet", shirt="Suit_1E2126", legs="Suit_1E2126",
                  helmet="Paint_E53935")
    crew += person(-0.98, -0.42, 0.18, hands=(-0.3, -0.42, 0.6), head="helmet", shirt="Suit_1E2126", legs="Suit_1E2126",
                   helmet="Paint_E53935")
    details = to_object("RaceBoatParts", m.bm, m.mats, smooth=False)
    return join_and_export("RaceBoat", [hull, details] + seats + motors + crew)


def center_console():
    """A 10 m centre console day boat with a T-top and triple outboards."""
    liv = bpy.data.images.get("T_LiveryCruise") or make_livery("T_LiveryCruise", style=1)
    hull = smooth_hull("CenterConsole", L=10.0, B=3.0, deck_z=0.85, keel_z=-0.62, deadrise=22, bow_start=0.12,
                       rocker=1.0, sheer=0.35, crown=0.05, steps=(-0.8,), stations=26,
                       cockpit=(-4.1, 3.1, 0.0, 0.62), livery="LiveryCruise_FFFFFF", livery_image=liv,
                       deck="Deck_F2F2F2", bottom="Bottom_F5F5F5", tub="Sole_E3E6E9")
    m = Model("CCParts")
    m.box((1.2, 1.1, 1.1), (0.6, 0, 0.75), "Console_F7F7F7")
    windscreen(m, x_front=1.25, half_w=0.6, z_base=1.3, height=0.45, rake=0.35, sweep=0.25)
    m.box((0.4, 0.9, 0.06), (0.75, 0, 1.31), "Dash_17191C", rot=(0, -20, 0))
    # T-top: four powder-coated legs, a hard top with a canvas edge and lights.
    for dx in (-0.5, 1.1):
        for dy in (-0.7, 0.7):
            tube(m, [(dx, dy, 0.25), (dx, dy, 2.5)], 0.035, "Frame_E0E3E6")
    m.box((2.4, 1.9, 0.08), (0.3, 0, 2.55), "Top_FAFAFA")
    m.box((2.42, 1.92, 0.05), (0.3, 0, 2.5), "Paint_1565C0")
    m.box((2.0, 0.05, 0.02), (0.3, 0.9, 2.47), "Lamp_B3E5FC")
    m.box((2.0, 0.05, 0.02), (0.3, -0.9, 2.47), "Lamp_B3E5FC")
    m.box((0.7, 1.2, 0.85), (-0.8, 0, 0.45), "Seat_ECEFF1")          # leaning post
    m.box((0.2, 1.2, 0.6), (-1.15, 0, 1.0), "Seat_ECEFF1")
    for k in range(5):                                                  # rod holders
        m.cyl(0.025, 0.025, 0.3, (-1.3, -0.5 + k * 0.25, 1.5), "Chrome_C7CCD1", rot=(15, 0, 0), segs=6)
    for side in (1, -1):
        tube(m, [(2.0, side * 1.05, 1.05), (3.6, side * 0.6, 1.18), (4.6, side * 0.18, 1.22)], 0.02, "Chrome_C7CCD1")
        m.box((3.5, 0.2, 0.14), (-1.5, side * 1.28, 0.85), "Coaming_F2F2F2")
    nav_lights(m, 4.7, 0.2, 1.05)
    m.box((0.35, 2.1, 0.5), (-5.15, 0, 0.35), "Bracket_EDEFF1")
    motors = [outboard(m, -5.4, y, 0.55, scale=1.1, cowl="Cowl_F7F7F7", stripe="Paint_1565C0") for y in (-0.7, 0.0, 0.7)]
    skipper = person(-0.15, 0.0, 0.25, pose="standing", shirt="Shirt_F5F5F5", legs="Pants_C9B38E", skin="Skin_8D5524")
    guest = person(-1.1, 0.3, 0.75, shirt="Shirt_26A69A", legs="Pants_37474F", skin="Skin_F1C27D")
    details = to_object("CCParts", m.bm, m.mats, smooth=False)
    return join_and_export("CenterConsole", [hull, details] + motors + skipper + guest)


def jet_ski():
    """A 3.4 m stand-off personal watercraft."""
    hull = smooth_hull("JetSki", L=3.3, B=1.2, deck_z=0.32, keel_z=-0.25, deadrise=20, bow_start=0.2,
                       rocker=0.35, sheer=0.1, crown=0.18, stations=16, strakes=False,
                       livery="LiveryCruise_FFFFFF", livery_image=bpy.data.images.get("T_LiveryCruise"),
                       deck="Deck_2B2E33", bottom="Bottom_1C1E21")
    m = Model("JetParts")
    p = Model("tmp")
    p.box((1.1, 0.42, 0.22), (-0.35, 0, 0.5), "Seat_111111")
    p.box((0.9, 0.72, 0.28), (0.75, 0, 0.5), "Paint_FF6D00")
    obj = to_object("jetseat", p.bm, p.mats)
    soften(obj, 0.08, 4)
    tube(m, [(0.45, 0, 0.6), (0.3, 0, 0.95)], 0.04, "Leg_1A1A1A")
    tube(m, [(0.3, -0.38, 0.98), (0.3, 0.38, 0.98)], 0.022, "Wheel_111111")
    windscreen(m, x_front=0.95, half_w=0.32, z_base=0.66, height=0.16, rake=0.2, sweep=0.12)
    m.box((0.25, 0.5, 0.1), (-1.6, 0, -0.05), "Leg_1A1A1A")
    rider = person(-0.35, 0.0, 0.62, shirt="Shirt_FFEB3B", legs="Pants_1A237E", skin="Skin_C68B59")
    details = to_object("JetParts", m.bm, m.mats, smooth=False)
    return join_and_export("JetSki", [hull, details, obj] + rider)


def spray_sheet():
    """A fan of bow spray, opaque at the root and fading to nothing at the
    tips through the vertex alpha. Lies along +X from the origin, rising."""
    bm = bmesh.new()
    col = bm.loops.layers.float_color.new("Col")
    rows, cols = 6, 9
    grid = []
    for r in range(rows + 1):
        t = r / rows
        row = []
        for c in range(cols + 1):
            s = c / cols
            ang = math.radians(-60 + 120 * s)
            reach = 0.3 + 2.6 * t
            x = reach * math.cos(ang) * 0.6 + t * 0.4
            y = reach * math.sin(ang) * 0.5
            z = 1.4 * t * (1.0 - 0.45 * t) + 0.15 * math.sin(s * math.pi) * t
            row.append(bm.verts.new((x, y + t * 0.9, z)))
        grid.append(row)
    for r in range(rows):
        for c in range(cols):
            f = bm.faces.new((grid[r][c], grid[r][c + 1], grid[r + 1][c + 1], grid[r + 1][c]))
            for loop in f.loops:
                p = loop.vert.co
                t = min(1.0, max(0.0, (p.z / 1.0)))
                edge = 1.0 - abs((math.atan2(p.y - t * 0.9, max(p.x - t * 0.4, 0.01)) / math.radians(60)))
                loop[col] = (1.0, 1.0, 1.0, max(0.0, min(1.0, (1.0 - t) * 1.3)) * max(0.0, min(1.0, edge * 2.5)))
    obj = to_object("SpraySheet", bm, ["Spray_FFFFFF"], smooth=True)
    obj.data.color_attributes.active_color = obj.data.color_attributes.get("Col")
    return join_and_export("SpraySheet", [obj])


def yacht_v2():
    """A 16 m express motor yacht: smooth hull, glazed superstructure, arch."""
    liv = bpy.data.images.get("T_LiveryCruise")
    hull = smooth_hull("Yacht", L=16.0, B=4.6, deck_z=1.6, keel_z=-1.0, deadrise=18, bow_start=0.1,
                       rocker=1.4, sheer=0.6, crown=0.08, stations=26, livery="LiveryCruise_FFFFFF", livery_image=liv,
                       deck="Deck_C8A27A", bottom="Bottom_23262B", cockpit=(-7.4, -4.6, 0.0, 0.5))
    m = Model("YachtParts")
    p = Model("tmp")
    p.box((7.5, 3.8, 1.5), (-0.8, 0, 2.4), "Cabin_FAFAFA")
    p.box((4.2, 3.0, 1.0), (-1.5, 0, 3.5), "Cabin_FAFAFA")
    sup = to_object("sup", p.bm, p.mats)
    soften(sup, 0.25, 4)
    # Dark glazing bands wrapped round the cabin sides, raked like a real express cruiser.
    m.box((6.6, 3.84, 0.5), (-0.6, 0, 2.6), "Window_1E2A36", rot=(0, -2, 0))
    m.box((3.6, 3.04, 0.42), (-1.3, 0, 3.55), "Window_1E2A36", rot=(0, -3, 0))
    tube(m, [(-3.0, -1.4, 4.0), (-2.4, -1.2, 5.0), (-2.4, 1.2, 5.0), (-3.0, 1.4, 4.0)], 0.09, "Frame_E0E3E6")
    m.cyl(0.35, 0.35, 0.12, (-2.4, 0, 5.15), "Radar_FAFAFA", segs=16)
    for side in (1, -1):
        tube(m, [(-6.5, side * 2.2, 2.2), (2.0, side * 2.25, 2.3), (6.5, side * 0.8, 2.5), (7.6, side * 0.2, 2.55)], 0.03, "Chrome_C7CCD1")
    m.box((1.8, 4.2, 0.15), (-8.6, 0, 0.35), "Deck_C8A27A")             # swim platform
    details = to_object("YachtParts", m.bm, m.mats, smooth=False)
    return join_and_export("Yacht", [hull, details, sup])


def spectator(name, skin, hair):
    parts = person(0, 0, 0, shirt="Paint_E53935", legs="Pants_37474F", skin=skin)
    for o in parts:
        for slot in o.data.materials:
            pass
    return join_and_export(name, parts)


def ripple():
    """A raindrop ring for the water: bright at the rim, clear inside and out."""
    bm = bmesh.new()
    col = bm.loops.layers.float_color.new("Col")
    radii, alphas = (0.0, 0.78, 0.9, 1.0), (0.0, 0.0, 1.0, 0.0)
    rings = []
    for r in radii:
        rings.append([bm.verts.new((r * math.cos(k / 24 * math.tau), r * math.sin(k / 24 * math.tau), 0.0)) for k in range(24)])
    for a in range(len(rings) - 1):
        for k in range(24):
            f = bm.faces.new((rings[a][k], rings[a][(k + 1) % 24], rings[a + 1][(k + 1) % 24], rings[a + 1][k]))
            for loop in f.loops:
                r = loop.vert.co.length
                i = min(range(4), key=lambda q: abs(radii[q] - r))
                loop[col] = (1.0, 1.0, 1.0, alphas[i])
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-5)
    obj = to_object("Ripple", bm, ["Foam_FFFFFF"], smooth=True)
    obj.data.color_attributes.active_color = obj.data.color_attributes.get("Col")
    return join_and_export("Ripple", [obj])


BOATS = [offshore_boat, center_console, jet_ski, spray_sheet, yacht_v2, ripple,
         lambda: spectator("SpectatorA", "Skin_C68B59", "Hair_2B1D14"),
         lambda: spectator("SpectatorB", "Skin_8D5524", "Hair_111111"),
         lambda: spectator("SpectatorC", "Skin_F1C27D", "Hair_A0522D")]
