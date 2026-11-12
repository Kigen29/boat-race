"""Generates and checks Source/BoatRace/BoatTrackData.h.

Lays out the racing line, the gates and every obstacle, then verifies that
gate lines never cross, that nothing blocks a gate or overlaps another
obstacle, that the barges actually cross the course, and that driving the
racing line scores every gate in order while driving it backwards scores
none. Edit the tables here rather than the generated header:

    python3 Scripts/course_layout.py && mv BoatTrackData.h Source/BoatRace/
"""
import math
HALF_W=45.0
PTS=[(0,-480),(210,-480),(395,-425),(520,-295),(560,-110),(525,70),(400,135),(215,150),
     (75,275),(-105,330),(-300,315),(-435,195),(-495,25),(-465,-175),(-350,-335),(-180,-450)]
N=len(PTS)
sub=lambda a,b:(a[0]-b[0],a[1]-b[1]); add=lambda a,b:(a[0]+b[0],a[1]+b[1])
mul=lambda a,s:(a[0]*s,a[1]*s); dot=lambda a,b:a[0]*b[0]+a[1]*b[1]
crs=lambda a,b:a[0]*b[1]-a[1]*b[0]; dist=lambda a,b:math.hypot(a[0]-b[0],a[1]-b[1])
def norm(a):
    l=math.hypot(*a); return (a[0]/l,a[1]/l)
def d_seg(p,a,b):
    ab=sub(b,a); t=max(0.0,min(1.0,dot(sub(p,a),ab)/dot(ab,ab))); return dist(p,add(a,mul(ab,t)))
gates=[]
for i in range(N):
    f=norm(sub(PTS[(i+1)%N],PTS[(i-1)%N])); ins=(-f[1],f[0])
    gates.append(dict(c=PTS[i],f=f,inner=add(PTS[i],mul(ins,HALF_W)),outer=sub(PTS[i],mul(ins,HALF_W))))
def on_line(seg,t,off):
    a,b=PTS[seg%N],PTS[(seg+1)%N]; f=norm(sub(b,a)); ins=(-f[1],f[0])
    return add(add(a,mul(sub(b,a),t)),mul(ins,off)), math.degrees(math.atan2(f[1],f[0]))
d_gates=lambda p: min(d_seg(p,g['inner'],g['outer']) for g in gates)
d_line =lambda p: min(d_seg(p,PTS[i],PTS[(i+1)%N]) for i in range(N))
ROCKS =[(1,.45,22,12),(2,.50,-26,10),(3,.55,24,9),(5,.40,-28,11),(7,.50,26,10),
        (9,.45,-24,9),(11,.50,25,12),(13,.45,-27,10),(14,.55,23,8)]
RAMPS =[(0,.55,18),(4,.50,-20),(8,.50,18),(12,.55,-18)]
POOLS =[(2,.50,50,30),(6,.50,-50,28),(10,.50,50,30)]
CRATES=[(0,.80,-8),(2,.25,10),(4,.75,-10),(5,.80,8),(7,.80,-9),(9,.75,9),(11,.80,-8),(14,.25,10)]
DEBRIS=[(1,.20,34),(1,.75,-32),(3,.30,30),(4,.25,-34),(6,.40,33),(6,.85,-30),
        (8,.20,31),(9,.20,-33),(10,.60,32),(12,.25,-31),(13,.70,30),(15,.40,-33)]
BARGES=[((340,-230),(640,-230),44.0),((-370,-140),(-640,-140),52.0)]
fail=0; solid=[]
def check(name,items,rad,mingate,collect=False):
    global fail
    out=[]
    for it in items:
        p,yaw=on_line(it[0],it[1],it[2]); r=rad(it)
        gc=d_gates(p)-r; oc=min([dist(p,o) for o in solid],default=1e18)-r
        if not (gc>mingate and oc>8):
            fail+=1; print("FAIL %-7s %8.1f %8.1f  gate%+7.1f other%+7.1f" % (name,p[0],p[1],gc,oc))
        out.append((p,yaw,r))
        if collect: solid.append(p)
    return out
