#include <microkit.h>

#define CH_PEER 0

void init(void)
{
    microkit_dbg_puts("pinger: init, sending first ping\n");
    microkit_notify(CH_PEER);
}

void notified(microkit_channel ch)
{
    if (ch == CH_PEER) {
        microkit_dbg_puts("pinger: got pong, sending ping\n");
        microkit_notify(CH_PEER);
    }
}
