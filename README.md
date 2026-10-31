# 👆 tapbot

**A 3D-printed five-bar robot that taps a phone screen.**
Two NEMA 17 steppers, two TMC2209 drivers and a BeagleBone Black move a
tip anywhere over a 200 × 100 mm area, in straight lines, in millimetres.
A solenoid on the tip will do the tapping.

## 🌟 Highlights

- 🖨️ **Mostly 3D printed**: 18 printed parts, all of them fit a Snapmaker A250 bed. The rest is two motors, four bearings and a handful of screws.
- 🦿 **Five-bar, held from both ends**: two motor-driven arms meet at the tip, and a mirrored pair of idler arms holds it from the other side, so the tip does not hang off two arms only.
- 📐 **Thinks in millimetres**: `tapbot move 40 120` and the tip goes there in a straight line. The five-bar kinematics are done for you and tested on the PC.
- 🛡️ **Checks before it moves**: every move is planned point by point and refused if the tip would leave the work area, a motor would pass its limit, an arm would fold flat, or the idler side couldn't follow.
- 🏠 **No endstops**: the tip starts in a known corner at power-on and every step is counted from there.
- 🐧 **One SD-card image**: a small Buildroot Linux for the BeagleBone Black, ready to boot.

## ℹ️ Overview

The two motors sit side by side, 50 mm apart, shafts up. Each one turns a
**proximal arm** (85 mm, motor axis to elbow) directly, with no belt. A
**distal arm** (125 mm, elbow to tip) joins each elbow to the tip, where
both meet on one pin. The elbows always point outward: the left one to
the left, the right one to the right. Turning the two motors places the
tip anywhere in the work area, a bit like a SCARA arm with two shoulders.

On the far side, 200 mm from the motors, two free pivots carry a copy of
the same arms, mirrored. They have no motor and simply follow the tip,
holding it from the other end. The phone lies flat under the arms.

