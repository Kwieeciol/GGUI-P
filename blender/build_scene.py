# -*- coding: utf-8 -*-
"""
build_scene.py  -  "Latarnia morska w platku sniegu"
Blender 4.2+ (testowane w Blenderze 5.2 LTS).

Uruchomienie:
  * Blender > Scripting > Open > build_scene.py > Run Script
  * albo z konsoli:  blender -b -P build_scene.py -- <katalog_wyjsciowy>

Skrypt:
  1. czysci scene,
  2. buduje platek (Boolean Union prostych bryl + wneka powietrzna na latarnie),
     latarnie, skale i drobne platki/kulki sniegu,
  3. ustawia materialy (Principled BSDF) i kamere,
  4. sprawdza limity trojkatow, manifold, normalne, samoprzeciecia i odstepy,
  5. eksportuje scene.obj/.mtl + snowflake/lighthouse/rock.obj, zapisuje scene.blend
     oraz raport scene_report.md / scene_stats.json.

Uklad: Blender Z w gore. Eksport: Forward -Z, Up Y  =>  (x, y, z)_blender -> (x, z, -y)_obj.
Wszystkie obiekty sa budowane od razu we wspolrzednych swiata (transformacje = identycznosc).
"""
import bpy, bmesh, math, os, sys, json, random
from mathutils import Vector, Matrix, Euler
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

# =====================================================================
#  PARAMETRY
# =====================================================================
SEG_TOWER = 32          # segmenty walcow/stozkow wiezy (24-32)
SEG_SMALL = 24          # laterna, dach, barierka
SEG_ROCK = 20

# --- platek (budowany w plaszczyznie XY, potem obracany do XZ: stoi pionowo, przodem do -Y)
FLAKE_R_CORE = 1.15       # promien (do wierzcholka) szesciokatnego rdzenia
FLAKE_CORE_HALF = 0.55    # polowa grubosci rdzenia
FLAKE_CORE_BEVEL = 0.32   # od tej glebokosci rdzen sie zweza (fazowanie)
FLAKE_CORE_FACE = 0.80    # skala sciany czolowej wzgledem R
FLAKE_PLATE_R = 1.40      # cienka szesciokatna plytka (wierzcholki miedzy ramionami)
FLAKE_PLATE_HALF = 0.07
ARM_START, ARM_END = 0.60, 1.84
ARM_W, ARM_HALF = 0.17, 0.09
TIP_R, TIP_HALF = 0.15, 0.08         # szesciokatny "krysztalek" na koncu ramienia
BRANCHES = [                          # (pozycja na ramieniu, dlugosc, szerokosc)
    (1.32, 0.40, 0.10),
    (1.56, 0.30, 0.09),
    (1.76, 0.17, 0.08),
]
BRANCH_HALF = 0.06
BRANCH_ANGLE = 60.0
N_ARMS = 6

# --- wneka powietrzna w rdzeniu (szesciokatny graniastoslup z daszkiem), profil (r, z)
CAVITY = [(0.0, -0.81), (0.46, -0.81), (0.46, 0.50), (0.22, 0.84), (0.0, 0.84)]

# --- skala i latarnia
ROCK_BASE_Z = -0.78
ROCK_H = 0.20
LH_BASE_Z = ROCK_BASE_Z + ROCK_H       # latarnia: od -0.58 do +0.62 (wysokosc 1.2)
LH_HEIGHT = 1.20

# --- snieg
N_BALLS = 10
N_STARS = 5
SEED = 2026

# --- kamera
CAM_LOC = (0.0, -8.5, 2.0)
CAM_TARGET = (0.0, 0.0, 0.0)
CAM_VFOV = 30.0
RESOLUTION = (1920, 1080)

LIMITS = dict(total=15000, snowflake=6000, lighthouse=4000, rock=1500, snow_piece=200)
GAP_MIN = 0.02

# --- materialy (nazwa -> parametry dla Blendera i dla MTL)
MATERIALS = {
    "ICE":        dict(kind="dielectric", color=(0.85, 0.93, 1.0), ior=1.31),
    "GLASS":      dict(kind="dielectric", color=(1.0, 1.0, 1.0), ior=1.5),
    "LAMP_LIGHT": dict(kind="emissive", color=(1.0, 0.85, 0.5), strength=15.0),
    "WALL_WHITE": dict(kind="lambertian", color=(0.9, 0.9, 0.88)),
    "WALL_RED":   dict(kind="lambertian", color=(0.7, 0.08, 0.06)),
    "METAL_DARK": dict(kind="metal", color=(0.2, 0.2, 0.22), fuzz=0.2),
    "ROCK":       dict(kind="lambertian", color=(0.35, 0.33, 0.3)),
    "SNOW":       dict(kind="lambertian", color=(0.95, 0.95, 0.97)),
}


