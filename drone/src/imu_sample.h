#pragma once

#include <stdint.h>

/*
 * Shared-memory layout for IMU samples published by imu_driver and
 * consumed by estimator.
 *
 * Concurrency: single-writer / multi-reader seqlock. The writer
 * increments `seq` to an odd value before touching the ring slot, and
 * to an even value after. Readers retry if they observe an odd seq or
 * if seq changed across the read.
 */

#define IMU_RING_LEN 16u

struct imu_sample {
    uint64_t timestamp_ticks;   /* monotonic sample counter */
    int32_t  gyro_mrad_s[3];    /* milli-rad/s  (x, y, z) */
    int32_t  accel_mg[3];       /* milli-g      (x, y, z) */
};

struct imu_shm {
    volatile uint64_t seq;
    uint64_t          _pad;     /* keep ring 16-byte aligned */
    struct imu_sample ring[IMU_RING_LEN];
};

_Static_assert(sizeof(struct imu_shm) <= 0x1000,
               "imu_shm must fit in one 4K page");
