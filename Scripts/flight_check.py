"""Numerical check of the jet flight model in Source/BoatRace/JetPawn.cpp.

Mirrors AJetPawn::Step force and rate equations (metres, seconds, radians) and reports
whether the jet holds height hands-off, reaches its top speeds, pulls no more
than its G limit, bleeds speed in a hard turn, recovers from a stall, rolls at
the right rate and flies the same at any frame rate. Run it after changing any
value in the [/Script/BoatRace.JetPawn] section of Config/DefaultGame.ini:

    python3 Scripts/flight_check.py

Must end in FAILURES: 0.
"""

import math, os, sys

# ---- Tuning: read from DefaultGame.ini so the check and the game agree -------
DEFAULTS = dict(
    Thrust=14.0, BoostThrust=26.0, Drag=3.5e-4, BrakeDrag=1.0e-3, InducedDrag=2.5e-4,
    Lift=2.1e-3, LiftSlope=5.73, StallAoA=15.0, SideForce=1.5e-3,
    MaxG=9.0, MaxRollRate=220.0, MaxYawRate=20.0, RateResponse=8.0, AoAGain=6.0,
    CruiseThrottle=0.6, StepRate=120.0,
    AimBankGain=2.5, AimPitchGain=0.6, AimMaxBank=65.0,
)
P = dict(DEFAULTS)
INI = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Config', 'DefaultGame.ini')
if os.path.exists(INI):
    section = None
    for line in open(INI):
        line = line.strip()
        if line.startswith('['):
            section = line
        elif section == '[/Script/BoatRace.JetPawn]' and '=' in line and not line.startswith(';'):
            key, value = line.split('=', 1)
            if key in P:
                P[key] = float(value)
G = 9.81

# ---- Vector and quaternion helpers (x forward, y right, z up) ----------------
def add(a, b): return (a[0]+b[0], a[1]+b[1], a[2]+b[2])
def sub(a, b): return (a[0]-b[0], a[1]-b[1], a[2]-b[2])
def mul(a, s): return (a[0]*s, a[1]*s, a[2]*s)
def dot(a, b): return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]
def length(a): return math.sqrt(dot(a, a))
def norm(a):
    n = length(a)
    return (0.0, 0.0, 0.0) if n < 1e-9 else mul(a, 1.0/n)

def qmul(a, b):
    aw, ax, ay, az = a; bw, bx, by, bz = b
    return (aw*bw-ax*bx-ay*by-az*bz, aw*bx+ax*bw+ay*bz-az*by,
            aw*by-ax*bz+ay*bw+az*bx, aw*bz+ax*by-ay*bx+az*bw)
def qconj(q): return (q[0], -q[1], -q[2], -q[3])
def qrot(q, v):
    r = qmul(qmul(q, (0.0,)+tuple(v)), qconj(q))
    return (r[1], r[2], r[3])
def qnorm(q):
    n = math.sqrt(sum(c*c for c in q))
    return tuple(c/n for c in q)
def qaxis(axis, angle):
    s = math.sin(angle/2)
    return (math.cos(angle/2), axis[0]*s, axis[1]*s, axis[2]*s)

# Body rates: roll about -x (right wing down), pitch about -y (nose up),
# yaw about +z (nose right). Written so each positive input means what a
# pilot expects; the C++ composes the same explicit body-axis quaternions.
def body_turn(q, roll, pitch, yaw):
    d = qmul(qmul(qaxis((0, 0, 1), yaw), qaxis((0, -1, 0), pitch)), qaxis((-1, 0, 0), roll))
    return qnorm(qmul(q, d))

def lift_coefficient(aoa):
    """Linear to the stall, falling to 60% of the peak 10 degrees past it."""
    a = abs(math.degrees(aoa))
    stall = P['StallAoA']
    peak = P['LiftSlope'] * math.radians(stall)
    if a <= stall:
        c = P['LiftSlope'] * math.radians(a)
    elif a <= stall + 10.0:
        c = peak - 0.4 * peak * (a - stall) / 10.0
    else:
        c = 0.6 * peak
    return math.copysign(c, aoa)

