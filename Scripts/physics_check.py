"""Numerical check of the boat physics in Source/BoatRace/BoatPawn.cpp.

Mirrors the forces in that file as a rigid body and reports whether the boat
floats, settles, reaches its top speed, turns without capsizing and behaves the
same at any frame rate. Run it after changing any handling value in
Config/DefaultGame.ini:  python3 Scripts/physics_check.py
"""

import math, os

G=980.0; MASS=1200.0; L,W,H=700.0,240.0,130.0; DRAFT=60.0
BUOY_MULT=2.6; ZETA=0.65; THRUST_ACC=1300.0; TOP=2900.0
LAT_RATIO=float(os.environ.get('LAT','7.0')); TURN_RATE=float(os.environ.get('TURN','76.0'))
TURN_RESP=float(os.environ.get('RESP','4.0')); SLOPE_F=1.0; ANG_DAMP=1.1
LIFT_FRAC=float(os.environ.get('LIFT','0.35')); PROP_DEPTH=40.0
HEEL=float(os.environ.get('HEEL','6.0')); MAX_HEEL=float(os.environ.get('MAXHEEL','14.0'))
CONTACT_GAIN=2.5; SELF_RIGHT=70.0

WEIGHT=MASS*G; TOTAL=BUOY_MULT*WEIGHT; PER=TOTAL/4
OMEGA_N=math.sqrt((TOTAL/DRAFT)/MASS); DAMP=(2*ZETA*MASS*OMEGA_N)/4
DRAG_F=(MASS*THRUST_ACC)/(TOP*TOP); LIFT_C=LIFT_FRAC*WEIGHT/(TOP*TOP)
Ix=MASS*(W*W+H*H)/12; Iy=MASS*(L*L+H*H)/12; Iz=MASS*(L*L+W*W)/12
IB=(Ix,Iy,Iz)
PROBES=[(L*0.42,W*0.38,-H*0.5),(L*0.42,-W*0.38,-H*0.5),(-L*0.42,W*0.38,-H*0.5),(-L*0.42,-W*0.38,-H*0.5)]
COM=(-L*0.06,0.0,-H*0.28); PROP=(-L*0.46,0.0,-H*0.5-PROP_DEPTH)
THRUST_PT=(-L*0.46,0.0,-H*0.28); LIFT_PT=(L*0.15,0.0,-H*0.28)

WAVES=[(55.0,9000.0,15.0),(32.0,5200.0,-35.0),(20.0,3100.0,62.0),(12.0,1700.0,-78.0),(7.0,900.0,124.0),(4.0,520.0,-151.0)]
WP=[]
for i,(a,wl,hd) in enumerate(WAVES):
    k=2*math.pi/wl
    WP.append((a,k,math.sqrt(G*k),math.cos(math.radians(hd)),math.sin(math.radians(hd)),i*1.7+0.37))

def wave(x,y,t,r):
    if r<=0: return 0.0,0.0,0.0,0.0
    h=sx=sy=vz=0.0
    for a,k,w,dx,dy,ph in WP:
        ang=(x*dx+y*dy)*k-w*t+ph
        h+=a*math.sin(ang); g=a*k*math.cos(ang); sx+=g*dx; sy+=g*dy; vz+=-a*w*math.cos(ang)
    return h*r,sx*r,sy*r,vz*r

def mul(A,B): return tuple(tuple(sum(A[i][k]*B[k][j] for k in range(3)) for j in range(3)) for i in range(3))
def mv(A,v): return tuple(sum(A[i][k]*v[k] for k in range(3)) for i in range(3))
def T(A): return tuple(tuple(A[j][i] for j in range(3)) for i in range(3))
def cross(a,b): return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def ortho(R):
    c0=[R[0][0],R[1][0],R[2][0]]; c1=[R[0][1],R[1][1],R[2][1]]
    n=math.sqrt(sum(v*v for v in c0)) or 1.0; c0=[v/n for v in c0]
    d=sum(c0[i]*c1[i] for i in range(3)); c1=[c1[i]-d*c0[i] for i in range(3)]
    n=math.sqrt(sum(v*v for v in c1)) or 1.0; c1=[v/n for v in c1]
    c2=cross(c0,c1)
    return ((c0[0],c1[0],c2[0]),(c0[1],c1[1],c2[1]),(c0[2],c1[2],c2[2]))