# =====================================================================
#  NARZEDZIA
# =====================================================================
def V(x, y, z):
    return Vector((x, y, z))


def out_dir():
    argv = sys.argv
    if "--" in argv and len(argv) > argv.index("--") + 1:
        return os.path.abspath(argv[argv.index("--") + 1])
    f = globals().get("__file__")
    if f and os.path.isfile(f):
        return os.path.dirname(os.path.abspath(f))
    if bpy.data.filepath:
        return os.path.dirname(bpy.data.filepath)
    return os.getcwd()


def clean_scene():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    for coll in (bpy.data.meshes, bpy.data.materials, bpy.data.cameras,
                 bpy.data.lights, bpy.data.curves):
        for d in list(coll):
            if d.users == 0:
                coll.remove(d)
    for c in list(bpy.data.collections):
        bpy.data.collections.remove(c)


def lathe(bm, prof, seg, M=Matrix(), phase=0.0, jitter=None):
    """Bryla obrotowa z profilu [(r, z), ...] od dolu do gory. r=0 -> biegun (zamkniecie)."""
    rings = []
    for i, (r, z) in enumerate(prof):
        if r <= 1e-9:
            rings.append([bm.verts.new(M @ V(0, 0, z))])
        else:
            ring = []
            for k in range(seg):
                a = phase + 2 * math.pi * k / seg
                rr, zz = jitter(i, k, r, z) if jitter else (r, z)
                ring.append(bm.verts.new(M @ V(rr * math.cos(a), rr * math.sin(a), zz)))
            rings.append(ring)
    for i in range(len(rings) - 1):
        A, B = rings[i], rings[i + 1]
        if len(A) == 1 and len(B) == 1:
            continue
        for k in range(seg):
            k1 = (k + 1) % seg
            if len(A) == 1:
                bm.faces.new((A[0], B[k1], B[k]))
            elif len(B) == 1:
                bm.faces.new((A[k], A[k1], B[0]))
            else:
                bm.faces.new((A[k], A[k1], B[k1], B[k]))
    if len(rings[0]) > 1:
        bm.faces.new(list(reversed(rings[0])))
    if len(rings[-1]) > 1:
        bm.faces.new(rings[-1])


def prism(r, half):
    return [(0, -half), (r, -half), (r, half), (0, half)]


def box(bm, center, size, R=Matrix(), M=Matrix()):
    mat = M @ Matrix.Translation(center) @ R @ Matrix.Diagonal((size[0], size[1], size[2], 1.0))
    bmesh.ops.create_cube(bm, size=1.0, matrix=mat)


def tube(bm, ro, ri, z0, z1, seg, M=Matrix()):
    """Zamknieta rura (np. szyba laterny) - bryla o grubosci ro-ri."""
    def ring(r, z):
        return [bm.verts.new(M @ V(r * math.cos(2 * math.pi * k / seg),
                                   r * math.sin(2 * math.pi * k / seg), z)) for k in range(seg)]
    ob, ot, ib, it = ring(ro, z0), ring(ro, z1), ring(ri, z0), ring(ri, z1)
    for k in range(seg):
        k1 = (k + 1) % seg
        bm.faces.new((ob[k], ob[k1], ot[k1], ot[k]))
        bm.faces.new((ib[k1], ib[k], it[k], it[k1]))
        bm.faces.new((ot[k], ot[k1], it[k1], it[k]))
        bm.faces.new((ob[k1], ob[k], ib[k], ib[k1]))


def torus(bm, R, r, segU, segV, M=Matrix()):
    g = [[bm.verts.new(M @ V((R + r * math.cos(v)) * math.cos(u),
                             (R + r * math.cos(v)) * math.sin(u), r * math.sin(v)))
          for v in [2 * math.pi * j / segV for j in range(segV)]]
         for u in [2 * math.pi * i / segU for i in range(segU)]]
    for i in range(segU):
        for j in range(segV):
            i1, j1 = (i + 1) % segU, (j + 1) % segV
            bm.faces.new((g[i][j], g[i1][j], g[i1][j1], g[i][j1]))


def star6(bm, M, ro, ri, half):
    pts = []
    for k in range(12):
        r = ro if k % 2 == 0 else ri
        a = math.pi * k / 6
        pts.append((r * math.cos(a), r * math.sin(a)))
    bot = [bm.verts.new(M @ V(x, y, -half)) for x, y in pts]
    top = [bm.verts.new(M @ V(x, y, half)) for x, y in pts]
    bm.faces.new(list(reversed(bot)))
    bm.faces.new(top)
    for k in range(12):
        k1 = (k + 1) % 12
        bm.faces.new((bot[k], bot[k1], top[k1], top[k]))


