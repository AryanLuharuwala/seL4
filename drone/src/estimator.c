#include <microkit.h>
#include <stdint.h>

#include "imu_sample.h"

/*
 * Attitude estimator. Reads IMU samples from the shared ring via a
 * seqlock retry loop and runs an integer complementary filter on roll
 * and pitch. The filter is intentionally minimal — the point in M2 is
 * to prove the pipeline and the shared-memory protocol, not to fly.
 *
 * Angles are kept in milli-radians to avoid pulling in a soft-float
 * dependency; dt is taken as a fixed 1 tick (to be replaced by a
 * real timer delta in M2.5).
 */

#define CH_IMU 0

/* Populated at load time by the microkit tool. */
uintptr_t imu_shm;

/* Milli-radians, integrated. */
static int32_t roll_mrad;
static int32_t pitch_mrad;
static uint64_t last_seen;

/* Complementary filter weight (out of 1024): 98% gyro, 2% accel tilt. */
#define ALPHA_NUM   1004
#define ALPHA_DEN   1024

static int read_latest(struct imu_sample *out)
{
    struct imu_shm *shm = (struct imu_shm *)imu_shm;

    for (int attempt = 0; attempt < 4; attempt++) {
        uint64_t s1 = __atomic_load_n(&shm->seq, __ATOMIC_ACQUIRE);
        if (s1 == 0 || (s1 & 1)) {
            continue;                       /* pre-init or mid-write */
        }
        struct imu_sample snap = shm->ring[(s1 - 2) % IMU_RING_LEN];
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        uint64_t s2 = __atomic_load_n(&shm->seq, __ATOMIC_ACQUIRE);
        if (s1 == s2) {
            *out = snap;
            return 1;
        }
    }
    return 0;
}

static void update(const struct imu_sample *s)
{
    /* Gyro integration: angle += omega * dt. dt is 1 tick for now,
     * so we just accumulate milli-rad/s as milli-rad per tick. */
    int32_t roll_gyro  = roll_mrad  + s->gyro_mrad_s[0];
    int32_t pitch_gyro = pitch_mrad + s->gyro_mrad_s[1];

    /* Accel tilt: tiny-angle approximation using normalised components.
     * accel in milli-g; for |a| ~ 1000 mg, roll ~ ax / az (rad). */
    int32_t az = s->accel_mg[2] == 0 ? 1 : s->accel_mg[2];
    int32_t roll_acc  = (s->accel_mg[0] * 1000) / az;   /* milli-rad */
    int32_t pitch_acc = (s->accel_mg[1] * 1000) / az;

    roll_mrad  = (int32_t)(((int64_t)roll_gyro  * ALPHA_NUM +
                            (int64_t)roll_acc  * (ALPHA_DEN - ALPHA_NUM)) / ALPHA_DEN);
    pitch_mrad = (int32_t)(((int64_t)pitch_gyro * ALPHA_NUM +
                            (int64_t)pitch_acc * (ALPHA_DEN - ALPHA_NUM)) / ALPHA_DEN);
}

static void print_attitude(uint64_t tick)
{
    /* Avoid dragging in printf; small hex dump every 1024 samples. */
    if ((tick & 0x3ff) != 0) {
        return;
    }

    char buf[64];
    const char *hex = "0123456789abcdef";
    int i = 0;
    buf[i++] = 'e'; buf[i++] = 's'; buf[i++] = 't'; buf[i++] = ':';
    buf[i++] = ' '; buf[i++] = 'r'; buf[i++] = '=';
    uint32_t r = (uint32_t)roll_mrad;
    for (int n = 7; n >= 0; n--) buf[i++] = hex[(r >> (n*4)) & 0xf];
    buf[i++] = ' '; buf[i++] = 'p'; buf[i++] = '=';
    uint32_t p = (uint32_t)pitch_mrad;
    for (int n = 7; n >= 0; n--) buf[i++] = hex[(p >> (n*4)) & 0xf];
    buf[i++] = '\n';
    buf[i] = '\0';
    microkit_dbg_puts(buf);
}

void init(void)
{
    microkit_dbg_puts("estimator: init, awaiting IMU samples\n");
}

void notified(microkit_channel ch)
{
    if (ch != CH_IMU) {
        return;
    }

    struct imu_sample s;
    if (!read_latest(&s)) {
        return;                             /* writer was racing; try next tick */
    }
    if (s.timestamp_ticks == last_seen) {
        microkit_notify(CH_IMU);            /* no new data; ack anyway */
        return;
    }
    last_seen = s.timestamp_ticks;

    update(&s);
    print_attitude(s.timestamp_ticks);
    microkit_notify(CH_IMU);
}
