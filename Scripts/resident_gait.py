"""Original in-place gait for the district resident's glTF rig. Coordinates are metres, Y up, Z forward."""
import math

# duration in seconds, nominal travel in metres per second
SAMPLE_RATE = 50
GAITS = {"A_Walk": (1.00, 1.45), "A_WalkAlt": (1.12, 1.23), "A_Jog": (.72, 3.35)}
X, Y, Z = (1., 0., 0.), (0., 1., 0.), (0., 0., 1.)
IDENTITY = (0., 0., 0., 1.)


def add(a, b):
    return tuple(x + y for x, y in zip(a, b))


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def scale(a, value):
    return tuple(x * value for x in a)


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def cross(a, b):
    return (a[1]*b[2] - a[2]*b[1], a[2]*b[0] - a[0]*b[2], a[0]*b[1] - a[1]*b[0])


def length(a):
    return math.sqrt(dot(a, a))


def unit(a):
    return scale(a, 1. / length(a))


def multiply(a, b):
    vector = add(add(scale(a[:3], b[3]), scale(b[:3], a[3])), cross(a[:3], b[:3]))
    return (*vector, a[3]*b[3] - dot(a[:3], b[:3]))


def inverse(q):
    return (-q[0], -q[1], -q[2], q[3])


def turn(axis, degrees):
    angle = .5 * math.radians(degrees)
    return (*scale(axis, math.sin(angle)), math.cos(angle))


def rotate(q, v):
    return multiply(multiply(q, (*v, 0.)), inverse(q))[:3]


def align(a, b):
    a, b = unit(a), unit(b)
    return unit((*cross(a, b), 1. + dot(a, b)))


def spline(t, knots):
    """Cubic Hermite interpolation. Tangents are derivatives per entire cycle."""
    for (a, x, dx), (b, y, dy) in zip(knots, knots[1:]):
        if t <= b:
            u = (t - a) / (b - a)
            return ((2*u**3 - 3*u*u + 1)*x + (u**3 - 2*u*u + u)*(b-a)*dx
                    + (-2*u**3 + 3*u*u)*y + (u**3-u*u)*(b-a)*dy)
    return knots[-1][1]


def ankle(t, stride, run, ground):
    stance = .33 if run else .56
    strike = stride * (.14 if run else .255)
    lift = .110 if run else .088
    toe_roll = .070
    if t <= stance:
        roll = max(0., (t / stance - .64) / .36)
        smooth = roll * roll * (3 - 2 * roll)
        forward = strike - stride*t + toe_roll*smooth
        height = ground + lift*roll**2
    else:
        end = strike - stride*stance + toe_roll
        forward = spline(t, [(stance, end, -stride), (stance+.032, end-.018, 0),
                             (.925, strike+.025, 0), (1., strike, -stride)])
        apex = .62 if run else .76
        height = ground + spline(t, [(stance, lift, 2*lift/(.36*stance)), (apex, .31 if run else .155, 0),
                                     (.92, .052, -.55), (1., 0, 0)])
    pitch = spline(t, [(0., -9., 0.), (.10, 0., 0.), (.64*stance, 0., 0.),
                       (stance, 35., 0.), (stance+.075, 40., 0.), (.89, -11., 0.), (1., -9., 0.)])
    return height, forward, pitch


def leg(rest, pose, hip, target, pelvis_rotation, side, foot_pitch):
    thigh = sub(rest["Knee"+side], rest["Hip"+side])
    calf = sub(rest["Foot"+side], rest["Knee"+side])
    thigh_length, calf_length = length(thigh), length(calf)
    line = sub(target, hip)
    d = min(length(line), thigh_length + calf_length - .0002)
    forward = unit(line)
    pole = unit(sub(Z, scale(forward, dot(Z, forward))))
    along = (thigh_length**2 - calf_length**2 + d*d) / (2*d)
    across = math.sqrt(max(0., thigh_length**2 - along**2))
    bent_thigh = add(scale(forward, along), scale(pole, across))
    bent_calf = sub(scale(forward, d), bent_thigh)
    upper, lower = align(thigh, bent_thigh), align(calf, bent_calf)
    pose["Hip"+side] = multiply(inverse(pelvis_rotation), upper)
    pose["Knee"+side] = multiply(inverse(upper), lower)
    pose["Foot"+side] = multiply(inverse(lower), turn(X, foot_pitch))