class Jet:
    def __init__(self, speed=150.0, height=1500.0, assisted=True):
        self.pos = (0.0, 0.0, height)
        self.vel = (speed, 0.0, 0.0)
        self.q = (1.0, 0.0, 0.0, 0.0)
        self.rates = [0.0, 0.0, 0.0]  # roll, pitch, yaw, rad/s
        self.assisted = assisted
        self.g = 1.0
        self.aoa = 0.0

    def fwd(self): return qrot(self.q, (1, 0, 0))
    def right(self): return qrot(self.q, (0, 1, 0))
    def up(self): return qrot(self.q, (0, 0, 1))

    def step(self, dt, pitch, roll, yaw, throttle, boost=False, brake=False):
        fwd, right, up = self.fwd(), self.right(), self.up()
        speed = max(length(self.vel), 1.0)
        vdir = mul(self.vel, 1.0/speed)
        aoa = math.atan2(-dot(self.vel, up), dot(self.vel, fwd))
        beta = math.atan2(dot(self.vel, right), dot(self.vel, fwd))
        self.aoa = aoa

        # ---- Controls: the stick asks for G, the jet finds the angle of attack.
        q_lift = P['Lift'] * P['LiftSlope'] * speed * speed  # m/s^2 per radian of AoA
        demand = pitch * P['MaxG'] * G
        if self.assisted:
            # Trim: carry the share of gravity the wings can, so hands off means level.
            demand += G * max(up[2], 0.0)
        demand = max(-P['MaxG'] * G, min(P['MaxG'] * G, demand))
        aoa_target = demand / q_lift
        stall = math.radians(P['StallAoA'])
        if self.assisted:
            aoa_target = max(min(aoa_target, stall * 0.92), -stall * 0.5)
        cl = lift_coefficient(aoa)
        lift_acc = P['Lift'] * cl * speed * speed
        lift_dir = norm(sub(up, mul(vdir, dot(up, vdir))))
        # How fast the flight path itself is bending in the lift plane, gravity
        # included: the nose follows it, plus a correction towards the target AoA.
        path_rate = (lift_acc - G * lift_dir[2]) / speed
        pitch_target = path_rate + P['AoAGain'] * (aoa_target - aoa)
        bank = math.atan2(right[2], up[2]) if abs(up[2]) > 1e-6 or abs(right[2]) > 1e-6 else 0.0
        roll_target = roll * math.radians(P['MaxRollRate'])
        if self.assisted and abs(roll) < 0.001 and abs(bank) < math.radians(60):
            roll_target = -1.2 * bank  # gentle wings-level
        yaw_target = yaw * math.radians(P['MaxYawRate']) + 2.0 * beta
        k = min(P['RateResponse'] * dt, 1.0)
        for i, target in enumerate((roll_target, pitch_target, yaw_target)):
            self.rates[i] += (target - self.rates[i]) * k

        # ---- Forces, as accelerations.
        side_dir = norm(sub(right, mul(vdir, dot(right, vdir))))
        drag_k = P['Drag'] + P['InducedDrag'] * cl * cl + (P['BrakeDrag'] if brake else 0.0)
        thrust = P['BoostThrust'] if boost else P['Thrust'] * throttle
        acc = mul(fwd, thrust)
        acc = add(acc, mul(lift_dir, lift_acc))
        acc = add(acc, mul(side_dir, -P['SideForce'] * beta * speed * speed))
        acc = add(acc, mul(vdir, -drag_k * speed * speed))
        aero = sub(acc, mul(fwd, thrust))
        self.g = dot(aero, up) / G
        acc = add(acc, (0.0, 0.0, -G))

        self.vel = add(self.vel, mul(acc, dt))
        self.pos = add(self.pos, mul(self.vel, dt))
        self.q = body_turn(self.q, self.rates[0]*dt, self.rates[1]*dt, self.rates[2]*dt)

