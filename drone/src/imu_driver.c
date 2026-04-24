#include <microkit.h>
#include <stdint.h>

#include "imu_sample.h"

/*
 * Simulated IMU driver. On real hardware this PD would be granted the
 * I2C/SPI MMIO region and the IMU's IRQ; here we synthesise samples so
 * the pipeline can be exercised under QEMU.
 *
 * NOTE: M2 does not yet have a timer PD, so sampling is paced by the
 * ack channel from the estimator — capped by MCS period/budget in
 * board.system. M2.5 will introduce a periodic tick source.
 */

#define CH_ESTIMATOR 0

/* Populated at load time by the microkit tool (setvar_vaddr in board.system). */
uintptr_t imu_shm;

static uint64_t tick;

static void publish_sample(void)
{
    struct imu_shm *shm = (struct imu_shm *)imu_shm;

    /* Trivial synthetic motion: slow rotation about Z, gravity on accel. */
    struct imu_sample s = {
        .timestamp_ticks = tick,
        .gyro_mrad_s  = { 0,      0,      10  },   /* ~0.57 deg/s yaw */
        .accel_mg     = { 0,      0,      1000 },  /* +1 g on Z */
    };

    uint64_t seq = shm->seq;
    __atomic_store_n(&shm->seq, seq + 1, __ATOMIC_RELAXED);   /* odd: writing */
    __atomic_thread_fence(__ATOMIC_RELEASE);
    shm->ring[seq % IMU_RING_LEN] = s;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    __atomic_store_n(&shm->seq, seq + 2, __ATOMIC_RELEASE);   /* even: done */

    tick++;
}

void init(void)
{
    microkit_dbg_puts("imu_driver: init, publishing first sample\n");
    publish_sample();
    microkit_notify(CH_ESTIMATOR);
}

void notified(microkit_channel ch)
{
    if (ch == CH_ESTIMATOR) {
        publish_sample();
        microkit_notify(CH_ESTIMATOR);
    }
}
