"""Plotter5Bar generator for Autodesk Fusion.

Builds the whole tapbot dual five-bar plotter from scratch in a new design:
User Parameters, one component per part, dimensioned sketches, the default
pose (tip at (0, Ym), elbows out) and the as-built joints with their limits.

Run it from Utilities > Scripts and Add-Ins. Edit PARAMS below to change the
geometry, then run it again: it always opens a new design and never touches
open documents.
"""
import adsk.core, adsk.fusion, math, traceback

app = adsk.core.Application.get()
V = adsk.core.ValueInput.createByString
P3 = adsk.core.Point3D.create
FO = adsk.fusion.FeatureOperations

PARAMS = [
    # name, expression, comment
    ('D', '50 mm', 'Motor axis spacing; motors at X = +/-D/2, Y = 0'),
    ('L1', '85 mm', 'Proximal arm length, centre to centre'),
    ('L2', '125 mm', 'Distal arm length, centre to centre'),
    ('Y0', '50 mm', 'Workspace near edge (Y)'),
    ('WsW', '200 mm', 'Workspace width (X from -WsW/2 to +WsW/2)'),
    ('WsH', '100 mm', 'Workspace height (Y from Y0 to Y0+WsH)'),
    ('Ym', 'Y0 + WsH/2', 'Mirror line Y of the dual five-bar'),
    ('PivotY', '2*Ym', 'Y of the passive (unmotorized) pivots'),
    ('BrgOD', '24.5 mm', 'Bearing outer diameter (jo)'),
    ('BrgID', '5 mm', 'Bearing bore (measured by jo)'),
    ('BrgW', '11 mm', 'Bearing width'),
    ('ClrHole', '5.4 mm', 'M5 clearance hole (the 4 M5x30 bearing axles)'),
    ('NutAF', '8 mm', 'M5 nut across flats (bearing axles)'),
    ('NutH', '4 mm', 'M5 nut height'),
    ('M3Clr', '3.4 mm', 'M3 clearance hole'),
    ('M4Clr', '4.4 mm', 'M4 clearance hole'),
    ('M4Pilot', '3.3 mm', 'M4 self-tapping pilot hole in plastic'),
    ('M4L', '9 mm', 'M4 screw length (jo has only M4x9)'),
    ('M4HeadD', '7 mm', 'M4 socket head diameter'),
    ('M4HeadH', '4 mm', 'M4 socket head height'),
    ('M5L', '30 mm', 'M5 axle screw length (4x M5x30)'),
    ('M5HeadD', '8.5 mm', 'M5 socket head diameter'),
    ('M5HeadH', '5 mm', 'M5 socket head height'),
    ('PinD', '8 mm', 'Printed pin diameter'),
    ('PinHole', 'PinD + 0.3 mm', 'Eye hole for printed pins'),
    ('PinPlay', '0.3 mm', 'Axial play under pin caps'),
    ('CapD', '14 mm', 'Printed pin cap (washer) diameter'),
    ('CapT', '2 mm', 'Printed pin cap thickness'),
    ('PlateFlange', '2 mm', 'Plate material under the M4 counterbores'),
    ('NemaW', '42.3 mm', 'NEMA17 body width'),
    ('NemaL', '40 mm', 'NEMA17 body length (Tevo Tarantula)'),
    ('NemaHole', '31 mm', 'NEMA17 M3 hole spacing'),
    ('NemaBoss', '22.5 mm', 'NEMA17 centring boss diameter'),
    ('Shaft', '5 mm', 'NEMA17 D-shaft diameter'),
    ('ShaftL', '24 mm', 'NEMA17 shaft length from face'),
    ('ArmW', '12 mm', 'Arm bar width'),
    ('ArmT', '6 mm', 'Arm bar thickness'),
    ('Clearance', '0.2 mm', 'Print clearance'),
    ('HubD', '20 mm', 'Motor shaft hub diameter'),
    ('LayerGap', '1 mm', 'Z gap (washer) between stacked links'),
    ('SolW', '13 mm', 'Solenoid JF-0530B body width (X)'),
    ('SolH', '15 mm', 'Solenoid JF-0530B body height (Y)'),
    ('SolL', '30 mm', 'Solenoid JF-0530B body length (along plunger)'),
    ('SolStroke', '10 mm', 'Solenoid stroke'),
    ('SolPlD', '6 mm', 'Solenoid plunger diameter'),
    ('SolTipOut', '5 mm', 'Plunger protrusion at rest (ASSUMED)'),
    ('FrameT', '6 mm', 'Mount plate thickness'),
    ('RailW', '15 mm', 'Rail width'),
    ('RailH', '10 mm', 'Rail height'),
    ('Wall', '3 mm', 'Wall around bearing seats'),
    ('HWall', '2.5 mm', 'Solenoid holder wall'),
    ('FilletR', '3 mm', 'Arm bar-to-eye fillet'),
    ('FootClr', '5 mm', 'Table clearance below motors'),
    ('PhoneT', '10 mm', 'Phone thickness (with case)'),
    ('TipGap', '3 mm', 'Plunger tip to screen gap at rest'),
    ('RailIn', '50 mm', 'Inner X end of rail cross-arms'),
    ('PlateW', '140 mm', 'Mount plate width (X)'),
    ('PlateDp', 'NemaW + 10 mm', 'Mount plate depth (Y)'),
    # derived
    ('EyeD', 'ArmW + 4 mm', 'Diameter of M5 eyes'),
    ('BossD', 'BrgOD + 2*Wall', 'Diameter of bearing bosses'),
    ('HubZ0', 'FrameT + LayerGap', 'Bottom Z of motor hub'),
    ('HubH', 'ShaftL + LayerGap - HubZ0', 'Motor hub height (covers the shaft)'),
    ('ZProx', 'HubZ0 + HubH - ArmT', 'Bottom Z of proximal arm bars'),
    ('Pitch', 'ArmT + LayerGap', 'Z pitch of link levels'),
    ('ZD1', 'ZProx + Pitch', 'Bottom Z of distal level 1'),
    ('ZD2', 'ZD1 + Pitch', 'Bottom Z of distal level 2'),
    ('ZPP', 'ZD1 + 4*Pitch', 'Bottom Z of passive proximal arms'),
    ('ZJm', 'ZD2 - LayerGap/2', 'Joint plane Z, motor side'),
    ('ZJp', 'ZJm + 2*Pitch', 'Joint plane Z, passive side'),
    ('ElbowBossH', 'BrgW + 2 mm', 'Proximal elbow bearing boss height'),
    ('TowerTop', 'ZPP - LayerGap', 'Top Z of passive pivot towers'),
    ('TableZ', '-(NemaL + FootClr)', 'Table / phone back Z'),
    ('SolBot', 'TableZ + PhoneT + TipGap + SolTipOut', 'Solenoid body bottom Z'),
    ('RailX', 'WsW/2 + 30 mm', 'Rail centre X'),
    ('BoltX1', 'RailIn + 5 mm', 'Rail-to-mount bolt 1 X'),
    ('BoltX2', 'RailIn + 15 mm', 'Rail-to-mount bolt 2 X'),
]