def dissolve_collinear(bm, tol_deg=0.01):
    """Usuwa wierzcholki lezace na prostej krawedzi (stopien 2) - zrodlo trojkatow o zerowym polu."""
    lim = math.cos(math.radians(180.0 - tol_deg))
    vs = []
    for v in bm.verts:
        if len(v.link_edges) == 2:
            a = v.link_edges[0].other_vert(v).co - v.co
            b = v.link_edges[1].other_vert(v).co - v.co
            if a.length > 0 and b.length > 0 and a.normalized().dot(b.normalized()) < lim:
                vs.append(v)
    if vs:
        bmesh.ops.dissolve_verts(bm, verts=vs)


def cleanup_triangulate(bm):
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-5)
    bmesh.ops.dissolve_degenerate(bm, edges=bm.edges[:], dist=1e-7)
    dissolve_collinear(bm)
    bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method='BEAUTY', ngon_method='BEAUTY')


def degenerate_tris(bm, eps=1e-9):
    return sum(1 for f in bm.faces if f.calc_area() < eps)


def apply_shading(bm, smooth, angle=40.0):
    for f in bm.faces:
        f.smooth = smooth
    if smooth:
        lim = math.radians(angle)
        for e in bm.edges:
            if len(e.link_faces) == 2 and e.calc_face_angle(0.0) > lim:
                e.smooth = False


def obj_from_bm(name, bm, mat=None, collection=None):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    me.update()
    ob = bpy.data.objects.new(name, me)
    (collection or bpy.context.scene.collection).objects.link(ob)
    if mat:
        me.materials.append(mat)
    return ob


def finalize(name, bm, mat, smooth=False, recalc=True, collection=None):
    cleanup_triangulate(bm)
    if recalc:
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    apply_shading(bm, smooth)
    ob = obj_from_bm(name, bm, mat, collection)
    bm.free()
    return ob


def bm_of(ob):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bm.transform(ob.matrix_world)
    bm.faces.ensure_lookup_table()
    return bm


def mesh_check(bm):
    bm.edges.ensure_lookup_table()
    boundary = sum(1 for e in bm.edges if e.is_boundary)
    non_manifold = sum(1 for e in bm.edges if not e.is_manifold)
    flipped = sum(1 for e in bm.edges if e.is_manifold and not e.is_contiguous)
    nm_verts = sum(1 for v in bm.verts if not v.is_manifold)
    kd = KDTree(len(bm.verts))
    for i, v in enumerate(bm.verts):
        kd.insert(v.co, i)
    kd.balance()
    dup = sum(1 for v in bm.verts if len(kd.find_range(v.co, 1e-6)) > 1) // 2
    vol = bm.calc_volume(signed=True)
    ok = boundary == 0 and non_manifold == 0 and flipped == 0 and nm_verts == 0 and dup == 0 and vol > 0
    return dict(manifold=ok, boundary_edges=boundary, non_manifold_edges=non_manifold,
                inconsistent_normals=flipped, non_manifold_verts=nm_verts,
                duplicate_verts=dup, signed_volume=round(vol, 5))


def self_intersections(bm):
    bm.faces.ensure_lookup_table()
    tree = BVHTree.FromBMesh(bm)
    bad = 0
    for i, j in tree.overlap(tree):
        if i >= j:
            continue
        if set(bm.faces[i].verts) & set(bm.faces[j].verts):
            continue
        bad += 1
    return bad


def convex_margin(points, bm_convex):
    """Min. odleglosc punktow od scian wypuklej bryly (dodatnia = w srodku)."""
    planes = [(f.calc_center_median(), f.normal.copy()) for f in bm_convex.faces]
    return min(min(-(p - c).dot(n) for c, n in planes) for p in points)


# =====================================================================
#  MATERIALY
# =====================================================================
def make_materials():
    mats = {}
    for name, p in MATERIALS.items():
        m = bpy.data.materials.new(name)
        if hasattr(m, "use_nodes"):
            try:
                m.use_nodes = True
            except Exception:
                pass
        b = m.node_tree.nodes.get("Principled BSDF")
        col = (*p["color"], 1.0)
        b.inputs["Base Color"].default_value = col
        b.inputs["Roughness"].default_value = 1.0
        b.inputs["Metallic"].default_value = 0.0
        if p["kind"] == "dielectric":
            b.inputs["Transmission Weight"].default_value = 1.0
            b.inputs["Roughness"].default_value = 0.0
            b.inputs["IOR"].default_value = p["ior"]
        elif p["kind"] == "metal":
            b.inputs["Metallic"].default_value = 1.0
            b.inputs["Roughness"].default_value = p["fuzz"]
        elif p["kind"] == "emissive":
            b.inputs["Emission Color"].default_value = col
            b.inputs["Emission Strength"].default_value = p["strength"]
        m.diffuse_color = col
        mats[name] = m
    return mats


