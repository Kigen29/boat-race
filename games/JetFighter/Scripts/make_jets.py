"""Original JetFighter art prototypes. Blender: +X nose, metres, origin at CoM.

    blender -b --factory-startup -P Scripts/make_jets.py -- Art/Models Saved/JetArt

The Python geometry is the editable source. GLBs are committed, Unreal assets
are regenerated. These are silhouette/material prototypes, not final UV-baked art.
"""
import bpy, bmesh, math, json, os, sys
from mathutils import Vector
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
out=os.path.abspath(args[0] if args else 'Art/Models')
preview=os.path.abspath(args[1]) if len(args)>1 else None
os.makedirs(out,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)

def material(name):
    m=bpy.data.materials.get(name) or bpy.data.materials.new(name)
    h=name.split('_')[-1];rgb=tuple(int(h[i:i+2],16)/255 for i in (0,2,4))
    m.diffuse_color=(*rgb,1);m.use_nodes=True
    bs=m.node_tree.nodes.get('Principled BSDF');bs.inputs['Base Color'].default_value=(*rgb,1)
    bs.inputs['Metallic'].default_value=0.65 if name.startswith(('Paint','Metal')) else 0.12
    bs.inputs['Roughness'].default_value=0.13 if name.startswith('Glass') else 0.3
    if name.startswith('Lamp'):
        bs.inputs['Emission Color'].default_value=(*rgb,1);bs.inputs['Emission Strength'].default_value=2.5
    return m