def ev(expr):
    """Evaluate an expression to cm."""
    return app.activeProduct.unitsManager.evaluateExpression(expr, 'mm')


def coord(expr):
    """'-D/2' -> (value_cm, 'D/2'); '0' -> (0, None)."""
    if expr in (None, '0'):
        return 0.0, None
    v = ev(expr)
    absx = expr[1:] if expr.startswith('-') else expr
    return v, absx


def _dim(sk, pt, orient, expr, tp):
    try:
        d = sk.sketchDimensions.addDistanceDimension(sk.originPoint, pt, orient, tp)
    except RuntimeError as e:
        if 'OVER_CONSTRAINTS' in str(e):
            return None  # already fixed by an inferred collinear/aligned constraint
        raise
    d.parameter.expression = expr
    return d


def _con(fn, a, b):
    try:
        fn(a, b)
    except RuntimeError as e:
        if 'OVER_CONSTRAINTS' not in str(e):
            raise


def pin(sk, pt, ex, ey):
    o = sk.originPoint
    gc = sk.geometricConstraints
    x, ax = coord(ex)
    y, ay = coord(ey)
    if ax:
        _dim(sk, pt, adsk.fusion.DimensionOrientations.HorizontalDimensionOrientation, ax, P3(x / 2, y - 0.5, 0))
    else:
        _con(gc.addVerticalPoints, o, pt)
    if ay:
        _dim(sk, pt, adsk.fusion.DimensionOrientations.VerticalDimensionOrientation, ay, P3(x - 0.5, y / 2, 0))
    else:
        _con(gc.addHorizontalPoints, o, pt)


def circle(sk, ex, ey, dexpr):
    x, _ = coord(ex)
    y, _ = coord(ey)
    c = sk.sketchCurves.sketchCircles.addByCenterRadius(P3(x, y, 0), ev(dexpr) / 2)
    pin(sk, c.centerSketchPoint, ex, ey)
    d = sk.sketchDimensions.addDiameterDimension(c, P3(x + ev(dexpr), y + ev(dexpr), 0))
    d.parameter.expression = dexpr
    return c


def rect(sk, ex0, ey0, ex1, ey1):
    x0, _ = coord(ex0); y0, _ = coord(ey0)
    x1, _ = coord(ex1); y1, _ = coord(ey1)
    lines = sk.sketchCurves.sketchLines.addTwoPointRectangle(P3(x0, y0, 0), P3(x1, y1, 0))
    pts = []
    for i in range(lines.count):
        pts.append(lines.item(i).startSketchPoint)

    def nearest(x, y):
        return min(pts, key=lambda p: (p.geometry.x - x) ** 2 + (p.geometry.y - y) ** 2)
    pin(sk, nearest(x0, y0), ex0, ey0)
    pin(sk, nearest(x1, y1), ex1, ey1)
    return lines


def sketch(comp, name, plane=None):
    sk = comp.sketches.add(plane or comp.xYConstructionPlane)
    sk.name = name
    sk.isComputeDeferred = True
    return sk


def done(sk):
    sk.isComputeDeferred = False
    return sk


def profs(sk, pick='all'):
    oc = adsk.core.ObjectCollection.create()
    ps = [sk.profiles.item(i) for i in range(sk.profiles.count)]
    if pick == 'largest':
        ps = [max(ps, key=lambda p: p.areaProperties().area)]
    elif pick == 'notsmallest':
        sm = min(ps, key=lambda p: p.areaProperties().area)
        ps = [p for p in ps if p != sm]
    for p in ps:
        oc.add(p)
    return oc


