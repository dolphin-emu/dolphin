using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace RTCV.NetCore
{   
    public class NetCoreConnector
    {
        public NetCoreSpec spec = null;

        internal UDPLink udp = null;
        internal volatile TCPLink tcp = null;
        internal MessageHub hub = null;
        internal ReturnWatch watch = null;

        private System.Timers.Timer shutdownTimer = null;

        public NetworkStatus status
        {
            get
            {
                return tcp?.status ?? NetworkStatus.DISCONNECTED;
            }
        }

        public NetCoreConnector(NetCoreSpec _spec)
        {
            spec = _spec;
            spec.Connector = this;
            Initialize();
        }

        private void Initialize()
        {
            if(spec.Side == NetworkSide.NONE)
            {
                ConsoleEx.WriteLine("Could not initialize connector : Side was not set");
                return;
            }

            try
            {
                hub = new MessageHub(spec);
                udp = new UDPLink(spec);
                tcp = new TCPLink(spec);
                watch = new ReturnWatch(spec);
            }
            catch(Exception ex)
            {
                Kill();
                throw ex;
            }
        }



        public void SendMessage(string message) => SendMessage(new NetCoreSimpleMessage(message));
        public void SendMessage(string message, object value) => SendMessage(new NetCoreAdvancedMessage(message) { objectValue = value });
        public object SendSyncedMessage(string message) { return SendMessage(new NetCoreAdvancedMessage(message), true); }
        public object SendSyncedMessage(string message, object value) { return SendMessage(new NetCoreAdvancedMessage(message) { objectValue = value }, true); }

        private object SendMessage(NetCoreMessage message, bool synced = false)
        {
            return hub?.SendMessage(message, synced);
        }

        public void ReturnValue(object value) => hub.ReturnValue(value);

        public void Stop()
        {
            //We just stop TCP gracefully because it's kind of useless to stop UDP.
            tcp?.StopNetworking();
        }

        public void Kill()
        {
            Stop();

            //Schedule the shutdown timer for a force kill in 1 sec
            shutdownTimer = new System.Timers.Timer();
            shutdownTimer.Interval = 1000;
            shutdownTimer.Elapsed += (o,e) => {
                udp?.Kill();
                tcp?.Kill();
                hub?.Kill();
                watch?.Kill();
                shutdownTimer.Stop();
                shutdownTimer = null;
            };
            shutdownTimer.Start();
        }

    }


}
