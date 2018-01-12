using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;

namespace RTCV.NetCore
{

    public enum NetworkSide
    {
        NONE,
        CLIENT,
        SERVER
    }

    public enum NetworkStatus
    {
        DISCONNECTED,
        CONNECTIONLOST,
        CONNECTING,
        CONNECTED,
        LISTENING
    }

    [Serializable()]
    public abstract class NetCoreMessage
    {
        public string Type;
    }

    [Serializable()]
    public class NetCoreSimpleMessage : NetCoreMessage
    {
        public NetCoreSimpleMessage(string _Type)
        {
            Type = _Type.Trim().ToUpper();
        }
    }

    [Serializable()]
    public class NetCoreAdvancedMessage : NetCoreMessage
    {
        public string ReturnedFrom;
        public bool Priority = false;
        public Guid? requestGuid = null;
        public object objectValue = null;

        public NetCoreAdvancedMessage(string _Type)
        {
            Type = _Type.Trim().ToUpper();
        }
    }

    public class NetCoreSpec
    {
        // This is a parameters object that must be passed to netcore in order to establish a link
        // The values here will determine the behavior of the Link

        public NetCoreConnector Connector = null;
        public NetworkSide Side = NetworkSide.NONE;
        public bool AutoReconnect = true;
        public int ClientReconnectDelay = 600;
        public int DefaultBoopMonitoringCounter = 15;

        public bool Loopback = true;
        public string IP = "127.0.0.1";
        public int Port = 42069;

        public int messageReadTimerDelay = 10; //represents how often the messages are read (ms) (15ms = ~66fps)

        public ISynchronizeInvoke syncObject = null;

        #region Events

        public event EventHandler<NetCoreEventArgs> MessageReceived;
        public virtual void OnMessageReceived(NetCoreEventArgs e) => MessageReceived?.Invoke(this, e);

        public event EventHandler ClientConnecting;
        internal virtual void OnClientConnecting(EventArgs e) => ClientConnecting?.Invoke(this, e);

        public event EventHandler ClientConnectingFailed;
        internal virtual void OnClientConnectingFailed(EventArgs e) => ClientConnectingFailed?.Invoke(this, e);

        public event EventHandler ClientConnected;
        internal virtual void OnClientConnected(EventArgs e) => ClientConnected?.Invoke(this, e);

        public event EventHandler ClientDisconnected;
        internal virtual void OnClientDisconnected(EventArgs e) => ClientDisconnected?.Invoke(this, e);

        public event EventHandler ClientConnectionLost;
        internal virtual void OnClientConnectionLost(EventArgs e) => ClientConnectionLost?.Invoke(this, e);

        public event EventHandler ServerListening;
        internal virtual void OnServerListening(EventArgs e) => ServerListening?.Invoke(this, e);

        public event EventHandler ServerConnected;
        internal virtual void OnServerConnected(EventArgs e) => ServerConnected?.Invoke(this, e);

        public event EventHandler ServerDisconnected;
        internal virtual void OnServerDisconnected(EventArgs e) => ServerDisconnected?.Invoke(this, e);

        public event EventHandler ServerConnectionLost;
        internal virtual void OnServerConnectionLost(EventArgs e) => ServerConnectionLost?.Invoke(this, e);

        public event EventHandler SyncedMessageStart;
        internal virtual void OnSyncedMessageStart(EventArgs e) => SyncedMessageStart?.Invoke(this, e);

        public event EventHandler SyncedMessageEnd;
        internal virtual void OnSyncedMessageEnd(EventArgs e) => SyncedMessageEnd?.Invoke(this, e);

        #endregion

    }

    public class ConsoleEx
    {
        public static ConsoleEx singularity
        {
            get
            {
                if (_singularity == null)
                    _singularity = new ConsoleEx();
                return _singularity;
            }
        }
        static ConsoleEx _singularity = null;

        public event EventHandler<NetCoreEventArgs> ConsoleWritten;
        public virtual void OnConsoleWritten(NetCoreEventArgs e) => ConsoleWritten?.Invoke(this, e);

        public static void WriteLine(string message)
        {

            bool ShowBoops = true; // for debugging purposes, put this to true in order to see BOOP commands in the console
            if (!ShowBoops && message.Contains("{BOOP}"))
                return;

            string consoleLine = "[" + DateTime.Now.ToString("hh:mm:ss.ffff") + "] " + message;

            ConsoleEx.singularity.OnConsoleWritten(new NetCoreEventArgs() { message = new NetCoreSimpleMessage(consoleLine) });

      System.Diagnostics.Debug.WriteLine(consoleLine);
      Console.WriteLine(consoleLine);
    }
    }
}