def ext(comp, sk, op, z0, h, pick='all', name=None):
    done(sk)
    inp = comp.features.extrudeFeatures.createInput(profs(sk, pick), op)
    if z0 not in (None, '0'):
        inp.startExtent = adsk.fusion.OffsetStartDefinition.create(V(z0))
    inp.setOneSideExtent(adsk.fusion.DistanceExtentDefinition.create(V(h)),
                         adsk.fusion.ExtentDirections.PositiveExtentDirection)
    f = comp.features.extrudeFeatures.add(inp)
    if name:
        f.name = name
    return f


def offset_plane(comp, expr, name):
    pi = comp.constructionPlanes.createInput()
    pi.setByOffset(comp.xYConstructionPlane, V(expr))
    pl = comp.constructionPlanes.add(pi)
    pl.name = name
    return pl


def joint_points(comp, zexpr, pts, name='JointPts'):
    pl = offset_plane(comp, zexpr, name + '_Plane')
    sk = sketch(comp, name, pl)
    out = []
    for ex, ey in pts:
        if ex in (None, '0') and ey in (None, '0'):
            out.append(sk.originPoint)
            continue
        x, _ = coord(ex); y, _ = coord(ey)
        p = sk.sketchPoints.add(P3(x, y, 0))
        pin(sk, p, ex, ey)
        out.append(p)
    done(sk)
    return out


def fillet_vertical(comp, maxlen_expr='ArmT'):
    body = comp.bRepBodies.item(0)
    lim = ev(maxlen_expr) + 1e-4
    edges = adsk.core.ObjectCollection.create()
    for e in body.edges:
        g = e.geometry
        if isinstance(g, adsk.core.Line3D):
            s, t = g.startPoint, g.endPoint
            if abs(s.x - t.x) < 1e-5 and abs(s.y - t.y) < 1e-5 and abs(s.z - t.z) <= lim:
                edges.add(e)
    if edges.count == 0:
        return None
    fi = comp.features.filletFeatures.createInput()
    try:
        fi.edgeSetInputs.addConstantRadiusEdgeSet(edges, V('FilletR'), True)
    except AttributeError:
        fi.addConstantRadiusEdgeSet(edges, V('FilletR'), True)
    f = comp.features.filletFeatures.add(fi)
    f.name = 'ArmFillets'
    return f


def name_body(comp):
    for i in range(comp.bRepBodies.count):
        comp.bRepBodies.item(i).name = comp.name if i == 0 else '%s_%d' % (comp.name, i)


def mat(x=0.0, y=0.0, z=0.0, ang=0.0, flip=False):
    m = adsk.core.Matrix3D.create()
    c, s = math.cos(ang), math.sin(ang)
    xa = adsk.core.Vector3D.create(c, s, 0)
    if flip:
        ya = adsk.core.Vector3D.create(s, -c, 0)
        za = adsk.core.Vector3D.create(0, 0, -1)
    else:
        ya = adsk.core.Vector3D.create(-s, c, 0)
        za = adsk.core.Vector3D.create(0, 0, 1)
    m.setWithCoordinateSystem(P3(x, y, z), xa, ya, za)
    return m


# ---------------------------------------------------------------- parts

def build_motor_mount(c):
    sk = sketch(c, 'Plate')
    rect(sk, '-PlateW/2', '-PlateDp/2', 'PlateW/2', 'PlateDp/2')
    ext(c, sk, FO.NewBodyFeatureOperation, '0', 'FrameT', name='Plate')
    sk = sketch(c, 'MotorHoles')
    for sx in ('-', ''):
        circle(sk, sx + 'D/2', '0', 'NemaBoss + 1 mm')
        for hx in ('(D/2 + NemaHole/2)', '(D/2 - NemaHole/2)'):
            for sy in ('-', ''):
                circle(sk, sx + hx, sy + 'NemaHole/2', 'M3Clr')
    ext(c, sk, FO.CutFeatureOperation, '0', 'FrameT', name='MotorHoles')
    plate_common(c, '0')
    joint_points(c, 'ZJm', [('-D/2', '0'), ('D/2', '0')])


def plate_common(c, yexpr):
    """Rail screw holes (M4x9, counterbored) and lightening pockets on a mount plate centred at Y=yexpr."""
    def yy(off):
        if yexpr == '0':
            return off
        if off.startswith('-'):
            return '%s - %s' % (yexpr, off[1:])
        return '%s + %s' % (yexpr, off)
    sk = sketch(c, 'RailScrews')
    for sx in ('-', ''):
        for bx in ('BoltX1', 'BoltX2'):
            circle(sk, sx + bx, yexpr, 'M4Clr')
    ext(c, sk, FO.CutFeatureOperation, '0', 'FrameT', name='RailScrews')
    sk = sketch(c, 'RailScrewCbore')
    for sx in ('-', ''):
        for bx in ('BoltX1', 'BoltX2'):
            circle(sk, sx + bx, yexpr, 'M4HeadD + 1 mm')
    ext(c, sk, FO.CutFeatureOperation, 'PlateFlange', 'FrameT - PlateFlange', name='RailScrewCbore')
    sk = sketch(c, 'Pockets')
    for sx in ('-', ''):
        for sy in ('-', ''):
            rect(sk, sx + '(PlateW/2 - 18 mm)', yy(sy + '9 mm'), sx + '(PlateW/2 - 3 mm)', yy(sy + '(PlateDp/2 - 4 mm)'))
    ext(c, sk, FO.CutFeatureOperation, '0', 'FrameT', name='Pockets')


