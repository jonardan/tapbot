# tapbot

A five-bar XY robot that taps a phone screen. Two NEMA17 motors side by
side each turn a proximal arm; two distal arms join at the tip, which will
carry a JF-0530B solenoid. A mirrored, unpowered five-bar on the opposite
side holds the tip from the other end. The parts are 3D-printed (Fusion
model `Plotter5Bar_M4`).

This repo holds the software: a Buildroot `BR2_EXTERNAL` tree for a
BeagleBone Black with two TMC2209 drivers. It uses the same shield, pins
and drivers as [OpenSCARA](https://github.com/jonardan/OpenSCARA), and the
same driver code (`tmc2209.c`, `tmc2209ctl`).

For now it moves the tip in X/Y only: no solenoid, no endstops.

## Geometry

Seen from above, motors at the bottom, idlers at the top:

```
          idler L  (-25, 200)      idler R  (25, 200)
                  \                    /
                   \  mirrored arms   /
   y=150 +--------------------------------+
         |                                |
         |       work area 200 x 100      |
         |                                |
   y=50  H--------------------------------+
        x=-100                         x=100
                  /                    \
           motor 1 (-25, 0)     motor 2 (25, 0)
              left                  right
```

| What | Value |
|---|---|
| motor axis to motor axis (`base_mm`) | 50 mm |
| proximal arm, motor to elbow (`proximal_mm`) | 85 mm |
| distal arm, elbow to tip (`distal_mm`) | 125 mm |
| idler pivots (`idler_y_mm`) | (±25, 200) mm |
| work area (`area_mm`) | x -100..100, y 50..150 mm |
| home, H (`home_mm`) | (-100, 50): bottom-left corner, next to the left motor |

X/Y are in mm from halfway between the motor axes, +Y away from the
motors. Motor angles are in degrees from the +X axis, counter-clockwise
seen from above. The elbows always point outward: the left one to -X, the
right one to +X. Over the work area the left motor turns 76.6..241.9° and
the right one -61.9..103.4°; at home they are at 237.34° and 93.34°. One
microstep (1/16) moves the tip 0.19 mm at most.

All of it is in [`tapbot.conf`](software/package/tapbot/tapbot.conf),
installed as `/etc/tapbot.conf`.

## Homing without endstops

At power-on the tip must sit in the home corner. The boot script runs
`tapbot sethome`, which takes it to be there and starts counting steps
from that pose (`/run/tapbot.pos`). Run `tapbot home` before switching
off, so the next boot starts from the right place. If the tip is moved by
hand, or a motor skips steps, put it back in the corner and run
`tapbot sethome`.

## Build

From the Buildroot source directory (`~/Desktop/buildroot-2026.05.3`). Use
its own output directory, so the OpenSCARA build in `output/` stays as it
is:

```bash
make O=output-tapbot BR2_EXTERNAL=~/Desktop/tapbot/software tapbot_defconfig   # once
make O=output-tapbot                                                           # output-tapbot/images/sdcard.img
```

After editing only the programs: `make O=output-tapbot tapbot-rebuild all`.

The math (five-bar IK/FK and straight lines) has no hardware code and is
tested on the PC:

```bash
make -C software/package/tapbot/src test
```

Flashing, boot and console are as in OpenSCARA (`readme.txt` there): `dd`
the image to the SD card, boot holding S2, console on `/dev/ttyACM0`.

## Wiring

The OpenSCARA shield as it is: motor 1 (left) on driver 1, motor 2 (right)
on driver 2.

| Signal | BBB pin | Notes |
|---|---|---|
| UART TX / RX | P9_24 (through 1 kΩ) / P9_26 | single-wire bus to PDN_UART of both drivers |
| motor 1 (left) STEP / DIR | P9_16 / P9_18 | driver UART address 0 |
| motor 2 (right) STEP / DIR | P9_12 / P9_30 | driver UART address 2; never P9_28 |

The device tree only muxes these. The old endstop, LED and IR pins are
left free for the solenoid.

## Commands

```
tapbot sethome          the tip is in the home corner now (done at boot)
tapbot where            motor angles and tip X/Y
tapbot move X Y [F]     straight line to X/Y at F mm/s (default 30)
tapbot home [F]         back to the home corner
tapbot corners [F]      round the edge of the work area, then home
tapbot fk A1 A2         tip X/Y for motor angles (no move)
tapbot ik X Y           motor angles for a tip X/Y (no move)
tapbot jog M DEG [RPM]  turn one motor, no checks: bring-up only
```

Every move is planned and checked before anything turns: the tip stays
inside the work area and inside the motor angle limits, the elbows and
the tip stay away from the flat poses where the arms lose stiffness, and
the idler side must be able to follow. Ctrl-C decelerates to a stop.

`tmc2209ctl` works as in OpenSCARA (`status`, `turn`, `enable`, ...), with
`/etc/tapbot.conf`.

## First test on the hardware

1. 12 V off, motors unplugged from the arms if possible. Boot, then
   `tmc2209ctl -m all status`: both drivers must answer.
2. Direction: `tmc2209ctl -m 1 turn 0.05` must turn the left arm
   counter-clockwise seen from above, same for `-m 2`. If one turns the
   other way, set `invert_dir = 1` in its `[motorN]` section.
3. Put the tip in the home corner (-100, 50), run `tapbot sethome`, then
   `tapbot where`: 237.34° and 93.34°.
4. `tapbot move -90 60 10`, then `tapbot move 0 100 10`: short and slow,
   with a hand on the 12 V switch.
5. `tapbot corners 20`, and measure with a ruler that the corners are
   200 x 100 mm apart. `tapbot home` at the end.

Still to check against the Fusion model: the joint rotation limits set
there, against `min_angle` / `max_angle` in `tapbot.conf`.