def sim(dt,secs,throttle,steer,rough,report_from=0.0):
    pos=[0.0,0.0,45.0]; vel=[0.0,0.0,0.0]; om=[0.0,0.0,0.0]
    R=((1,0,0),(0,1,0),(0,0,1))
    o=dict(minz=1e9,maxz=-1e9,mp=0,mr=0,sunk=False,settle=None)
    for n in range(int(secs/dt)):
        t=n*dt
        fwd=(R[0][0],R[1][0],R[2][0]); up=(R[0][2],R[1][2],R[2][2])
        F=[0.0,0.0,-WEIGHT]; Tq=[0.0,0.0,0.0]
        com=[pos[i]+mv(R,COM)[i] for i in range(3)]
        ssum=0.0
        for p in PROBES:
            pw=[pos[i]+mv(R,p)[i] for i in range(3)]
            h,sx,sy,wv=wave(pw[0],pw[1],t,rough)
            sub=max(0.0,min(1.0,(h-pw[2])/DRAFT)); ssum+=sub
            if sub<=0: continue
            r=[pw[i]-com[i] for i in range(3)]
            pv=[vel[i]+cross(om,r)[i] for i in range(3)]
            f=[-sx*MASS*G*0.25*SLOPE_F*sub, -sy*MASS*G*0.25*SLOPE_F*sub,
               PER*sub-(pv[2]-wv)*DAMP*sub]
            for i in range(3): F[i]+=f[i]
            tq=cross(r,f)
            for i in range(3): Tq[i]+=tq[i]
        subm=ssum/4; contact=max(0.0,min(1.0,subm*BUOY_MULT*CONTACT_GAIN))
        fh=math.hypot(fwd[0],fwd[1]) or 1.0
        fH=(fwd[0]/fh,fwd[1]/fh,0.0); rH=(-fH[1],fH[0],0.0)
        along=vel[0]*fH[0]+vel[1]*fH[1]; across=vel[0]*rH[0]+vel[1]*rH[1]
        if contact>0:
            da=-math.copysign(1,along)*DRAG_F*along*along if along else 0.0
            dc=-math.copysign(1,across)*DRAG_F*LAT_RATIO*across*across if across else 0.0
            for i in range(3): F[i]+=(fH[i]*da+rH[i]*dc)*contact
        pp=[pos[i]+mv(R,PROP)[i] for i in range(3)]
        hp,_,_,_=wave(pp[0],pp[1],t,rough)
        ssub=max(0.0,min(1.0,(hp-pp[2])/DRAFT))
        if throttle and ssub>0:
            tw=[pos[i]+mv(R,THRUST_PT)[i] for i in range(3)]
            th=[fwd[i]*(throttle*THRUST_ACC*MASS*ssub) for i in range(3)]
            r=[tw[i]-com[i] for i in range(3)]
            for i in range(3): F[i]+=th[i]
            tq=cross(r,th)
            for i in range(3): Tq[i]+=tq[i]
        if contact>0 and along>0:
            lift=min(LIFT_C*along*along*contact,0.8*WEIGHT)
            lw=[pos[i]+mv(R,LIFT_PT)[i] for i in range(3)]
            lf=[0.0,0.0,lift]; r=[lw[i]-com[i] for i in range(3)]
            for i in range(3): F[i]+=lf[i]
            tq=cross(r,lf)
            for i in range(3): Tq[i]+=tq[i]
        flow=max(-1.0,min(1.0,abs(along)/600.0))*(1 if along>=0 else -1)
        sf=min(1.0,abs(along)/TOP)
        want=steer*TURN_RATE*flow*(1-0.35*sf)*ssub
        Tq[2]+=math.radians(want-math.degrees(om[2]))*Iz*TURN_RESP
        roll_now=math.atan2(R[2][1],R[2][2])
        roll_rate=sum(om[i]*fH[i] for i in range(3))
        heel=((math.radians(-steer*MAX_HEEL*sf)-roll_now)*HEEL-roll_rate*2.0)*Ix*contact
        heel=max(-Ix*12.0,min(Ix*12.0,heel))
        for i in range(3): Tq[i]+=fH[i]*heel
        tilt=math.degrees(math.acos(max(-1.0,min(1.0,up[2]))))
        if tilt>SELF_RIGHT:
            ax=cross(up,(0,0,1)); nn=math.sqrt(sum(a*a for a in ax)) or 1.0
            st=min(1.0,(tilt-SELF_RIGHT)/110.0)
            for i in range(3): Tq[i]+=ax[i]/nn*(Iz*2.2*st)
        # rigid-body angular acceleration with the inertia tensor in world space
        tb=mv(T(R),Tq)
        ab=[tb[i]/IB[i] for i in range(3)]
        alpha=mv(R,ab)
        for i in range(3):
            vel[i]+=F[i]/MASS*dt; pos[i]+=vel[i]*dt
            om[i]+=alpha[i]*dt-om[i]*ANG_DAMP*dt
        Wm=((0,-om[2],om[1]),(om[2],0,-om[0]),(-om[1],om[0],0))
        dR=mul(Wm,R)
        R=ortho(tuple(tuple(R[i][j]+dR[i][j]*dt for j in range(3)) for i in range(3)))
        if t>=report_from:
            o['minz']=min(o['minz'],pos[2]); o['maxz']=max(o['maxz'],pos[2])
            o['mp']=max(o['mp'],abs(math.degrees(math.asin(max(-1,min(1,-R[2][0]))))))
            o['mr']=max(o['mr'],abs(math.degrees(math.atan2(R[2][1],R[2][2]))))
        if pos[2]<-400: o['sunk']=True; break
        if o['settle'] is None and t>0.5 and abs(vel[2])<2.0 and rough==0: o['settle']=t
    o['speed']=math.hypot(vel[0],vel[1]); o['yaw']=math.degrees(om[2]); o['z']=pos[2]
    return o