# =====================================================================
#  PLATEK SNIEGU
# =====================================================================
ROT_FLAKE = Matrix.Rotation(math.radians(90.0), 4, 'X')   # plaszczyzna XY -> XZ


def core_profile():
    R, h, b, s = FLAKE_R_CORE, FLAKE_CORE_HALF, FLAKE_CORE_BEVEL, FLAKE_CORE_FACE
    return [(0, -h), (s * R, -h), (R, -b), (R, b), (s * R, h), (0, h)]


def build_core_bm():
    bm = bmesh.new()
    lathe(bm, core_profile(), 6, M=ROT_FLAKE, phase=math.radians(30))  # wierzcholek w gore
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    return bm


def build_cavity_bm():
    bm = bmesh.new()
    lathe(bm, CAVITY, 6, phase=0.0)              # sciana plaska zwrocona do kamery (-Y)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def build_snowflake(mat, solver='EXACT'):
    scene = bpy.context.scene
    parts_col = bpy.data.collections.new("_flake_parts")
    scene.collection.children.link(parts_col)

    def part(fn):
        bm = bmesh.new()
        fn(bm)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        return obj_from_bm("_flake_part", bm, None, parts_col)

    core_bm = build_core_bm()
    target = obj_from_bm("Snowflake", core_bm, None)
    core_bm.free()

    part(lambda bm: lathe(bm, prism(FLAKE_PLATE_R, FLAKE_PLATE_HALF), 6, M=ROT_FLAKE, phase=0.0))
    for k in range(N_ARMS):
        Mk = ROT_FLAKE @ Matrix.Rotation(math.radians(30 + 60 * k), 4, 'Z')
        part(lambda bm, Mk=Mk: box(bm, V((ARM_START + ARM_END) / 2, 0, 0),
                                   (ARM_END - ARM_START, ARM_W, 2 * ARM_HALF), M=Mk))
        part(lambda bm, Mk=Mk: lathe(bm, prism(TIP_R, TIP_HALF), 6,
                                     M=Mk @ Matrix.Translation((ARM_END, 0, 0)), phase=0.0))
        for (p, L, w) in BRANCHES:
            for s in (1, -1):
                a = s * math.radians(BRANCH_ANGLE)
                c = V(p + math.cos(a) * L / 2, math.sin(a) * L / 2, 0)
                part(lambda bm, Mk=Mk, c=c, a=a, L=L, w=w:
                     box(bm, c, (L, w, 2 * BRANCH_HALF), R=Matrix.Rotation(a, 4, 'Z'), M=Mk))

    mod = target.modifiers.new("Union", 'BOOLEAN')
    mod.operation = 'UNION'
    mod.operand_type = 'COLLECTION'
    mod.collection = parts_col
    mod.solver = solver
    dg = bpy.context.evaluated_depsgraph_get()
    me_new = bpy.data.meshes.new_from_object(target.evaluated_get(dg))
    target.modifiers.clear()
    old = target.data
    target.data = me_new
    me_new.name = "Snowflake"
    bpy.data.meshes.remove(old)
    for o in list(parts_col.objects):
        me = o.data
        bpy.data.objects.remove(o, do_unlink=True)
        bpy.data.meshes.remove(me)
    bpy.data.collections.remove(parts_col)

    # sprzatanie + normalne na zewnatrz
    bm = bm_of(target)
    cleanup_triangulate(bm)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    outer_ok = mesh_check(bm)

    # wneka: osobna, zamknieta powloka z normalnymi skierowanymi DO wneki (na zewnatrz lodu)
    cav = build_cavity_bm()
    bmesh.ops.reverse_faces(cav, faces=cav.faces[:])
    vmap = {v: bm.verts.new(v.co) for v in cav.verts}
    for f in cav.faces:
        bm.faces.new([vmap[v] for v in f.verts])
    cav.free()
    apply_shading(bm, smooth=False)
    for f in bm.faces:
        f.material_index = 0
    bm.to_mesh(target.data)
    bm.free()
    target.data.materials.clear()
    target.data.materials.append(mat)
    return target, outer_ok


