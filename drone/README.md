# sel4-drone

A secure drone flight-control system built on the seL4 microkernel using
[seL4 Microkit](https://github.com/seL4/microkit). The project sits alongside
the kernel source in this repository; the kernel itself is the parent
`seL4/` tree.

## Goal

A quadcopter stack where the radio link and mission planner are treated as
untrusted: a compromised ground station or a malformed MAVLink packet cannot
command the motors directly. Motor output is gated by a small, trusted
flight controller and an independent geofence/envelope checker.

See `docs/architecture.md` for the component diagram and trust boundaries.

## Milestones

- [x] M1: two-PD ping/pong on QEMU AArch64 (this commit).
- [ ] M2: read IMU samples from a driver PD at 1 kHz under MCS budgets.
- [ ] M3: PWM output to a single motor on real hardware.
- [ ] M4: closed-loop attitude hold on a single-axis rig.
- [ ] M5: add `mavlink_radio` + `geofence` with enforced trust boundary.
- [ ] M6: tethered full-quad flight.

## Prerequisites

1. **AArch64 bare-metal toolchain.** On Debian/Ubuntu:
   ```
   sudo apt install gcc-aarch64-linux-gnu qemu-system-arm
   ```
   (The Makefile uses `aarch64-none-elf-` by default; override with
   `TOOLCHAIN=aarch64-linux-gnu` if you installed the Linux-targeted one.)

2. **seL4 Microkit SDK.** Download a release from
   <https://github.com/seL4/microkit/releases> and extract it somewhere,
   then export:
   ```
   export MICROKIT_SDK=/path/to/microkit-sdk-x.y.z
   ```

## Build and run (M1)

```
cd drone
make BOARD=qemu_virt_aarch64
make qemu
```

Expected output (ping and pong alternating over the serial console):

```
pinger: init, sending first ping
ponger: got ping, sending pong
pinger: got pong, sending ping
...
```

Press `Ctrl-A X` to exit QEMU.

## Switching to real hardware

Microkit supports several boards out of the box; for the drone target
use one of:

- `odroidc4` — well-supported, has hardware PWM, recommended for M3+.
- `rpi4b_1gb` — check your SDK version; cheaper, more GPIO add-ons.
- `maaxboard` — i.MX8, good if you already own one.

Rebuild with `make BOARD=odroidc4` and flash the resulting `build/drone.img`
per the Microkit board docs.