fail=0
print("constants: buoyancy/probe %.0f  damping/probe %.0f  heave %.1f rad/s  drag %.4f  rest draft %.0f cm"
      % (PER,DAMP,OMEGA_N,DRAG_F,DRAFT/BUOY_MULT))
r=sim(1/60,12,0,0,0.0); print("\n1 rest, calm      z %.0f cm after %.1fs  sunk=%s"%(r['z'],r['settle'] or -1,r['sunk']))
if r['sunk'] or not 20<r['z']<70: fail+=1; print("  FAIL")
r=sim(1/60,60,0,0,1.0,20); print("2 drifting, sea   z %.0f..%.0f  pitch %.0f  roll %.0f  sunk=%s"%(r['minz'],r['maxz'],r['mp'],r['mr'],r['sunk']))
if r['sunk'] or r['mr']>60: fail+=1; print("  FAIL")
r=sim(1/60,60,1.0,0,0.0,40); print("3 flat out, calm  %.0f cm/s = %.0f km/h  pitch %.0f"%(r['speed'],r['speed']*0.036,r['mp']))
if abs(r['speed']-TOP)>300: fail+=1; print("  FAIL")
r=sim(1/60,60,1.0,0,1.0,40); print("4 flat out, sea   %.0f cm/s = %.0f km/h  pitch %.0f  roll %.0f  sunk=%s"%(r['speed'],r['speed']*0.036,r['mp'],r['mr'],r['sunk']))
if r['sunk']: fail+=1; print("  FAIL")
for st,lab in ((0.35,"a third"),(0.7,"most"),(1.0,"full")):
    r=sim(1/60,45,1.0,st,1.0,28)
    rad=(r['speed']/math.radians(abs(r['yaw'])))/100 if abs(r['yaw'])>0.5 else 0
    print("5 %-5s rudder   yaw %5.1f deg/s  speed %4.0f (%3.0f km/h)  radius %5.0f m  roll %3.0f  sunk=%s"
          %(lab,r['yaw'],r['speed'],r['speed']*0.036,rad,r['mr'],r['sunk']))
    if r['sunk'] or r['mr']>65: fail+=1; print("  FAIL")
    if abs(r['yaw'])<12: fail+=1; print("  FAIL: will not turn")
print("6 frame rate")
for dt,lab in ((1/120,"120fps"),(1/60,"60fps"),(1/30,"30fps"),(1/20,"20fps")):
    r=sim(dt,45,1.0,0.5,1.0,28)
    print("   %-7s speed %4.0f  yaw %5.1f  roll %3.0f  z %5.0f  sunk=%s"%(lab,r['speed'],r['yaw'],r['mr'],r['z'],r['sunk']))
    if r['sunk'] or r['mr']>65: fail+=1; print("     FAIL")
r=sim(1/60,60,1.0,0.3,2.0,30); print("7 storm (rough=2) speed %.0f  pitch %.0f  roll %.0f  z %.0f..%.0f  sunk=%s"%(r['speed'],r['mp'],r['mr'],r['minz'],r['maxz'],r['sunk']))
if r['sunk']: fail+=1; print("  FAIL")
print("\nFAILURES: %d"%fail)