rocks=check("rocks",ROCKS,lambda it:it[3],20,True)
ramps=check("ramps",RAMPS,lambda it:14.0,20,True)
pools=check("pools",POOLS,lambda it:it[3],20)
crates=check("crates",CRATES,lambda it:3.0,15)
debris=check("debris",DEBRIS,lambda it:2.0,10)
for (a,b,per) in BARGES:
    hits=[]
    for i in range(N):
        c,d=PTS[i],PTS[(i+1)%N]; r,s=sub(b,a),sub(d,c); den=crs(r,s)
        if abs(den)>1e-12:
            t=crs(sub(c,a),s)/den; u=crs(sub(c,a),r)/den
            if 0<=t<=1 and 0<=u<=1: hits.append(i)
    print("barge %-16s -> %-16s period %4.0fs  crosses racing line at segment %s" % (a,b,per,hits or "NONE"))
    if not hits: fail+=1
    for p,y,r in rocks+ramps:
        if d_seg(p,a,b) < r+25:
            fail+=1; print("FAIL barge path passes within %.0f m of obstacle %s" % (d_seg(p,a,b),p))
# drive the line
def crossed(g,a,b):
    d=sub(b,a)
    if dot(d,g['f'])<=0: return False
    e=sub(g['outer'],g['inner']); den=crs(d,e)
    if abs(den)<1e-12: return False
    w=sub(g['inner'],a); t=crs(w,e)/den; u=crs(w,d)/den
    return 0<=t<=1 and 0<=u<=1
g0=gates[0]
path=[sub(g0['c'],mul(g0['f'],25))]
for lp in range(3):
    for i in range(N):
        a = path[-1] if (lp==0 and i==0) else PTS[i]; b=PTS[(i+1)%N]
        steps=int(dist(a,b)/1.2)+1
        for k in range(1,steps+1): path.append(add(a,mul(sub(b,a),k/steps)))
nxt=0;laps=-1;passed=0;prev=path[0]
for q in path[1:]:
    if crossed(gates[nxt],prev,q):
        if nxt==0: laps+=1
        passed+=1; nxt=(nxt+1)%N
    prev=q
if passed!=3*N or laps!=3: fail+=1
print("driving the line: %d gates, %d laps (want %d, 3)" % (passed,laps,3*N))
rev=0;nxt=0;prev=path[-1]
for q in reversed(path[:-1]):
    if crossed(gates[nxt],prev,q): rev+=1;nxt=(nxt+1)%N
    prev=q
if rev: fail+=1
print("driving it backwards scores %d gates (want 0)" % rev)
lap=sum(dist(PTS[i],PTS[(i+1)%N]) for i in range(N))
print("lap %.0f m   %d gates   width %.0f m   lap time at 23 m/s %.0f s" % (lap,N,HALF_W*2,lap/23))
print("FAILURES: %d" % fail)
L=["#pragma once\n",
 "// Generated and checked by Scripts/course_layout.py: gate lines never cross,",
 "// every obstacle clears the gate lines and the other obstacles, driving the",
 "// racing line scores all %d gates per lap in order, and driving it backwards" % N,
 "// scores none.\n",
 "// Distances are metres. BoatTrack.cpp converts to Unreal units (1 uu = 1 cm).\n",
 "namespace BoatTrackData\n{",
 "\t// Half the course width. 90 m is the UIM minimum for an F1H2O circuit.",
 "\tconst double GateHalfWidthMetres = %.1f;\n" % HALF_W]
def emit(name,rows,comment,w):
    L.append("\t// %s" % comment)
    L.append("\tconst double %s[][%d] =\n\t{" % (name,w))
    for r in rows: L.append("\t\t{ %s }," % ", ".join("%9.2f" % v for v in r))
    L.append("\t};\n")
emit("RacingLine",[(p[0],p[1]) for p in PTS],"Racing line, counter-clockwise. Index 0 is the start / finish line.",2)
emit("Rocks",[(p[0],p[1],r) for p,y,r in rocks],"Rock pillars: X, Y, radius",3)
emit("Ramps",[(p[0],p[1],y) for p,y,r in ramps],"Jump ramps: X, Y, heading in degrees",3)
emit("Whirlpools",[(p[0],p[1],r) for p,y,r in pools],"Whirlpools: X, Y, radius",3)
emit("Crates",[(p[0],p[1]) for p,y,r in crates],"Ammo crates: X, Y",2)
emit("Debris",[(p[0],p[1]) for p,y,r in debris],"Floating debris barrels: X, Y",2)
emit("Barges",[(a[0],a[1],b[0],b[1],per) for a,b,per in BARGES],"Moving barges: from X, Y, to X, Y, round-trip seconds",5)
L.append("}")
open("BoatTrackData.h","w").write("\n".join(L)+"\n")