class Mesh:
    def __init__(self,name): self.name=name;self.v=[];self.f=[];self.mi=[];self.mats=[];self.smooth=[]
    def face(self,ids,mat,smooth=False):
        if mat not in self.mats:self.mats.append(mat)
        self.f.append(tuple(ids));self.mi.append(self.mats.index(mat));self.smooth.append(smooth)
    def prism(self,points,axis,depth,mat):
        first=len(self.v);n=len(points);d=Vector(axis)*depth*.5
        self.v.extend([tuple(Vector(p)-d) for p in points]+[tuple(Vector(p)+d) for p in points])
        self.face(range(first+n-1,first-1,-1),mat);self.face(range(first+n,first+2*n),mat)
        for i in range(n):j=(i+1)%n;self.face((first+i,first+j,first+n+j,first+n+i),mat)
    def loft(self,rings,mat,segs=32,y=0,z=0):
        start=len(self.v)
        for x,w,h,cz in rings:
            for i in range(segs):
                a=i*math.tau/segs;self.v.append((x,y+math.cos(a)*w,z+cz+math.sin(a)*h))
        for r in range(len(rings)-1):
            for i in range(segs):
                j=(i+1)%segs;a=start+r*segs;self.face((a+i,a+j,a+segs+j,a+segs+i),mat,True)
        self.face(range(start+segs-1,start-1,-1),mat)
        last=start+(len(rings)-1)*segs;self.face(range(last,last+segs),mat)
    def ellipsoid(self,centre,scale,mat,segs=28,rings=14):
        data=[]
        for j in range(rings+1):
            a=math.pi*(0.001+0.998*j/rings)
            for i in range(segs):
                b=math.tau*i/segs;data.append((centre[0]+scale[0]*math.cos(a),centre[1]+scale[1]*math.sin(a)*math.cos(b),centre[2]+scale[2]*math.sin(a)*math.sin(b)))
        s=len(self.v);self.v.extend(data)
        for j in range(rings):
            for i in range(segs):n=(i+1)%segs;a=s+j*segs;self.face((a+i,a+n,a+segs+n,a+segs+i),mat,True)
        self.face(range(s+segs-1,s-1,-1),mat);last=s+rings*segs;self.face(range(last,last+segs),mat)
    def export(self):
        data=bpy.data.meshes.new(self.name);data.from_pydata(self.v,[],self.f);data.update()
        for m in self.mats:data.materials.append(material(m))
        for p,i,smooth in zip(data.polygons,self.mi,self.smooth):p.material_index=i;p.use_smooth=smooth
        bm=bmesh.new();bm.from_mesh(data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
        obj=bpy.data.objects.new(self.name,data);bpy.context.collection.objects.link(obj)
        bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
        # A usable UV layout for material prototypes; production atlases/bakes
        # and panel-line textures are explicit tasks in the art plan.
        bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(island_margin=0.015);bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.export_scene.gltf(filepath=os.path.join(out,self.name+'.glb'),export_format='GLB',use_selection=True,export_apply=True)
        tris=sum(len(p.vertices)-2 for p in data.polygons)
        print('JET MODEL',self.name,'triangles',tris,'materials',len(self.mats),'bounds',tuple(round(x,2) for x in obj.dimensions))
        return obj,{'model':self.name,'triangles':tris,'material_slots':len(self.mats),'dimensions_metres':[round(x,3) for x in obj.dimensions]}

SPECS=[
 dict(name='JetKite',callsign='AF-01 KITE',length=13.4,span=9.8,body=.72,paint='Paint_476776',trim='Trim_F0A34A',engines=1,canard=True,wide=False),
 dict(name='JetRook',callsign='AF-02 ROOK',length=17.2,span=11.8,body=.88,paint='Paint_687B91',trim='Trim_C3D1DB',engines=2,canard=False,wide=False),
 dict(name='JetManta',callsign='AF-03 MANTA',length=18.0,span=14.0,body=1.1,paint='Paint_66725F',trim='Trim_D3A962',engines=2,canard=False,wide=True),
]
manifest=[];objects=[]
for spec in SPECS:
    m=Mesh(spec['name']);L=spec['length'];S=spec['span'];W=spec['body'];P=spec['paint'];T=spec['trim']
    body=[(.50,.015,.02,.0),(.46,.12,.14,.02),(.35,.48,.42,.08),(.20,.88,.70,.02),(.02,1.,.72,0),(-.18,.96,.64,-.02),(-.36,.70,.48,-.05),(-.48,.42,.38,-.04)]
    m.loft([(x*L,w*W,h,cz) for x,w,h,cz in body],P)
    for side in (-1,1):
        if spec['wide']:
            points=[(.26*L,.65*W),(.01*L,.49*S),(-.14*L,.5*S),(-.32*L,.34*S),(-.37*L,.8*W)]
        elif spec['canard']:
            points=[(.18*L,.6*W),(-.14*L,.5*S),(-.30*L,.43*S),(-.29*L,.7*W)]
        else:
            points=[(.10*L,.65*W),(-.12*L,.5*S),(-.20*L,.48*S),(-.26*L,.8*W)]
        m.prism([(x,side*y,-.08) for x,y in points],(0,0,1),.12,P)
        # Contrasting, geometric wing-tip identification band.
        a,b=points[1],points[2]
        strip=[(a[0]+.06,side*a[1],.0),(b[0]-.06,side*b[1],.0),(b[0]+.1,side*(b[1]-.23),.0),(a[0]+.20,side*(a[1]-.23),.0)]
        m.prism(strip,(0,0,1),.02,T)
        if spec['canard']:
            m.prism([(.30*L,side*.55,.25),(.21*L,side*2.,.25),(.14*L,side*1.9,.25),(.18*L,side*.55,.25)],(0,0,1),.09,T)
        else:
            m.prism([(-.31*L,side*.6,.12),(-.40*L,side*S*.30,.12),(-.48*L,side*S*.29,.12),(-.47*L,side*.6,.12)],(0,0,1),.09,P)
        # Intake opening and lip are modelled geometry, not black UI decals.
        m.loft([(.11*L,.30,.31,-.23),(.03*L,.35,.35,-.23),(-.12*L,.30,.3,-.23)],'Metal_303E49',segs=20,y=side*W*.94)
        m.loft([(.115*L,.25,.25,-.23),(.111*L,.25,.25,-.23)],'Rubber_101821',segs=20,y=side*W*.94)
        m.ellipsoid((-.16*L,side*(S*.5-.1),.0),(.16,.08,.07),'Lamp_F34A39' if side<0 else 'Lamp_50EAB0',segs=12,rings=6)
        # Small under-wing training stores, with actual tapered noses/fins.
        y=side*S*.27
        m.loft([(.02*L,.02,.02,-.4),(-.01*L,.12,.12,-.4),(-.15*L,.12,.12,-.4),(-.17*L,.07,.07,-.4)],'Metal_BCC7C9',segs=16,y=y)
        m.prism([(-.11*L,y-.27,-.4),(-.17*L,y-.27,-.4),(-.17*L,y+.27,-.4),(-.11*L,y+.27,-.4)],(0,0,1),.025,'Metal_53616A')
    # Canopy frame and opaque reflective glass keep the prototype cheap on iGPU.
    m.ellipsoid((.21*L,0,.72),(L*.115,.49,.45),'Rubber_15212A')
    m.ellipsoid((.21*L+.025,0,.765),(L*.107,.445,.425),'Glass_204D64')
    fins=[0] if spec['engines']==1 else [-1,1]
    for side in fins:
        y=side*(W*.95);tipy=y+side*.45
        m.prism([(-.26*L,y,.40),(-.35*L,tipy,2.35),(-.46*L,tipy,2.05),(-.48*L,y,.32)],(0,1,0),.12,P)
        m.prism([(-.35*L,tipy,2.34),(-.46*L,tipy,2.04),(-.45*L,tipy,1.82),(-.34*L,tipy,2.11)],(0,1,0),.14,T)
    for side in ([0] if spec['engines']==1 else [-1,1]):
        y=side*W*.68;r=.42 if side==0 else .39
        m.loft([(-.13*L,r*1.1,r*.95,-.12),(-.35*L,r,r,-.12),(-.48*L,r*.9,r*.9,-.12)],P,segs=24,y=y)
        m.loft([(-.44*L,r,r,-.12),(-.49*L,r*1.03,r*1.03,-.12),(-.50*L,r*.87,r*.87,-.12)],'Metal_6B737D',segs=32,y=y)
        m.loft([(-.501*L,r*.75,r*.75,-.12),(-.502*L,r*.75,r*.75,-.12)],'Rubber_101821',segs=24,y=y)
        m.loft([(-.503*L,r*.49,r*.49,-.12),(-.504*L,r*.49,r*.49,-.12)],'Lamp_FF9D36',segs=24,y=y)
    obj,info=m.export();info.update(callsign=spec['callsign'],author='Original JetFighter procedural art',stage='silhouette-material prototype')
    manifest.append(info);objects.append(obj)
with open(os.path.join(out,'airframes.json'),'w') as f:json.dump(manifest,f,indent=2);f.write('\n')
if preview:
    os.makedirs(preview,exist_ok=True)
    for obj,y in zip(objects,(-19,0,19)):obj.location.y=y
    scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=16;scene.cycles.use_denoising=True
    scene.world.color=(.12,.14,.18)
    for loc,power,size in (((8,-20,30),3800,25),((-20,25,20),2600,22)):
        data=bpy.data.lights.new('Softbox','AREA');data.energy=power;data.shape='DISK';data.size=size
        light=bpy.data.objects.new('Softbox',data);scene.collection.objects.link(light);light.location=loc;light.rotation_euler=(Vector((0,0,0))-light.location).to_track_quat('-Z','Y').to_euler()
    data=bpy.data.cameras.new('Camera');cam=bpy.data.objects.new('Camera',data);scene.collection.objects.link(cam)
    cam.location=(-33,-42,28);cam.rotation_euler=(Vector((0,0,0))-cam.location).to_track_quat('-Z','Y').to_euler();data.type='ORTHO';data.ortho_scale=62;scene.camera=cam
    for obj,spec in zip(objects,SPECS):
        data=bpy.data.curves.new(spec['callsign'],'FONT');data.body=spec['callsign'];data.align_x='CENTER';data.size=1.25
        text=bpy.data.objects.new(spec['callsign'],data);scene.collection.objects.link(text);text.location=obj.location+Vector((0,0,-2.5));text.rotation_euler=(cam.location-text.location).to_track_quat('Z','Y').to_euler();data.materials.append(material('Lamp_BFD4DD'))
    scene.render.resolution_x=1600;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG';scene.render.film_transparent=False;scene.render.filepath=os.path.join(preview,'JetFighter_Airframe_Lineup.png')
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(preview,'Airframes.blend'));bpy.ops.render.render(write_still=True)
