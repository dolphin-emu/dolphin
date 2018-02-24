using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Windows.Forms;


namespace RTCV.NetCore
{

    public class UDPLink
    {
        private NetCoreSpec spec;
        private string IP { get { return spec.IP; } }
        private int PortServer{ get { return spec.Port; } }
        private int PortClient{get{return spec.Port + (spec.Loopback ? 1 : 0 );}} //If running on loopback, will use port+1 for client

        private Thread ReaderThread;
        private UdpClient Sender = null;
        private static volatile bool Running = false;

        internal UDPLink(NetCoreSpec _spec)
        {
            spec = _spec;

            int port = (spec.Side == NetworkSide.SERVER ? PortServer : PortClient);
            Sender = new UdpClient(IP, port);
            ConsoleEx.WriteLine($"UDP Client sending at {IP}:{port}");
            ReaderThread = new Thread(new ThreadStart(ListenToReader));
            ReaderThread.IsBackground = true;
            ReaderThread.Name = "UDP READER";
            ReaderThread.Start();
        }

        internal void Stop()
        {
            Running = false;
        }

        internal void Kill()
        {
            Stop();

            try { ReaderThread.Abort(); } catch { }
            try { Sender.Close(); } catch { }
        }

        internal void SendMessage(NetCoreSimpleMessage message)
        {
            if(Running)
            {
                Byte[] sdata = Encoding.ASCII.GetBytes(message.Type);
                Sender.Send(sdata, sdata.Length);
                ConsoleEx.WriteLine($"UDP : Sent simple message \"{message.Type}\"");
            }
        }

        private void ListenToReader()
        {
            do
            {

                bool done = false;
                int port = (spec.Side == NetworkSide.SERVER ? PortClient : PortServer);

                UdpClient Listener;
                IPEndPoint groupEP;

                try
                {
                    Listener = new UdpClient(port);
                    groupEP = new IPEndPoint((IP == "127.0.0.1" ? IPAddress.Loopback : IPAddress.Parse(IP)), port);
                }
                catch (Exception ex)
                {
                    ConsoleEx.WriteLine(ex.ToString());
                    return;
                }

                try
                {
                    Running = true;
                    ConsoleEx.WriteLine($"UDP Server listening on Port {port}");

                    while (!done)
                    {
                        if (!Running)
                            break;

                        byte[] bytes = Listener.Receive(ref groupEP);

                        spec.Connector.hub.QueueMessage(new NetCoreSimpleMessage(Encoding.ASCII.GetString(bytes, 0, bytes.Length)));
                    }

                }
                catch (Exception e)
                {
                    ConsoleEx.WriteLine(e.ToString());
                }
                finally
                {
                    Listener.Close();
                }

            }
            while (Running); //Reopens the UDP Link if closed unexpectedly
        }

    }
}