# =====================================================================
#  SKALA, LATARNIA, SNIEG
# =====================================================================
def build_rock(mat):
    rnd = random.Random(SEED + 7)
    prof = [(0, 0), (0.27, 0), (0.30, 0.035), (0.31, 0.08), (0.28, 0.13),
            (0.235, 0.17), (0.21, ROCK_H), (0, ROCK_H)]
    jit = {}

    def jitter(i, k, r, z):
        key = (i, k)
        if key not in jit:
            if i in (2, 3, 4, 5):
                jit[key] = (r * (1 + rnd.uniform(-0.10, 0.10)), z + rnd.uniform(-0.012, 0.012))
            elif i == 1:
                jit[key] = (r * (1 + rnd.uniform(-0.06, 0.06)), z)
            else:
                jit[key] = (r, z)
        return jit[key]

    bm = bmesh.new()
    lathe(bm, prof, SEG_ROCK, M=Matrix.Translation((0, 0, ROCK_BASE_Z)), jitter=jitter)
    return finalize("Rock", bm, mat, smooth=False)


def tower_r(h):
    return 0.17 - (h - 0.05) * (0.054 / 0.715)


def build_lighthouse(mats):
    M = Matrix.Translation((0, 0, LH_BASE_Z))
    objs = []
    # pasy wiezy (geometria, nie tekstura): od dolu czerwony, bialy, ...
    n_bands, h0, h1 = 6, 0.05, 0.765
    white, red = bmesh.new(), bmesh.new()
    lathe(white, [(0, 0), (0.20, 0), (0.20, 0.05), (0, 0.05)], SEG_TOWER, M=M)   # cokol
    for i in range(n_bands):
        za = h0 + (h1 - h0) * i / n_bands
        zb = h0 + (h1 - h0) * (i + 1) / n_bands
        lathe(red if i % 2 == 0 else white,
              [(0, za), (tower_r(za), za), (tower_r(zb), zb), (0, zb)], SEG_TOWER, M=M)
    objs.append(finalize("LH_Tower_White", white, mats["WALL_WHITE"], smooth=True))
    objs.append(finalize("LH_Tower_Red", red, mats["WALL_RED"], smooth=True))

    # metal: galeria, slupki, barierka, podstawa laterny, szprosy, dach z iglica, drzwi, okna
    mt = bmesh.new()
    # galeria + podstawa laterny jako jeden profil (jedna zamknieta powloka)
    lathe(mt, [(0, 0.765), (0.118, 0.765), (0.20, 0.795), (0.20, 0.81),
               (0.105, 0.81), (0.105, 0.845), (0, 0.845)], SEG_TOWER, M=M)
    rail_z, rail_R, rail_r = 0.875, 0.19, 0.008
    post_top = rail_z - rail_r
    for k in range(8):
        a = 2 * math.pi * k / 8 + math.pi / 8
        box(mt, V(rail_R * math.cos(a), rail_R * math.sin(a), (0.81 + post_top) / 2),
            (0.01, 0.01, post_top - 0.81), R=Matrix.Rotation(a, 4, 'Z'), M=M)
    torus(mt, rail_R, rail_r, SEG_SMALL, 6, M=M @ Matrix.Translation((0, 0, rail_z)))
    for k in range(6):
        a = 2 * math.pi * k / 6
        box(mt, V(0.0905 * math.cos(a), 0.0905 * math.sin(a), 0.91), (0.008, 0.008, 0.124),
            R=Matrix.Rotation(a, 4, 'Z'), M=M)
    zc, rs = 1.128, 0.02
    roof = [(0, 0.975), (0.118, 0.975), (0.118, 0.985), (0.10, 1.005), (0.075, 1.04),
            (0.045, 1.07), (0.02, 1.09), (0.008, 1.098), (0.008, 1.106)]
    roof += [(rs * math.sin(math.radians(t)), zc - rs * math.cos(math.radians(t)))
             for t in (30, 60, 90, 120, 150)]
    roof += [(0.005, 1.152), (0, LH_HEIGHT)]
    lathe(mt, roof, SEG_SMALL, M=M)
    hd = 0.05 + 0.055
    box(mt, V(0, -(tower_r(hd) - 0.006), hd), (0.06, 0.03, 0.11), M=M)          # drzwi (od kamery)
    for ang, h in ((-90, 0.40), (-90, 0.62), (90, 0.50), (0, 0.30), (180, 0.55)):
        a = math.radians(ang)
        rr = tower_r(h) - 0.006
        box(mt, V(rr * math.cos(a), rr * math.sin(a), h), (0.035, 0.025, 0.05),
            R=Matrix.Rotation(a - math.pi / 2, 4, 'Z'), M=M)
    objs.append(finalize("LH_Metal", mt, mats["METAL_DARK"], smooth=True))

    # szyba laterny: zamknieta rura (powietrze w srodku), lampa w srodku
    gl = bmesh.new()
    tube(gl, 0.085, 0.078, 0.848, 0.972, SEG_SMALL, M=M)
    objs.append(finalize("LH_Glass", gl, mats["GLASS"], smooth=True))
    lp = bmesh.new()
    bmesh.ops.create_uvsphere(lp, u_segments=16, v_segments=8, radius=0.04,
                              matrix=M @ Matrix.Translation((0, 0, 0.91)))
    objs.append(finalize("LH_Lamp", lp, mats["LAMP_LIGHT"], smooth=True))
    return objs