def build_pivot_mount(c):
    sk = sketch(c, 'Plate')
    rect(sk, '-PlateW/2', 'PivotY - PlateDp/2', 'PlateW/2', 'PivotY + PlateDp/2')
    ext(c, sk, FO.NewBodyFeatureOperation, '0', 'FrameT', name='Plate')
    sk = sketch(c, 'Towers')
    circle(sk, '-D/2', 'PivotY', 'BossD')
    circle(sk, 'D/2', 'PivotY', 'BossD')
    ext(c, sk, FO.JoinFeatureOperation, 'FrameT', 'TowerTop - FrameT', name='Towers')
    sk = sketch(c, 'BearingSeats')
    circle(sk, '-D/2', 'PivotY', 'BrgOD')
    circle(sk, 'D/2', 'PivotY', 'BrgOD')
    ext(c, sk, FO.CutFeatureOperation, 'TowerTop - BrgW', 'BrgW', name='BearingSeats')
    sk = sketch(c, 'AxleRelief')
    circle(sk, '-D/2', 'PivotY', 'M5HeadD + 1.5 mm')
    circle(sk, 'D/2', 'PivotY', 'M5HeadD + 1.5 mm')
    ext(c, sk, FO.CutFeatureOperation, '0', 'TowerTop', name='AxleRelief')
    plate_common(c, 'PivotY')
    joint_points(c, 'ZJp', [('-D/2', 'PivotY'), ('D/2', 'PivotY')])


def build_rail(c):
    sk = sketch(c, 'Rail')
    rect(sk, 'RailX - RailW/2', '-RailW/2', 'RailX + RailW/2', 'PivotY + RailW/2')
    rect(sk, 'RailIn', '-RailW/2', 'RailX', 'RailW/2')
    rect(sk, 'RailIn', 'PivotY - RailW/2', 'RailX', 'PivotY + RailW/2')
    ext(c, sk, FO.NewBodyFeatureOperation, '-RailH', 'RailH', name='Rail')
    sk = sketch(c, 'Feet')
    rect(sk, 'RailX - RailW/2', '-RailW/2', 'RailX + RailW/2', 'RailW/2')
    rect(sk, 'RailX - RailW/2', 'PivotY - RailW/2', 'RailX + RailW/2', 'PivotY + RailW/2')
    ext(c, sk, FO.JoinFeatureOperation, 'TableZ', '-RailH - TableZ', name='Feet')
    sk = sketch(c, 'PilotHoles')
    for y in ('0', 'PivotY'):
        for bx in ('BoltX1', 'BoltX2'):
            circle(sk, bx, y, 'M4Pilot')
    ext(c, sk, FO.CutFeatureOperation, '-RailH', 'RailH', name='PilotHoles')


def radial_hole(c, name, zexpr, dexpr, lexpr, op=FO.CutFeatureOperation):
    """Hole/boss along -X from the component YZ plane, centred at Z=zexpr."""
    sk = sketch(c, name, c.yZConstructionPlane)
    sp = sk.modelToSketchSpace(P3(0, 0, ev(zexpr)))
    sk.sketchCurves.sketchCircles.addByCenterRadius(sp, ev(dexpr) / 2)
    done(sk)
    gi = c.features.extrudeFeatures.createInput(profs(sk), op)
    gi.setOneSideExtent(adsk.fusion.DistanceExtentDefinition.create(V(lexpr)),
                        adsk.fusion.ExtentDirections.NegativeExtentDirection)
    g = c.features.extrudeFeatures.add(gi)
    g.name = name
    return g


def build_prox(c):
    sk = sketch(c, 'Bar')
    rect(sk, '0', '-ArmW/2', 'L1', 'ArmW/2')
    ext(c, sk, FO.NewBodyFeatureOperation, 'ZProx', 'ArmT', name='Bar')
    sk = sketch(c, 'Hub')
    circle(sk, '0', '0', 'HubD')
    ext(c, sk, FO.JoinFeatureOperation, 'HubZ0', 'HubH', name='Hub')
    sk = sketch(c, 'ElbowBoss')
    circle(sk, 'L1', '0', 'BossD')
    ext(c, sk, FO.JoinFeatureOperation, 'ZProx + ArmT - ElbowBossH', 'ElbowBossH', name='ElbowBoss')
    fillet_vertical(c)
    # D bore
    sk = sketch(c, 'DBore')
    circle(sk, '0', '0', 'Shaft + Clearance')
    r = (ev('Shaft') + ev('Clearance')) / 2
    xf = -(ev('Shaft') / 2 - 0.05)
    yf = math.sqrt(r * r - xf * xf)
    sk.sketchCurves.sketchLines.addByTwoPoints(P3(xf, -yf, 0), P3(xf, yf, 0))
    ext(c, sk, FO.CutFeatureOperation, 'HubZ0', 'HubH', pick='largest', name='DBore')
    # M4x9 self-tapping grub onto the flat (from -X)
    radial_hole(c, 'GrubHole', 'HubZ0 + HubH/2', 'M4Pilot', 'HubD/2 + 1 mm')
    # bearing seat + relief for the M5 axle head
    sk = sketch(c, 'BearingSeat')
    circle(sk, 'L1', '0', 'BrgOD')
    ext(c, sk, FO.CutFeatureOperation, 'ZProx + ArmT - BrgW', 'BrgW', name='BearingSeat')
    sk = sketch(c, 'AxleRelief')
    circle(sk, 'L1', '0', 'M5HeadD + 1.5 mm')
    ext(c, sk, FO.CutFeatureOperation, 'ZProx + ArmT - ElbowBossH - 1 mm', 'ElbowBossH + 2 mm', name='AxleRelief')
    return joint_points(c, 'ZJm', [('L1', '0')])