The BeagleBone Black runs everything. It talks to both TMC2209 drivers
over one shared UART line (current, microstepping, quiet stealthChop),
and sends the STEP/DIR pulses itself. On top of that sits `tapbot`, a
small C program that converts X/Y in millimetres to motor angles and
back, plans straight lines and checks them. The board, the shield, the
pins and the driver code are the same as on
[OpenSCARA](https://github.com/jonardan/OpenSCARA).

The solenoid is not wired yet: for now tapbot only moves in X/Y.

### ✍️ Author

tapbot is built by **jo** ([@jonardan](https://github.com/jonardan)).

## 📐 Geometry

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
seen from above. Over the work area the left motor turns 76.6..241.9° and
the right one -61.9..103.4°. At home they are at 237.34° and 93.34°. One
microstep (1/16) moves the tip 0.19 mm at most.

All of it is in [`tapbot.conf`](software/package/tapbot/tapbot.conf),
installed on the board as `/etc/tapbot.conf`.

## 🧰 Bill of materials

| Qty | Part | Where |
|---|---|---|
| 2 | NEMA 17 stepper (from a Tevo Tarantula), 5 mm D-shaft | motors |
| 2 | BIGTREETECH TMC2209 V1.3 | drivers, on the OpenSCARA shield |
| 1 | BeagleBone Black + micro-SD card (≥ 1 GB) | controller |
| 1 | 12 V power supply | motors |
| 4 | ball bearing 5 × 24.5 × 11 mm | motor-side elbows, idler pivots |
| 4 | M5 × 30 screw + 4 M5 nuts | the axles of the four bearings |
| 14 | M4 × 9 screw | frame, pin caps, motor hub and solenoid grub screws |
| 8 | M3 screw (from the Tevo) | NEMA 17 to the motor plate |
| 1 | JF-0530B push-pull solenoid | the tip (not wired yet) |
| 18 | printed parts | below |

## 🖨️ Printed parts

The STLs come from the Fusion design `Plotter5Bar_M4` and are already
oriented for printing: load them in Cura as they are. The `_xN` suffix of
a file name is how many copies to print.

| Folder | Parts |
|---|---|
| `1_Struttura` | motor plate, idler pivot plate, 2 side rails |
| `2_Bracci` | 2 motor arms, 4 distal arms, 2 idler arms; `PIATTO_tutti_i_bracci_8pz.stl` has all 8 on one bed (141 × 213 mm) |
| `3_Effettore` | solenoid holder (tip hub) |
| `4_Perni` | printed pins, M5 sleeves and pin caps |

Cura settings, PLA, 0.4 mm nozzle:

| Setting | Value |
|---|---|
| Layer height | 0.3 mm (first layer 0.3). Max ~75 % of the nozzle: 0.4 mm layers need a 0.6 mm nozzle, then use 3 walls |
| Walls | 4 (~1.6 mm), for strong material around holes and bearings |
| Top / bottom | 4 layers |
| Infill | 40 % gyroid |
| Temperature | nozzle 210-215 °C, bed 60 °C |
| Speed | 50 mm/s, outer wall 30 mm/s |
| Fan | 100 % from layer 3 |
| Adhesion | brim 5 mm |
| Initial Layer Horizontal Expansion | -0.1 mm (against elephant's foot shrinking the holes) |
| Supports | none |

Print the small sleeves and caps at 0.2 mm whatever the nozzle: their
flange is only 0.5 mm thick.

Before printing all the arms, print **one motor arm** and try the bearing
and the motor shaft in it. If they are too tight, set *Hole Horizontal
Expansion* to 0.1 mm in Cura.

## 🔩 Assembly

1. **Frame.** Screw the NEMA 17s to the motor plate with their M3 screws,
   shafts up. Join the motor plate and the idler plate with the two side
   rails, M4 × 9 screwed straight into the plastic. The area under the
   arms stays open for the phone.
2. **Bearings.** Press one bearing into the elbow of each motor arm, and
   one into the top of each tower of the idler plate.
3. **Motor arms.** Slide each motor arm's hub onto its shaft, the flat of
   the D-shaft against the flat of the bore, and lock it with the M4 × 9
   grub screw.
4. **Motor-side elbows.** An M5 × 30 goes through the bearing of the motor
   arm and the eye of a distal arm, closed with an M5 nut. Snug, not
   tight: the arm must swing freely.
5. **Idler arms.** An M5 × 30 through each idler tower bearing carries an
   idler arm, with an M5 nut. The idler elbows turn on a printed pin,
   closed by a printed cap and an M4 × 9.
6. **Tip.** The four distal arms stack on the solenoid holder's pin, at
   their own heights, then a printed cap and an M4 × 9 close it. The
   four distal arms are the same part: two go on upright and two flipped,
   so each reaches its level (the taller eye skips one level). Between
   two levels there is a 1 mm gap.
7. **Check by hand.** With the motors unpowered, move the tip around the
   whole area: nothing may rub or catch, and the elbows must stay
   outward, the left one to the left, the right one to the right.

Height levels from the bottom, as modelled: motor arms, then the two
motor-side distal arms, then the two idler-side distal arms, then the
idler arms on top, so the idler side mirrors the motor side and the tip is
held from below and above. At rest the solenoid tip sits about 3 mm above
a 10 mm thick phone lying on the table.

## 🔌 Wiring

The OpenSCARA shield as it is: the left motor on driver 1, the right
motor on driver 2. The full wiring guide, with the drivers' pins, power,
capacitors and grounds, is in OpenSCARA's
[`software/README.md`](https://github.com/jonardan/OpenSCARA/blob/main/software/README.md).

| Signal | BBB pin | Notes |
|---|---|---|
| UART TX / RX | P9_24 (through 1 kΩ) / P9_26 | one single-wire bus to PDN_UART of both drivers |
| motor 1 (left) STEP / DIR | P9_16 / P9_18 | driver UART address 0 (MS1, MS2 to GND) |
| motor 2 (right) STEP / DIR | P9_12 / P9_30 | driver UART address 2 (MS2 to 3.3 V); never P9_28 |
| driver VIO / logic GND | P9_3 (3.3 V) / P9_2 | 3.3 V only |
| driver VM / GND | 12 V supply | 100 µF ≥ 25 V across VM-GND at each driver |

The device tree only muxes these pins. The endstop, LED and IR pins of
OpenSCARA are left free for the solenoid.

> **Power everything off while wiring.** Never plug or unplug a motor
> while 12 V is on: it destroys the driver.

## 🐧 Build the SD-card image

tapbot is a Buildroot `BR2_EXTERNAL` tree. Build it from the Buildroot
source directory (`~/Desktop/buildroot-2026.05.3`), in its own output
directory so the OpenSCARA build in `output/` stays as it is:

```bash
make O=output-tapbot BR2_EXTERNAL=~/Desktop/tapbot/software tapbot_defconfig   # once
make O=output-tapbot                                                           # output-tapbot/images/sdcard.img
```

A first build needs several GB of disk. After editing only the programs:

```bash
make O=output-tapbot tapbot-rebuild all
```

## 💾 Flash and boot

```bash
lsblk                                   # find the SD card, e.g. /dev/sdb
sudo dd if=output-tapbot/images/sdcard.img of=/dev/sdX bs=4M conv=fsync status=progress
```

1. Put the card in the unpowered BeagleBone.
2. Put the tip in the home corner (see below).
3. Hold the S2 (BOOT) button, plug in the mini-USB cable, and let go of S2
   when the LEDs start blinking.
4. Switch on the 12 V supply.

The mini-USB port is also the console, `/dev/ttyACM0` on the PC:

```bash
picocom -b 115200 /dev/ttyACM0          # login: root, no password
```

## 🏠 Homing without endstops

At power-on the tip must sit in the home corner: the bottom-left corner of
the work area, next to the left motor (-100, 50). At boot the
`S95tapbot` script configures the drivers and runs `tapbot sethome`, which
takes the tip to be there and counts every step from that pose
(`/run/tapbot.pos`).

- Run `tapbot home` before switching off, so the next boot starts from the
  right place.
- If the 12 V was off at boot, run `tmc2209ctl -m all init` and
  `tapbot sethome` once it is on.
- If the tip was moved by hand or a motor skipped steps, put it back in
  the corner and run `tapbot sethome`.

## 🚀 Usage

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

For example:

```bash
tapbot move 0 100        # middle of the area
tapbot move 80 140 50    # top right, at 50 mm/s
tapbot home
```

Every move is planned in 1 mm steps and checked before anything turns.
Ctrl-C decelerates to a stop. `tmc2209ctl` works as in OpenSCARA
(`status`, `turn`, `enable`, `disable`, ...), with `/etc/tapbot.conf`.

## 🧪 First test on the hardware

1. 12 V off. Boot, switch the 12 V on, then `tmc2209ctl -m all status`:
   both drivers must answer.
2. **Direction.** `tmc2209ctl -m 1 turn 0.05` must turn the left arm
   counter-clockwise seen from above; the same for `-m 2`. If one turns
   the other way, set `invert_dir = 1` in its `[motorN]` section of
   `/etc/tapbot.conf`.
3. Put the tip in the home corner, run `tapbot sethome`, then
   `tapbot where`: 237.34° and 93.34°.
4. `tapbot move -90 60 10`, then `tapbot move 0 100 10`: short and slow,
   with a hand on the 12 V switch.
5. `tapbot corners 20`, and check with a ruler that the corners are
   200 × 100 mm apart. `tapbot home` at the end.

Still to check against the Fusion model: its joint rotation limits,
against `min_angle` / `max_angle` in `tapbot.conf` (60..260° left,
-80..120° right).

## 🧮 How the kinematics work

[`fivebar.c`](software/package/tapbot/src/fivebar.c) has no hardware code.

- **Inverse** (X/Y to motor angles), one arm at a time: with *d* the
  distance from the motor axis to the tip and φ its direction,
  cos α = (l1² + d² − l2²) / (2 · l1 · d), and the arm angle is φ + α for
  the left motor and φ − α for the right one, so the elbows point
  outward.
- **Forward** (angles to X/Y): the tip is where the two circles of
  radius l2 around the elbows cross, on the side away from the motors.
- **Checks**: the tip must be inside the work area and the motors inside
  their limits. Each elbow must stay 10° away from folded or straight, and
  the distal arms must meet at the tip at 25..155°. The mirrored idler
  side must pass the same checks.

[`path.c`](software/package/tapbot/src/path.c) splits a straight line into
1 mm segments with IK at each point. The driver code streams them in one
stepping loop that only accelerates at the ends.

Both are tested on the PC, over every point of the area 1 mm apart, and
on every line between its corners:

```bash
make -C software/package/tapbot/src test
```

## 📁 Repository layout

| Path | What it is |
|---|---|
| [`software/configs/tapbot_defconfig`](software/configs/tapbot_defconfig) | the Buildroot configuration |
| [`software/package/tapbot/`](software/package/tapbot/) | `tapbot` and `tmc2209ctl` (C), `tapbot.conf`, boot script |
| [`software/board/beaglebone/`](software/board/beaglebone/) | device tree, boot files, image layout, USB console |
