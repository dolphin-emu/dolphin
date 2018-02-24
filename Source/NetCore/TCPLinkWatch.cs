using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;

namespace RTCV.NetCore
{
    public class TCPLinkWatch
    {
        private System.Timers.Timer watchdog = null;
        private object watchLock = new object();

        internal TCPLinkWatch(TCPLink tcp, NetCoreSpec spec)
        {
            watchdog = new System.Timers.Timer();
            watchdog.Interval = spec.ClientReconnectDelay;
            watchdog.Elapsed += (o, e) =>
            {
                lock (watchLock)
                {
                    if ((tcp.status == NetworkStatus.DISCONNECTED || tcp.status == NetworkStatus.CONNECTIONLOST))
                    {
                        tcp.StopNetworking(false);
                        tcp.StartNetworking();
                    }
                }
            };

            tcp.StartNetworking();
            watchdog.Start();

        }

        internal void Kill()
        {
            watchdog?.Stop();
            watchdog = null;
        }

    }
}