def build_distal(c):
    sk = sketch(c, 'Bar')
    rect(sk, '0', '-ArmW/2', 'L2', 'ArmW/2')
    ext(c, sk, FO.NewBodyFeatureOperation, 'ZD2 + LayerGap/2', 'ArmT', name='Bar')  # flush with the eyes: prints flat
    sk = sketch(c, 'EffectorEye')
    circle(sk, 'L2', '0', 'EyeD')
    ext(c, sk, FO.JoinFeatureOperation, 'ZD2 - LayerGap/2', 'Pitch', name='EffectorEye')
    sk = sketch(c, 'ElbowEye')
    circle(sk, '0', '0', 'EyeD')
    ext(c, sk, FO.JoinFeatureOperation, 'ZD1 - LayerGap/2', '2*Pitch', name='ElbowEye')
    fillet_vertical(c)
    sk = sketch(c, 'PinHoles')
    circle(sk, '0', '0', 'PinHole')
    circle(sk, 'L2', '0', 'PinHole')
    ext(c, sk, FO.CutFeatureOperation, 'ZD1 - 1 mm', '2*Pitch + 2 mm', name='PinHoles')


def build_pprox(c):
    sk = sketch(c, 'Bar')
    rect(sk, '0', '-ArmW/2', 'L1', 'ArmW/2')
    ext(c, sk, FO.NewBodyFeatureOperation, 'ZPP', 'ArmT', name='Bar')
    sk = sketch(c, 'Eyes')
    circle(sk, '0', '0', 'EyeD')
    circle(sk, 'L1', '0', 'EyeD')
    ext(c, sk, FO.JoinFeatureOperation, 'ZPP - LayerGap/2', 'ArmT + LayerGap/2', name='Eyes')
    fillet_vertical(c)
    # spacer sitting on the bearing inner ring
    sk = sketch(c, 'BaseSpacer')
    circle(sk, '0', '0', 'BrgID + 3 mm')
    ext(c, sk, FO.JoinFeatureOperation, 'TowerTop', 'ZPP - LayerGap/2 - TowerTop', name='BaseSpacer')
    # printed elbow pin (points down through the distal elbow eye)
    sk = sketch(c, 'ElbowPin')
    circle(sk, 'L1', '0', 'PinD')
    ext(c, sk, FO.JoinFeatureOperation, 'ZD1 + 2*Pitch - LayerGap/2 - PinPlay', 'ZPP - ZD1 - 2*Pitch + PinPlay', name='ElbowPin')
    sk = sketch(c, 'ElbowPinPilot')
    circle(sk, 'L1', '0', 'M4Pilot')
    ext(c, sk, FO.CutFeatureOperation, 'ZD1 + 2*Pitch - LayerGap/2 - PinPlay', 'M4L + 1 mm', name='ElbowPinPilot')
    sk = sketch(c, 'AxleHole')
    circle(sk, '0', '0', 'ClrHole')
    ext(c, sk, FO.CutFeatureOperation, 'TowerTop - 1 mm', 'ZPP + ArmT - TowerTop + 2 mm', name='AxleHole')
    return joint_points(c, 'ZJp', [('L1', '0')])


def build_hub(c):
    sk = sketch(c, 'Sleeve')
    rect(sk, '-(SolW/2 + Clearance + HWall)', '-(SolH/2 + Clearance + HWall)',
         'SolW/2 + Clearance + HWall', 'SolH/2 + Clearance + HWall')
    ext(c, sk, FO.NewBodyFeatureOperation, 'SolBot', 'ZProx - SolBot', name='Sleeve')
    sk = sketch(c, 'Collar')
    circle(sk, '0', '0', 'EyeD')
    ext(c, sk, FO.JoinFeatureOperation, 'ZProx', 'ArmT + LayerGap/2', name='Collar')
    # printed effector pin: the 4 distal eyes stack on it, cap + M4x9 on top
    sk = sketch(c, 'EffectorPin')
    circle(sk, '0', '0', 'PinD')
    ext(c, sk, FO.JoinFeatureOperation, 'ZProx + ArmT + LayerGap/2', '4*Pitch + PinPlay', name='EffectorPin')
    sk = sketch(c, 'EffectorPinPilot')
    circle(sk, '0', '0', 'M4Pilot')
    ext(c, sk, FO.CutFeatureOperation, 'ZProx + ArmT + LayerGap/2 + 4*Pitch + PinPlay - M4L - 1 mm', 'M4L + 1 mm',
        name='EffectorPinPilot')
    # clamp boss so an M4x9 just reaches the solenoid body
    radial_hole(c, 'ClampBoss', 'SolBot + SolL/2', 'M4HeadD + 3 mm', 'SolW/2 + Clearance + M4L - 0.5 mm', FO.JoinFeatureOperation)
    sk = sketch(c, 'SolenoidPocket')
    rect(sk, '-(SolW/2 + Clearance)', '-(SolH/2 + Clearance)', 'SolW/2 + Clearance', 'SolH/2 + Clearance')
    ext(c, sk, FO.CutFeatureOperation, 'SolBot', 'SolL', name='SolenoidPocket')
    sk = sketch(c, 'PlungerRelief')
    circle(sk, '0', '0', 'SolPlD + 2 mm')
    ext(c, sk, FO.CutFeatureOperation, 'SolBot + SolL', 'SolStroke', name='PlungerRelief')
    radial_hole(c, 'ClampPilot', 'SolBot + SolL/2', 'M4Pilot', 'SolW/2 + Clearance + M4L')
    a = joint_points(c, 'ZJm', [('0', '0')], 'JointPtsMotor')
    b = joint_points(c, 'ZJp', [('0', '0')], 'JointPtsPassive')
    return a[0], b[0]