def build_snow(mat, flake_bm):
    rnd = random.Random(SEED)
    tree = BVHTree.FromBMesh(flake_bm)
    placed = []
    attempts = 0
    while len(placed) < N_BALLS + N_STARS and attempts < 5000:
        attempts += 1
        p = V(rnd.uniform(-2.3, 2.3), rnd.uniform(-1.0, 1.0), rnd.uniform(-2.1, 2.1))
        kind = "ball" if len(placed) < N_BALLS else "star"
        size = rnd.uniform(0.04, 0.07) if kind == "ball" else rnd.uniform(0.08, 0.12)
        if abs(p.x) < 1.35 and abs(p.z) < 1.35 and abs(p.y) < 0.8:
            continue                                   # nie w rdzeniu
        if p.y < 0 and abs(p.x) < 0.8 and -1.0 < p.z < 1.0:
            continue                                   # nie zaslaniaj latarni
        hit = tree.find_nearest(p)
        if hit[0] is not None and hit[3] < size + 0.08:
            continue
        if any((p - q).length < size + s + 0.15 for q, s, _ in placed):
            continue
        placed.append((p, size, kind))
    bm = bmesh.new()
    pieces = []
    for i, (p, size, kind) in enumerate(placed):
        before = len(bm.faces)
        if kind == "ball":
            bmesh.ops.create_icosphere(bm, subdivisions=1, radius=size, matrix=Matrix.Translation(p))
        else:
            rot = Euler((rnd.uniform(0, 6.28), rnd.uniform(0, 6.28), rnd.uniform(0, 6.28))).to_matrix().to_4x4()
            star6(bm, Matrix.Translation(p) @ rot, size, size * 0.4, 0.012)
        pieces.append(kind)
    ob = finalize("Snow", bm, mat, smooth=True)
    return ob, placed


# =====================================================================
#  KAMERA / PODGLAD
# =====================================================================
def build_camera():
    scene = bpy.context.scene
    cd = bpy.data.cameras.new("Camera")
    cd.sensor_fit = 'VERTICAL'
    cd.angle_y = math.radians(CAM_VFOV)
    cam = bpy.data.objects.new("Camera", cd)
    scene.collection.objects.link(cam)
    cam.location = CAM_LOC
    d = Vector(CAM_TARGET) - Vector(CAM_LOC)
    cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    scene.camera = cam
    scene.render.resolution_x, scene.render.resolution_y = RESOLUTION
    # tylko do podgladu w Blenderze (nie trafia do OBJ)
    w = bpy.data.worlds.get("PreviewSky") or bpy.data.worlds.new("PreviewSky")
    scene.world = w
    try:
        w.use_nodes = True
    except Exception:
        pass
    bg = w.node_tree.nodes.get("Background")
    if bg:
        bg.inputs["Color"].default_value = (0.55, 0.68, 0.85, 1.0)
        bg.inputs["Strength"].default_value = 0.8
    return cam


