#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/net/openthread.h>
#include <openthread/thread.h>
#include <openthread/udp.h>
#define SLEEP_TIME_MS 1000

void udpReceiveCb(void *aContext, otMessage *aMessage,
                  const otMessageInfo *aMessageInfo)
{
    uint16_t payloadLength = otMessageGetLength(aMessage) - otMessageGetOffset(aMessage);
    char buf[payloadLength + 1];
    otMessageRead(aMessage, otMessageGetOffset(aMessage), buf, payloadLength);
    buf[payloadLength] = '\0';
    printk("%s\n", buf);
}
static void udp_init(void)
{
    otError error = OT_ERROR_NONE;
    otInstance *myInstance = openthread_get_default_instance();
    otUdpSocket mySocket;
    otSockAddr mySockAddr;
    memset(&mySockAddr, 0, sizeof(mySockAddr));
    mySockAddr.mPort = 1235;
    do
    {
        error = otUdpOpen(myInstance, &mySocket, udpReceiveCb, NULL);
        if (error != OT_ERROR_NONE)
        {
            break;
        }
        error = otUdpBind(myInstance, &mySocket, &mySockAddr, OT_NETIF_THREAD);
    } while (false);
    if (error != OT_ERROR_NONE)
    {
        printk("init_udp error: %d\n", error);
    }
}

int main(void)
{
    udp_init();
    while (1)
    {
        k_msleep(SLEEP_TIME_MS);
    }
}