def build_m5_sleeve(c):
    """Flanged sleeve: fills the 8 mm distal elbow hole around the M5 axle and spaces it off the bearing inner ring."""
    sk = sketch(c, 'Flange')
    circle(sk, '0', '0', 'PinHole + 1.5 mm')
    ext(c, sk, FO.NewBodyFeatureOperation, 'ZProx + ArmT', 'LayerGap/2', name='Flange')
    sk = sketch(c, 'Body')
    circle(sk, '0', '0', 'PinD')
    ext(c, sk, FO.JoinFeatureOperation, 'ZProx + ArmT + LayerGap/2', '2*Pitch', name='Body')
    sk = sketch(c, 'Bore')
    circle(sk, '0', '0', 'ClrHole')
    ext(c, sk, FO.CutFeatureOperation, 'ZProx + ArmT - 1 mm', '2*Pitch + 2 mm', name='Bore')


def build_cap(c):
    sk = sketch(c, 'Cap')
    circle(sk, '0', '0', 'CapD')
    ext(c, sk, FO.NewBodyFeatureOperation, '0', 'CapT', name='Cap')
    sk = sketch(c, 'Hole')
    circle(sk, '0', '0', 'M4Clr')
    ext(c, sk, FO.CutFeatureOperation, '0', 'CapT', name='Hole')


def build_nema(c):
    sk = sketch(c, 'Body')
    rect(sk, '-NemaW/2', '-NemaW/2', 'NemaW/2', 'NemaW/2')
    ext(c, sk, FO.NewBodyFeatureOperation, '-NemaL', 'NemaL', name='Body')
    sk = sketch(c, 'Boss')
    circle(sk, '0', '0', 'NemaBoss')
    ext(c, sk, FO.JoinFeatureOperation, '0', '2 mm', name='Boss')
    sk = sketch(c, 'Shaft')
    circle(sk, '0', '0', 'Shaft')
    ext(c, sk, FO.JoinFeatureOperation, '0', 'ShaftL', name='Shaft')


def build_bearing(c):
    sk = sketch(c, 'Ring')
    circle(sk, '0', '0', 'BrgOD')
    circle(sk, '0', '0', 'BrgID')
    ext(c, sk, FO.NewBodyFeatureOperation, '0', 'BrgW', pick='notsmallest', name='Ring')


def build_solenoid(c):
    sk = sketch(c, 'Body')
    rect(sk, '-SolW/2', '-SolH/2', 'SolW/2', 'SolH/2')
    ext(c, sk, FO.NewBodyFeatureOperation, '0', 'SolL', name='Body')
    sk = sketch(c, 'Plunger')
    circle(sk, '0', '0', 'SolPlD')
    ext(c, sk, FO.JoinFeatureOperation, '-SolTipOut', 'SolL + SolTipOut + 3 mm', name='Plunger')


def build_screw(c, d, l, hd, hh):
    """Origin at the head underside: shank along +Z, head below."""
    sk = sketch(c, 'Shank')
    circle(sk, '0', '0', d)
    ext(c, sk, FO.NewBodyFeatureOperation, '0', l, name='Shank')
    sk = sketch(c, 'Head')
    circle(sk, '0', '0', hd)
    ext(c, sk, FO.JoinFeatureOperation, '-(%s)' % hh, hh, name='Head')


def build_nut(c):
    sk = sketch(c, 'Nut')
    sk.sketchCurves.sketchLines.addScribedPolygon(P3(0, 0, 0), 6, 0, ev('NutAF') / 2, False)
    circle(sk, '0', '0', 'ClrHole')
    ext(c, sk, FO.NewBodyFeatureOperation, '0', 'NutH', pick='notsmallest', name='Nut')


# ---------------------------------------------------------------- assembly

def ik(px, py, mx, my, sgn):
    """Return (shoulder angle, elbow x, elbow y). sgn=+1 elbow rotated CCW from the base->effector line."""
    L1, L2 = ev('L1'), ev('L2')
    dx, dy = px - mx, py - my
    d = math.hypot(dx, dy)
    a = math.atan2(dy, dx)
    b = math.acos((L1 * L1 + d * d - L2 * L2) / (2 * L1 * d))
    th = a + sgn * b
    return th, mx + L1 * math.cos(th), my + L1 * math.sin(th)


JOINT_LIMITS = {  # degrees relative to the default pose; keeps every five-bar elbow-out
    'Rev_MotorL': (-90, 90), 'Rev_MotorR': (-90, 90), 'Rev_PivotL': (-90, 90), 'Rev_PivotR': (-90, 90),
    'Rev_ElbowL': (-50, 95), 'Rev_PElbowR': (-50, 95), 'Rev_ElbowR': (-95, 50), 'Rev_PElbowL': (-95, 50)}


