# Architecture

## Trust model

The system is partitioned into Microkit protection domains (PDs). Each PD
runs in its own address space with only the capabilities explicitly granted
to it in `board.system`. The trust model follows from the blast radius of
each PD:

- **Most trusted** — can command motors. Must be small and auditable.
- **Trusted** — handles sensor data or sits between untrusted input and
  trusted output. Must not panic, must validate inputs.
- **Untrusted** — parses attacker-controlled data or runs large libraries.
  Assumed hostile; its only channel out is to a validator.

## Components

| PD              | Trust           | Responsibility                                          |
| --------------- | --------------- | ------------------------------------------------------- |
| `imu_driver`    | trusted         | I2C/SPI to IMU. Timestamp samples, publish to estimator |
| `estimator`     | trusted         | Sensor fusion (EKF / complementary). Outputs attitude   |
| `flight_ctrl`   | **most trusted**| Inner PID loop, motor mixing, hard rate/thrust limits   |
| `geofence`      | **most trusted**| Vetoes setpoints outside position/altitude envelope     |
| `motor_driver`  | trusted         | PWM out to ESCs. No policy — just executes commands     |
| `mission`       | semi-trusted    | Waypoint follower. Output flows through `geofence`      |
| `mavlink_radio` | **untrusted**   | Parses ground-station packets. Treated as hostile       |
| `logger`        | trusted         | Append-only black box. Receive-only from others         |
| `vision`        | **untrusted**   | Camera + obstacle detection. Advisory only              |

## Data flow

```
                       (sensors)
  IMU --> imu_driver ----> estimator ----> flight_ctrl ----> motor_driver --> ESCs
                                              ^                    ^
                                              |                    |
                                           geofence <-- mission    |
                                              ^         ^          |
                                              |         |          |
                                           mavlink_radio           |
                                              |                    |
                                              +--> logger <--------+
                                                      ^            |
                                                      +------------+
                                                     (receive-only)
```

Key property: **there is no edge from `mavlink_radio` (or `vision`) directly
to `motor_driver`**. A compromised radio can inject setpoints, but those
setpoints flow through `geofence` and `flight_ctrl`, both of which enforce
envelope and rate limits. A compromised radio also cannot read the logger.

## Scheduling

Use the seL4 kernel's MCS (Mixed-Criticality Scheduling) configuration so
each PD gets a scheduling context with a bounded budget and period:

| PD              | Period  | Budget  | Notes                       |
| --------------- | ------- | ------- | --------------------------- |
| `imu_driver`    | 1 ms    | 100 us  | Hard real-time              |
| `estimator`     | 1 ms    | 300 us  | Hard real-time              |
| `flight_ctrl`   | 1 ms    | 200 us  | Hard real-time              |
| `motor_driver`  | 1 ms    | 100 us  | Hard real-time              |
| `geofence`      | 20 ms   | 500 us  | Soft                        |
| `mission`       | 100 ms  | 2 ms    | Soft                        |
| `mavlink_radio` | 20 ms   | 1 ms    | Best-effort, budget-capped  |
| `logger`        | 50 ms   | 500 us  | Best-effort                 |
| `vision`        | 33 ms   | 10 ms   | Best-effort                 |

Budgets ensure an untrusted PD cannot monopolise the CPU even if its code
is compromised.