def gait_pose(name, phase, rest):
    """Return local rotations and the pelvis position in the fixed root frame."""
    period, speed = GAITS[name]
    run = name == "A_Jog"
    phase %= 1.
    cycle = phase * math.tau
    step = (phase * 2) % 1.
    wave = math.cos(cycle)
    roll = (1.8 if run else 2.4) * math.sin(cycle)
    yaw = (-5.8 if run else -4.) * wave
    pelvis_rotation = multiply(turn(Z, roll), turn(Y, yaw))
    resting = rest["Pelvis"]
    if run:
        drop = spline(step, [(0, -.067, -.10), (.24, -.068, 0.), (.48, -.058, .12), (.76, -.016, 0.), (1, -.067, -.10)])
    else:
        drop = spline(step, [(0, -.074, .04), (.16, -.045, .36), (.31, -.021, .20), (.56, -.003, 0.), (.76, -.019, -.21), (1, -.074, .04)])
    pelvis = add(resting, ((.012 if run else .020)*math.sin(cycle), drop, 0.))
    pose = {bone: IDENTITY for bone in rest}
    pose["Pelvis"] = pelvis_rotation
    pose["Spine"] = multiply(turn(Y, -yaw*.72), turn(X, (7. if run else 2.) + 1.2*math.sin(2*cycle)))
    pose["Chest"] = multiply(turn(Y, -yaw*.95), turn(Z, -roll*.60))
    pose["Neck"] = multiply(turn(Y, yaw*.48), turn(X, -4. if run else -1.))
    pose["Head"] = multiply(turn(Y, yaw*.19), turn(Z, -roll*.30))
    for side, sign in (("l", 1), ("r", -1)):
        t = (phase + (0 if sign == 1 else .5)) % 1.
        hip = add(pelvis, rotate(pelvis_rotation, sub(rest["Hip"+side], resting)))
        ground = rest["Foot"+side][1]
        height, fore, pitch = ankle(t, period*speed, run, ground)
        stance = .33 if run else .56
        clearance = .012*math.sin(math.pi*(t-stance)/(1.-stance))**2 if t > stance else 0.
        target = (sign*(.096 + clearance), height, fore)
        leg(rest, pose, hip, target, pelvis_rotation, side, pitch)
        swing = math.cos(t*math.tau-.18)
        pose["Shoulder"+side] = multiply(turn(X, (29. if run else 17.)*swing + (5. if run else 1.)),
                                         turn(Z, -sign*(30. if run else 34.)))
        pose["Elbow"+side] = turn(X, (-42. if run else 0.) + (13. if run else 5.)*swing)
    return pose, pelvis


def idle_pose(phase, rest):
    """Settle the original bent, outward-pointing bind arms beside the body."""
    pose = {bone: IDENTITY for bone in rest}
    breath = math.sin(math.tau*phase)
    pose["Spine"] = turn(X, .35*breath)
    pose["Head"] = multiply(turn(X, -.25*breath), turn(Y, .65*breath))
    for side, sign in (("l", 1), ("r", -1)):
        arm = sub(rest["Elbow"+side], rest["Shoulder"+side])
        forearm = sub(rest["Hand"+side], rest["Elbow"+side])
        upper = align(arm, (sign*.053, -.225, -.015))
        hanging = (sign*.008, -.218, .050 + .003*breath)
        lower = multiply(align(rotate(upper, forearm), hanging), upper)
        pose["Shoulder"+side] = upper
        pose["Elbow"+side] = multiply(inverse(upper), lower)
        pose["Hip"+side] = turn(Z, -sign*7.)
    return pose, rest["Pelvis"]
