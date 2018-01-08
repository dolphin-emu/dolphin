using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Timers;

namespace RTCV.NetCore
{
    public class MessageHub
    {
        private NetCoreSpec spec;

        private Timer hubTimer;

        private object MessagePoolLock = new object();
        private LinkedList<NetCoreMessage> MessagePool = new LinkedList<NetCoreMessage>();
        

        private volatile NetCoreAdvancedMessage returnAdvancedMessage = null;

        internal MessageHub(NetCoreSpec _spec)
        {
            spec = _spec;
            hubTimer = new Timer();
            hubTimer.Interval = spec.messageReadTimerDelay;
            hubTimer.Elapsed += CheckMessages;
            hubTimer.Start();
        }

        private void CheckMessages(object sender, System.Timers.ElapsedEventArgs e)
        {
            while (true) //checks all messages
            {
                NetCoreMessage message;

                lock (MessagePoolLock)
                {
                    if (MessagePool.Count == 0) //Stop checking messages once the pool is empty
                        return;

                    message = MessagePool.First.Value;
                    MessagePool.RemoveFirst();
                }

                if (message is NetCoreAdvancedMessage || (message.Type.Length > 0 && message.Type[0] == '{'))
                    if (spec.Connector.tcp.ProcessAdvancedMessage(message))
                        continue;   //If this message was processed internally, don't send further

                var ea = new NetCoreEventArgs();
                ea.message = message;

                try
                {
                    spec.OnMessageReceived(ea); //Send message to Client Implementation
                }
                catch (Exception ex)
                {
                    //We don't want to leave the scope here because if the code after the OnMessageReceive() call isn't
                    //executed, the other side could be left hanging forever if it's waiting for synced message response.

                    ConsoleEx.WriteLine(ex.ToString());
                }

                if (message is NetCoreAdvancedMessage)
                {
                    // Create return Message if a synced request was issued but no Return Advanced Message was set
                    if (returnAdvancedMessage == null && (message as NetCoreAdvancedMessage).requestGuid != null)
                        returnAdvancedMessage = new NetCoreAdvancedMessage("{RETURNVALUE}");

                    //send command back or return value if from bizhawk to bizhawk
                    if (returnAdvancedMessage != null)
                    {
                        returnAdvancedMessage.ReturnedFrom = message.Type;
                        returnAdvancedMessage.requestGuid = (message as NetCoreAdvancedMessage).requestGuid;
                        SendMessage(returnAdvancedMessage);
                    }

                    returnAdvancedMessage = null;
                }
            }
        }

        internal void QueueMessage(NetCoreMessage message, bool priority = false)
        {
            lock (MessagePoolLock)
            {
                if (message.Type.Length > 0 && message.Type[0] == '{' && MessagePool.FirstOrDefault(it => it.Type == message.Type) != null) //Prevents doubling of internal messages
                    return;

                if (priority) //This makes sure a prioritized message is the very next to be treated
                    MessagePool.AddFirst(message);
                else
                    MessagePool.AddLast(message);
            }
        }

        internal object SendMessage(string message) => SendMessage(new NetCoreSimpleMessage(message));
        internal object SendMessage(NetCoreMessage message, bool synced = false)
        {
            if (message is NetCoreSimpleMessage)
                spec.Connector.udp.SendMessage((NetCoreSimpleMessage)message);
            else //message is NetCoreAdvancedMessage
            {
                if(synced)
                    return spec.Connector.tcp.SendSyncedMessage((NetCoreAdvancedMessage)message); //This will block the sender's thread until a response is received
                else
                    spec.Connector.tcp.SendMessage((NetCoreAdvancedMessage)message);    //This sends the message async
            }

            return null;
        }

        public void ReturnValue(object _objectValue = null)
        {
            var message = new NetCoreAdvancedMessage("{RETURNVALUE}") { objectValue = _objectValue };

            if (spec.Connector.hub.returnAdvancedMessage != null)
                ConsoleEx.WriteLine($"ReturnValue was already set but was overriden with another value");

            spec.Connector.hub.returnAdvancedMessage = message;
        }

        internal void Kill()
        {
            if (hubTimer != null)
                hubTimer.Stop();

        }

    }

    public class NetCoreEventArgs : EventArgs
    {
        public NetCoreMessage message = null;
    }
    
}