def fly(jet, seconds, fps, controls):
    """Render frames at fps; the model steps at a fixed rate with an accumulator."""
    step = 1.0 / P['StepRate']
    acc = 0.0
    sim = 0.0
    frame = 1.0 / fps
    peak_g = 0.0
    while sim < seconds - 1e-9:
        acc += frame
        while acc >= step - 1e-12 and sim < seconds - 1e-9:
            # Controls are read on the model's clock, so a frame rate cannot
            # move the moment an input changes.
            jet.step(step, *controls(jet, sim))
            peak_g = max(peak_g, jet.g)
            acc -= step
            sim += step
    return peak_g

def aim_controls(jet, heading, pitch):
    """The same banking and path-angle controller as AJetPawn::AimControls."""
    fwd, right, up = jet.fwd(), jet.right(), jet.up()
    wrap = lambda a: (a + math.pi) % (2.0 * math.pi) - math.pi
    error = wrap(math.radians(heading) - math.atan2(fwd[1], fwd[0]))
    bank = math.atan2(-right[2], up[2])
    limit = math.radians(P['AimMaxBank'])
    desired = max(-limit, min(limit, error * P['AimBankGain']))
    rate = wrap(desired - bank) * 4.0 - jet.rates[0] * 0.35
    roll = max(-1.0, min(1.0, rate / math.radians(P['MaxRollRate'])))
    path_pitch = math.atan2(jet.vel[2], math.hypot(jet.vel[0], jet.vel[1]))
    pitch_error = math.radians(pitch) - path_pitch
    demand = (G * math.cos(path_pitch) + length(jet.vel) * P['AimPitchGain'] * pitch_error) / max(up[2], 0.25)
    trim = G * max(up[2], 0.0) if jet.assisted else 0.0
    stick_pitch = max(-1.0, min(1.0, (demand - trim) / (P['MaxG'] * G)))
    yaw = max(-0.3, min(0.3, error * 0.3))
    return (stick_pitch, roll, yaw, P['CruiseThrottle'])

FAIL = 0
def check(name, ok, detail):
    global FAIL
    print(('PASS ' if ok else 'FAIL ') + name + ': ' + detail)
    if not ok:
        FAIL += 1

kmh = lambda v: length(v) * 3.6
cruise = P['CruiseThrottle']

# 1. Hands off at cruise throttle: holds its height.
jet = Jet(speed=150.0)
fly(jet, 30.0, 60, lambda j, t: (0.0, 0.0, 0.0, cruise))
drift = jet.pos[2] - 1500.0
check('hands-off level flight', abs(drift) < 60.0, 'height change %.0f m over 30 s at %.0f km/h' % (drift, kmh(jet.vel)))

# 2. Top speeds, level.
jet = Jet(speed=150.0)
fly(jet, 60.0, 60, lambda j, t: (0.0, 0.0, 0.0, 1.0))
top = kmh(jet.vel)
check('top speed, full throttle', 650 < top < 800, '%.0f km/h' % top)
jet = Jet(speed=150.0)
fly(jet, 60.0, 60, lambda j, t: (0.0, 0.0, 0.0, 1.0, True))
boost = kmh(jet.vel)
check('top speed, afterburner', 880 < boost < 1050, '%.0f km/h' % boost)

# 3. Full pull at corner speed: within the G limit, a usable turn rate, no stall.
jet = Jet(speed=170.0)
heading0 = math.atan2(jet.vel[1], jet.vel[0])
stall_seen = [0.0]
def pull(j, t):
    stall_seen[0] = max(stall_seen[0], math.degrees(j.aoa))
    return (1.0, 0.0, 0.0, 1.0)
