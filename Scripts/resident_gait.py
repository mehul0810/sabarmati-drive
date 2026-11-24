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
    d = dot(a, b)
    if d < -.99999:
        return turn(unit(cross(a, X if abs(a[0]) < .85 else Y)), 180.)
    return unit((*cross(a, b), 1. + d))


def arm_directions(rest, pose, side, upper, lower, palm, twist=0.):
    """Aim the upper arm, forearm and wrist in chest space."""
    upper_bind = sub(rest["Elbow"+side], rest["Shoulder"+side])
    fore_bind = sub(rest["Hand"+side], rest["Elbow"+side])
    shoulder = align(upper_bind, upper)
    forearm = multiply(align(rotate(shoulder, fore_bind), lower), shoulder)
    hand = multiply(turn(unit(palm), twist), multiply(align(rotate(forearm, fore_bind), palm), forearm))
    pose["Shoulder"+side] = shoulder
    pose["Elbow"+side] = multiply(inverse(shoulder), forearm)
    pose["Hand"+side] = multiply(inverse(forearm), hand)


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


def gait_pose(name, phase, rest, sampled_arms=None):
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
        if sampled_arms:
            upper, lower, palm = sampled_arms[side]
        else:
            swing = -math.cos(t*math.tau-.18)
            upper = (sign*.18, -1., (.63 if run else .26)*swing - .05)
            lower = (-sign*.10, -.56 if run else -1., (.57 if run else .24)*swing + (.87 if run else .23))
            palm = (lower[0], lower[1]-.06, lower[2]-.05)
        arm_directions(rest, pose, side, upper, lower, palm, sign*(4. if run else 1.))
    return pose, pelvis


def idle_pose(phase, rest):
    """Settle the original bent, outward-pointing bind arms beside the body."""
    pose = {bone: IDENTITY for bone in rest}
    breath = math.sin(math.tau*phase)
    pose["Spine"] = turn(X, .35*breath)
    pose["Head"] = multiply(turn(X, -.25*breath), turn(Y, .65*breath))
    for side, sign in (("l", 1), ("r", -1)):
        hanging = (sign*.008, -.218, .050 + .003*breath)
        arm_directions(rest, pose, side, (sign*.053, -.225, -.015), hanging, (0., -.22, .031))
        pose["Hip"+side] = turn(Z, -sign*7.)
    return pose, rest["Pelvis"]


# Phase, torso lean, pelvis drop, right foot lift/reach, left foot lift/reach.
# The right knee leads each move. A vault uses the left palm and a climb uses both.
TRAVERSALS = {
    "A_FenceVault": (.82, (
        (0.,    4., -.025, .0,   .11, .0,   -.16),
        (.15,  24., -.20,  .04,  .16, .015, -.20),
        (.35,  48., -.39,  .41,  .28, .13,  -.26),
        (.54,  33., -.26,  .25,  .45, .29,   .11),
        (.73,  15., -.16,  .05,  .32, .20,   .05),
        (.88,   9., -.20,  .00,  .18, .07,  -.16),
        (1.,    4., -.04,  .00,  .10, .00,  -.12),
    )),
    "A_WallMantle": (1.24, (
        (0.,    3., -.025, .0,   .07, .0,   -.13),
        (.16,  17., -.18,  .02,  .12, .0,   -.14),
        (.33,  38., -.31,  .36,  .26, .11,  -.21),
        (.52,  47., -.41,  .31,  .25, .18,  -.22),
        (.69,  43., -.42,  .10,  .26, .22,  -.12),
        (.84,  24., -.30,  .00,  .21, .08,  -.04),
        (1.,    4., -.04,  .00,  .10, .00,  -.115),
    )),
}


def traversal_pose(name, phase, rest):
    _, frames = TRAVERSALS[name]
    lean, drop, right_lift, right_reach, left_lift, left_reach = (
        spline(phase, [(frame[0], frame[i], 0.) for frame in frames]) for i in range(1, 7))
    mantle = name == "A_WallMantle"
    reach = spline(phase, [(0., 0., 0.), (.29, 1., 0.), (.48, .9, 0.), (.76, .34, 0.), (1., 0., 0.)])
    pell = rest["Pelvis"]
    pelvis = add(pell, (-.023*reach if not mantle else 0., drop, .054*reach))
    pose = {bone: IDENTITY for bone in rest}
    pelvis_rotation = multiply(turn(X, .36*lean), turn(Y, -5.*reach if not mantle else 0.))
    pose["Pelvis"] = pelvis_rotation
    pose["Spine"] = turn(X, .48*lean)
    pose["Chest"] = turn(X, .16*lean)
    pose["Neck"] = turn(X, -.56*lean)
    pose["Head"] = turn(X, -.31*lean)
    for side, sign, lift, fore in (("l", 1, left_lift, left_reach), ("r", -1, right_lift, right_reach)):
        hip = add(pelvis, rotate(pelvis_rotation, sub(rest["Hip"+side], pell)))
        foot = rest["Foot"+side]
        leg(rest, pose, hip, (sign*.12, foot[1]+lift, fore), pelvis_rotation, side, 14.*reach)
        support = 1. if mantle or sign == 1 else 0.
        upper = (sign*(.20+(.13 if support else .91)*reach), -1.+.57*reach,
                 .12+reach*(.85 if support else -.47))
        lower = (sign*.05, -.85+.35*reach, .55+.22*reach)
        palm = (sign*.06, -.62, .55)
        if support:
            along_edge = rotate(turn(X, -lean), (sign*.06, -.09, .98))
            palm = add(scale(palm, 1.-reach), scale(along_edge, reach))
        arm_directions(rest, pose, side, upper, lower, palm, -sign*13.*reach)
    return pose, pelvis
