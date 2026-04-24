#include <microkit.h>

#define CH_PEER 0

void init(void)
{
    microkit_dbg_puts("ponger: init, waiting for ping\n");
}

void notified(microkit_channel ch)
{
    if (ch == CH_PEER) {
        microkit_dbg_puts("ponger: got ping, sending pong\n");
        microkit_notify(CH_PEER);
    }
}