# =====================================================================
#  EKSPORT
# =====================================================================
def write_mtl(path, names):
    L = ["# MTL dla wlasnego raytracera (RTIOW).",
         "# illum 1 = lambertian, 3 = metal (Pr = fuzz), 7 = dielectric (Ni = IOR), Ke = emisja (kolor * moc)", ""]
    for n in sorted(names):
        p = MATERIALS[n]
        r, g, b = p["color"]
        L.append(f"newmtl {n}")
        if p["kind"] == "dielectric":
            L += [f"# kind=dielectric ior={p['ior']}", f"Kd {r:.4f} {g:.4f} {b:.4f}", "Ks 1.0000 1.0000 1.0000",
                  f"Tf {r:.4f} {g:.4f} {b:.4f}", f"Ni {p['ior']:.4f}", "d 0.0000", "Tr 1.0000",
                  "Pr 0.0000", "Pm 0.0000", "Ke 0.0000 0.0000 0.0000", "illum 7"]
        elif p["kind"] == "metal":
            L += [f"# kind=metal fuzz={p['fuzz']}", f"Kd {r:.4f} {g:.4f} {b:.4f}", f"Ks {r:.4f} {g:.4f} {b:.4f}",
                  "Ni 1.0000", "d 1.0000", f"Pr {p['fuzz']:.4f}", "Pm 1.0000",
                  "Ke 0.0000 0.0000 0.0000", "illum 3"]
        elif p["kind"] == "emissive":
            s = p["strength"]
            L += [f"# kind=diffuse_light strength={s}", f"Kd {r:.4f} {g:.4f} {b:.4f}", "Ks 0.0000 0.0000 0.0000",
                  f"Ke {r*s:.4f} {g*s:.4f} {b*s:.4f}", "Ni 1.0000", "d 1.0000", "Pr 1.0000", "Pm 0.0000", "illum 0"]
        else:
            L += ["# kind=lambertian", f"Kd {r:.4f} {g:.4f} {b:.4f}", "Ks 0.0000 0.0000 0.0000",
                  "Ni 1.0000", "d 1.0000", "Pr 1.0000", "Pm 0.0000", "Ke 0.0000 0.0000 0.0000", "illum 1"]
        L.append("")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(L))


def export_obj(path, objs):
    vl = bpy.context.view_layer
    for o in vl.objects:
        o.select_set(False)
    for o in objs:
        o.select_set(True)
    vl.objects.active = objs[0]
    bpy.ops.wm.obj_export(
        filepath=path, check_existing=False, export_selected_objects=True,
        export_animation=False, forward_axis='NEGATIVE_Z', up_axis='Y', global_scale=1.0,
        apply_modifiers=True, apply_transform=True, export_uv=False, export_normals=True,
        export_colors=False, export_materials=True, export_pbr_extensions=False,
        path_mode='STRIP', export_triangulated_mesh=True, export_curves_as_nurbs=False,
        export_object_groups=False, export_material_groups=False, export_vertex_groups=False,
        export_smooth_groups=False)
    names = {s.material.name for o in objs for s in o.material_slots if s.material}
    write_mtl(os.path.splitext(path)[0] + ".mtl", names)