def main():
    doc = app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
    des = adsk.fusion.Design.cast(app.activeProduct)
    des.designType = adsk.fusion.DesignTypes.ParametricDesignType
    des.fusionUnitsManager.distanceDisplayUnits = adsk.fusion.DistanceUnits.MillimeterDistanceUnits
    root = des.rootComponent
    for n, e, cm in PARAMS:
        des.userParameters.add(n, V(e), 'mm', cm)

    occs = root.occurrences

    def new(name, m=None):
        o = occs.addNewComponent(m or mat())
        o.component.name = name
        return o

    def inst(comp, m):
        return occs.addExistingComponent(comp, m)

    # frame
    mm = new('MotorMount'); build_motor_mount(mm.component); name_body(mm.component)
    pm = new('PivotMount'); build_pivot_mount(pm.component); name_body(pm.component)
    r1 = new('Rail'); build_rail(r1.component); name_body(r1.component)
    r2 = inst(r1.component, mat(0, ev('PivotY'), 0, math.pi))

    # pose: effector at (0, Ym), elbow-out
    D2, Ym, PY = ev('D') / 2, ev('Ym'), ev('PivotY')
    ex, ey = 0.0, Ym
    tL, eLx, eLy = ik(ex, ey, -D2, 0, +1)
    tR, eRx, eRy = ik(ex, ey, D2, 0, -1)
    tPL, pLx, pLy = ik(ex, ey, -D2, PY, -1)
    tPR, pRx, pRy = ik(ex, ey, D2, PY, +1)
    ang = lambda x0, y0, x1, y1: math.atan2(y1 - y0, x1 - x0)
    zjm, zjp = ev('ZJm'), ev('ZJp')

    pl = new('ProximalArm', mat(-D2, 0, 0, tL))
    elbowL_pt = build_prox(pl.component)[0]
    name_body(pl.component)
    pr = inst(pl.component, mat(D2, 0, 0, tR))

    ml = new('DistalArm', mat(eLx, eLy, 2 * zjm, ang(eLx, eLy, ex, ey), flip=True))
    build_distal(ml.component); name_body(ml.component)
    dc = ml.component
    mr = inst(dc, mat(eRx, eRy, 0, ang(eRx, eRy, ex, ey)))
    dpl = inst(dc, mat(pLx, pLy, zjp - zjm, ang(pLx, pLy, ex, ey)))
    dpr = inst(dc, mat(pRx, pRy, zjm + zjp, ang(pRx, pRy, ex, ey), flip=True))

    ppl = new('PassiveProximalArm', mat(-D2, PY, 0, tPL))
    pelbow_pt = build_pprox(ppl.component)[0]
    name_body(ppl.component)
    ppr = inst(ppl.component, mat(D2, PY, 0, tPR))

    hub = new('EffectorHub_SolenoidHolder', mat(ex, ey, 0))
    hub_m, hub_p = build_hub(hub.component)
    name_body(hub.component)

    s1 = new('M5Sleeve', mat(eLx, eLy, 0)); build_m5_sleeve(s1.component); name_body(s1.component)
    s2 = inst(s1.component, mat(eRx, eRy, 0))
    zpin_top = ev('ZProx + ArmT + LayerGap/2 + 4*Pitch + PinPlay')
    zpe_bot = ev('ZD1 + 2*Pitch - LayerGap/2 - PinPlay - CapT')
    c1 = new('PinCap', mat(ex, ey, zpin_top)); build_cap(c1.component); name_body(c1.component)
    c2 = inst(c1.component, mat(pLx, pLy, zpe_bot))
    c3 = inst(c1.component, mat(pRx, pRy, zpe_bot))

    # purchased placeholders
    n1 = new('PURCHASED_NEMA17', mat(-D2, 0, 0)); build_nema(n1.component); name_body(n1.component)
    n2 = inst(n1.component, mat(D2, 0, 0))
    zb = ev('ZProx + ArmT - BrgW')
    b1 = new('PURCHASED_Bearing', mat(eLx, eLy, zb)); build_bearing(b1.component); name_body(b1.component)
    b2 = inst(b1.component, mat(eRx, eRy, zb))
    zt = ev('TowerTop - BrgW')
    b3 = inst(b1.component, mat(-D2, PY, zt))
    b4 = inst(b1.component, mat(D2, PY, zt))
    so = new('PURCHASED_Solenoid_JF0530B', mat(ex, ey, ev('SolBot'))); build_solenoid(so.component); name_body(so.component)
    # 4x M5x30 axles at the bearing joints, head under the bearing inner ring
    a1 = new('PURCHASED_M5x30', mat(eLx, eLy, zb))
    build_screw(a1.component, '5 mm', 'M5L', 'M5HeadD', 'M5HeadH'); name_body(a1.component)
    a2 = inst(a1.component, mat(eRx, eRy, zb))
    a3 = inst(a1.component, mat(-D2, PY, zt))
    a4 = inst(a1.component, mat(D2, PY, zt))
    zn1 = ev('ZProx + ArmT + LayerGap/2 + 2*Pitch')
    zn2 = ev('ZPP + ArmT')
    u1 = new('PURCHASED_M5_Nut', mat(eLx, eLy, zn1)); build_nut(u1.component); name_body(u1.component)
    u2 = inst(u1.component, mat(eRx, eRy, zn1))
    u3 = inst(u1.component, mat(-D2, PY, zn2))
    u4 = inst(u1.component, mat(D2, PY, zn2))
    # M4x9: on the pin caps and in the plate counterbores (shank down = flipped)
    m4 = new('PURCHASED_M4x9', mat(ex, ey, zpin_top + ev('CapT'), flip=True))
    build_screw(m4.component, '4 mm', 'M4L', 'M4HeadD', 'M4HeadH'); name_body(m4.component)
    m4p = [inst(m4.component, mat(x, y, zpe_bot)) for (x, y) in ((pLx, pLy), (pRx, pRy))]
    zf = ev('PlateFlange')
    m4f = []
    for yy in (0.0, PY):
        for sx in (-1, 1):
            for bx in ('BoltX1', 'BoltX2'):
                m4f.append(inst(m4.component, mat(sx * ev(bx), yy, zf, flip=True)))

    for o in [mm, pm, r1, r2, n1, n2, b3, b4] + m4f:
        o.isGrounded = True

    # joints (as-built: keep the computed default pose)
    abj = root.asBuiltJoints

    def rev(name, o1, o2, sp, owner):
        g = adsk.fusion.JointGeometry.createByPoint(sp.createForAssemblyContext(owner))
        ji = abj.createInput(o1, o2, g)
        ji.setAsRevoluteJointMotion(adsk.fusion.JointDirections.ZAxisJointDirection)
        j = abj.add(ji)
        j.name = name
        if name in JOINT_LIMITS:
            lo, hi = JOINT_LIMITS[name]
            lim = j.jointMotion.rotationLimits
            lim.isMinimumValueEnabled = True
            lim.minimumValue = math.radians(lo)
            lim.isMaximumValueEnabled = True
            lim.maximumValue = math.radians(hi)
        return j

    def rigid(name, o1, o2):
        ji = abj.createInput(o1, o2, None)
        ji.setAsRigidJointMotion()
        j = abj.add(ji)
        j.name = name
        return j

    def pts(occ):
        sk = occ.component.sketches.itemByName('JointPts')
        p = [sk.sketchPoints.item(i) for i in range(sk.sketchPoints.count) if sk.sketchPoints.item(i) != sk.originPoint]
        return sorted(p, key=lambda q: q.geometry.x)
    mpts, ppts = pts(mm), pts(pm)

    rev('Rev_MotorL', pl, mm, mpts[0], mm)
    rev('Rev_MotorR', pr, mm, mpts[1], mm)
    rev('Rev_ElbowL', ml, pl, elbowL_pt, pl)
    rev('Rev_ElbowR', mr, pr, elbowL_pt, pr)
    rev('Rev_Effector_ML', ml, hub, hub_m, hub)
    rev('Rev_Effector_MR', mr, hub, hub_m, hub)
    rev('Rev_PivotL', ppl, pm, ppts[0], pm)
    rev('Rev_PivotR', ppr, pm, ppts[1], pm)
    rev('Rev_PElbowL', dpl, ppl, pelbow_pt, ppl)
    rev('Rev_PElbowR', dpr, ppr, pelbow_pt, ppr)
    rev('Rev_Effector_PL', dpl, hub, hub_p, hub)
    rev('Rev_Effector_PR', dpr, hub, hub_p, hub)

    for nm_, o1, o2 in (('BearingL', b1, pl), ('BearingR', b2, pr), ('Solenoid', so, hub),
                        ('SleeveL', s1, ml), ('SleeveR', s2, mr), ('AxleElbowL', a1, ml), ('AxleElbowR', a2, mr),
                        ('NutElbowL', u1, ml), ('NutElbowR', u2, mr),
                        ('AxlePivotL', a3, ppl), ('AxlePivotR', a4, ppr), ('NutPivotL', u3, ppl), ('NutPivotR', u4, ppr),
                        ('CapEffector', c1, hub), ('ScrewEffector', m4, hub),
                        ('CapPElbowL', c2, ppl), ('CapPElbowR', c3, ppr),
                        ('ScrewPElbowL', m4p[0], ppl), ('ScrewPElbowR', m4p[1], ppr)):
        rigid('Rigid_' + nm_, o1, o2)

    # appearances
    def byname(coll, n):
        try:
            return coll.itemByName(n)
        except RuntimeError:
            return None
    lib = None
    for i in range(app.materialLibraries.count):
        l = app.materialLibraries.item(i)
        if byname(l.appearances, 'Plastica - Lucido (giallo)') or byname(l.appearances, 'Plastic - Glossy (Yellow)'):
            lib = l
            break
    if lib:
        def ap(*names):
            for n in names:
                a = byname(des.appearances, n)
                if a:
                    return a
                src = byname(lib.appearances, n)
                if src:
                    return des.appearances.addByCopy(src, n)
        yellow = ap('Plastica - Lucido (giallo)', 'Plastic - Glossy (Yellow)')
        steel = ap('Acciaio - Satinato', 'Steel - Satin')
        black = ap('Plastica - Lucido (nero)', 'Plastic - Glossy (Black)')
        for comp in des.allComponents:
            if comp == root:
                continue
            a = yellow
            if 'NEMA' in comp.name or 'Solenoid' in comp.name:
                a = black
            elif 'PURCHASED' in comp.name:
                a = steel
            for b in comp.bRepBodies:
                if a:
                    b.appearance = a

    print('angles deg L %.1f R %.1f PL %.1f PR %.1f' % tuple(math.degrees(t) for t in (tL, tR, tPL, tPR)))
    print('occurrences', occs.count, 'asbuilt joints', abj.count)
    for i in range(des.timeline.count):
        it = des.timeline.item(i)
        if it.healthState != adsk.fusion.FeatureHealthStates.HealthyFeatureHealthState:
            print('UNHEALTHY', it.name, it.errorOrWarningMessage)


def run(_context):
    try:
        main()
    except Exception:
        adsk.core.Application.get().userInterface.messageBox('Plotter5Bar build failed:' + chr(10) + traceback.format_exc())