peak = fly(jet, 2.0, 60, pull)
pitch_turned = math.degrees(math.atan2(jet.vel[2], jet.vel[0]))
check('G limit', peak <= P['MaxG'] + 0.4, 'peak %.1f g (limit %.0f)' % (peak, P['MaxG']))
# Include the initial rate response: the two-second average is lower than
# the settled turn rate. Keep a meaningful minimum without rounding it.
check('turn rate', pitch_turned / 2.0 > 20.0, 'flight path turned %.0f deg in 2 s' % pitch_turned)
check('assisted pull never stalls', stall_seen[0] < P['StallAoA'], 'max angle of attack %.1f deg' % stall_seen[0])

# 4. A sustained hard turn without afterburner bleeds speed (energy management).
jet = Jet(speed=200.0)
def turn(j, t):
    # bank 80 degrees, then pull
    return (1.0 if t > 0.6 else 0.0, 1.0 if t < 0.4 else 0.0, 0.0, 1.0)
fly(jet, 10.0, 60, turn)
check('hard turn bleeds speed', kmh(jet.vel) < 200.0 * 3.6 * 0.85, '%.0f -> %.0f km/h in 10 s' % (720, kmh(jet.vel)))

# 5. Expert mode: throttle off, pull until it stalls, let go: recovers.
jet = Jet(speed=110.0, height=3000.0, assisted=False)
deep = [0.0]
def stall(j, t):
    deep[0] = max(deep[0], math.degrees(j.aoa))
    return (1.0 if t < 6.0 else 0.0, 0.0, 0.0, 0.0 if t < 6.0 else 1.0)
fly(jet, 20.0, 60, stall)
check('expert pull can stall', deep[0] > P['StallAoA'], 'max angle of attack %.1f deg' % deep[0])
check('stall recovery', kmh(jet.vel) > 300.0 and jet.pos[2] > 0.0, '%.0f km/h, %.0f m after letting go' % (kmh(jet.vel), jet.pos[2]))

# 6. Full roll rate: a whole roll in about two seconds.
jet = Jet(speed=170.0)
start_right = jet.right()
fly(jet, 360.0 / P['MaxRollRate'] + 0.15, 60, lambda j, t: (0.0, 1.0, 0.0, 1.0))
check('full roll', dot(jet.right(), start_right) > 0.8, 'roll time %.1f s' % (360.0 / P['MaxRollRate'] + 0.15))

# 7. The same manoeuvre at 20, 60 and 144 fps ends in the same place.
ends = []
for fps in (20, 60, 144):
    jet = Jet(speed=170.0)
    fly(jet, 6.0, fps, lambda j, t: (0.6, 0.4 if t < 1.0 else 0.0, 0.1, 1.0))
    ends.append(jet.pos)
spread = max(length(sub(a, b)) for a in ends for b in ends)
check('frame-rate independence', spread < 2.0, 'positions differ by %.2f m across 20/60/144 fps' % spread)

# Positive roll must lower the right wing; positive rudder turns right.
jet = Jet()
fly(jet, 0.5, 120, lambda j, t: (0.0, 0.5, 0.0, cruise))
check('right roll lowers right wing', jet.right()[2] < -0.4, 'right wing z %.2f' % jet.right()[2])
jet = Jet()
fly(jet, 1.0, 120, lambda j, t: (0.0, 0.0, 0.5, cruise))
check('right rudder turns right', jet.fwd()[1] > 0.05, 'heading %.1f deg' % math.degrees(math.atan2(jet.fwd()[1], jet.fwd()[0])))

for heading in (60.0, -60.0):
    jet = Jet()
    fly(jet, 15.0, 120, lambda j, t: aim_controls(j, heading, 10.0))
    actual_heading = math.degrees(math.atan2(jet.vel[1], jet.vel[0]))
    actual_pitch = math.degrees(math.atan2(jet.vel[2], math.hypot(jet.vel[0], jet.vel[1])))
    error = (actual_heading - heading + 180.0) % 360.0 - 180.0
    check('mouse aim %+.0f deg' % heading, abs(error) < 5.0 and abs(actual_pitch - 10.0) < 3.0,
          'heading %.1f deg, climb %.1f deg' % (actual_heading, actual_pitch))

print('FAILURES: %d' % FAIL)

sys.exit(1 if FAIL else 0)