# =====================================================================
#  MAIN
# =====================================================================
def main(export=True):
    OUT = out_dir()
    clean_scene()
    mats = make_materials()

    flake, outer = build_snowflake(mats["ICE"], 'EXACT')
    if not outer["manifold"]:
        bpy.data.objects.remove(flake, do_unlink=True)
        flake, outer = build_snowflake(mats["ICE"], 'MANIFOLD')
    rock = build_rock(mats["ROCK"])
    lh = build_lighthouse(mats)
    fbm = bm_of(flake)
    snow, placed = build_snow(mats["SNOW"], fbm)
    cam = build_camera()

    meshes = [flake] + lh + [rock, snow]

    # --- Decimate awaryjnie, gdyby cos przekroczylo limit
    def tris(o):
        o.data.calc_loop_triangles()
        return len(o.data.loop_triangles)
    if tris(flake) > LIMITS["snowflake"]:
        d = flake.modifiers.new("Decimate", 'DECIMATE')
        d.ratio = LIMITS["snowflake"] / tris(flake) * 0.95
        dg = bpy.context.evaluated_depsgraph_get()
        me = bpy.data.meshes.new_from_object(flake.evaluated_get(dg))
        flake.modifiers.clear()
        flake.data = me

    # --- kontrola transformacji (wszystko zbudowane w ukladzie swiata)
    for o in meshes:
        if o.matrix_world != Matrix.Identity(4):
            bpy.context.view_layer.objects.active = o
            o.select_set(True)
            bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    # --- statystyki i testy
    rows = []
    for o in meshes:
        bm = bm_of(o)
        chk = mesh_check(bm)
        si = self_intersections(bm) if o.name in ("Snowflake", "LH_Glass", "LH_Lamp") else None
        rows.append(dict(object=o.name, material=o.material_slots[0].material.name,
                         triangles=len(bm.faces), **chk, degenerate_tris=degenerate_tris(bm),
                         self_intersections=si))
        bm.free()
    total = sum(r["triangles"] for r in rows)
    lh_total = sum(r["triangles"] for r in rows if r["object"].startswith("LH_"))

    # odstepy: latarnia+skala wewnatrz wneki, wneka wewnatrz rdzenia
    cav = build_cavity_bm()
    core = build_core_bm()
    pts = []
    for o in lh + [rock]:
        pts += [o.matrix_world @ v.co for v in o.data.vertices]
    gap_lh_cavity = convex_margin(pts, cav)
    gap_cavity_core = convex_margin([v.co for v in cav.verts], core)
    ftree = BVHTree.FromBMesh(fbm)
    gap_lh_flake = min(ftree.find_nearest(p)[3] for p in pts)
    sbm = bm_of(snow)
    gap_snow_flake = min(ftree.find_nearest(v.co)[3] for v in sbm.verts)
    gbm, lbm, mbm = bm_of(bpy.data.objects["LH_Glass"]), bm_of(bpy.data.objects["LH_Lamp"]), bm_of(bpy.data.objects["LH_Metal"])
    gtree = BVHTree.FromBMesh(gbm)
    glass_lamp_overlap = len(gtree.overlap(BVHTree.FromBMesh(lbm)))
    glass_metal_overlap = len(gtree.overlap(BVHTree.FromBMesh(mbm)))
    flake_lh_overlap = sum(len(ftree.overlap(BVHTree.FromBMesh(b))) for b in (gbm, lbm, mbm))
    cav_vol = abs(cav.calc_volume(signed=True))
    for b in (cav, core, sbm, gbm, lbm, mbm, fbm):
        b.free()

    bb = [flake.matrix_world @ Vector(c) for c in flake.bound_box]
    flake_bbox = [[round(min(v[i] for v in bb), 3) for i in range(3)],
                  [round(max(v[i] for v in bb), 3) for i in range(3)]]
    lh_z = [min(p.z for p in pts), max(p.z for p in pts)]
    lh_only = []
    for o in lh:
        lh_only += [o.matrix_world @ v.co for v in o.data.vertices]

    stats = dict(
        objects=rows, total_triangles=total, lighthouse_triangles=lh_total,
        limits=LIMITS,
        within_limits=dict(
            total=total <= LIMITS["total"],
            snowflake=rows[0]["triangles"] <= LIMITS["snowflake"],
            lighthouse=lh_total <= LIMITS["lighthouse"],
            rock=[r for r in rows if r["object"] == "Rock"][0]["triangles"] <= LIMITS["rock"],
            snow_piece=True),
        snow_pieces=len(placed), snow_piece_triangles={"ball": 80, "star": 44},
        gaps=dict(lighthouse_rock_to_cavity_min=round(gap_lh_cavity, 4),
                  lighthouse_rock_to_ice_surface_min=round(gap_lh_flake, 4),
                  cavity_to_core_outer_min=round(gap_cavity_core, 4),
                  snow_to_ice_min=round(gap_snow_flake, 4),
                  required=GAP_MIN),
        overlaps=dict(glass_vs_lamp=glass_lamp_overlap, glass_vs_metal=glass_metal_overlap,
                      ice_vs_lighthouse=flake_lh_overlap),
        cavity_volume=round(cav_vol, 4),
        snowflake_bbox=flake_bbox,
        lighthouse_height=round(max(p.z for p in lh_only) - min(p.z for p in lh_only), 4),
        lighthouse_rock_z=[round(lh_z[0], 3), round(lh_z[1], 3)],
        camera=dict(blender=dict(position=list(CAM_LOC), look_at=list(CAM_TARGET), up=[0, 0, 1]),
                    obj_y_up=dict(position=[CAM_LOC[0], CAM_LOC[2], -CAM_LOC[1]],
                                  look_at=[CAM_TARGET[0], CAM_TARGET[2], -CAM_TARGET[1]], up=[0, 1, 0]),
                    vfov_deg=CAM_VFOV, aspect=f"{RESOLUTION[0]}:{RESOLUTION[1]}"),
        out_dir=OUT,
    )

    # --- wydruk
    print("\n%-16s %-11s %8s  %s" % ("obiekt", "material", "trojk.", "manifold"))
    for r in rows:
        print("%-16s %-11s %8d  %s" % (r["object"], r["material"], r["triangles"], r["manifold"]))
    print("RAZEM: %d (latarnia %d)" % (total, lh_total))

    if export:
        bpy.context.view_layer.update()
        export_obj(os.path.join(OUT, "scene.obj"), meshes)
        export_obj(os.path.join(OUT, "snowflake.obj"), [flake])
        export_obj(os.path.join(OUT, "lighthouse.obj"), lh)
        export_obj(os.path.join(OUT, "rock.obj"), [rock])
        export_obj(os.path.join(OUT, "snow.obj"), [snow])
        with open(os.path.join(OUT, "scene_stats.json"), "w", encoding="utf-8") as f:
            json.dump(stats, f, indent=2, ensure_ascii=False)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "scene.blend"))
    return stats


if __name__ == "__main__":
    STATS = main(export=True)
