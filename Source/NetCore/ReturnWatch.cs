using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;

namespace RTCV.NetCore
{
    public class ReturnWatch
    {
        //This is a component that allows to freeze the thread that asked for a value from a Synced Message
        //This makes inter-process calls able to block and wait for return values to keep code linearity

        private volatile NetCoreSpec spec;
        private volatile Dictionary<Guid, object> SyncReturns = new Dictionary<Guid, object>();
        private volatile int attemptsAtReading = 0;
        private volatile bool KillReturnWatch = false;

        public bool IsWaitingForReturn
        {
            get { return attemptsAtReading > 0; }
        }

        internal ReturnWatch(NetCoreSpec _spec)
        {
            spec = _spec;
        }

        public void Kill()
        {
            SyncReturns.Clear();

            if (IsWaitingForReturn)
                KillReturnWatch = true;
        }

        public void AddReturn(NetCoreAdvancedMessage message)
        {
            SyncReturns.Add((Guid)message.requestGuid, message.objectValue);
        }

        internal object GetValue(Guid WatchedGuid, string type)
        {
            //Jams the current thread until the value is returned or the KillReturnWatch flag is set to true

            ConsoleEx.WriteLine("GetValue:Awaiting -> " + type.ToString());
            //spec.OnSyncedMessageStart(null);
            spec.Connector.hub.QueueMessage(new NetCoreAdvancedMessage("{EVENT_SYNCEDMESSAGESTART}"));

            attemptsAtReading = 0;

            while (!SyncReturns.ContainsKey(WatchedGuid))
            {
                attemptsAtReading++;

                if (attemptsAtReading % 100 == 0)
                    System.Windows.Forms.Application.DoEvents(); //prevents the forms to freeze (not responding)

                if (KillReturnWatch)
                {
                    //Stops waiting and returns null
                    KillReturnWatch = false;
                    attemptsAtReading = 0;

                    ConsoleEx.WriteLine("GetValue:Killed -> " + type.ToString());
                    //spec.OnSyncedMessageEnd(null);
                    spec.Connector.hub.QueueMessage(new NetCoreAdvancedMessage("{EVENT_SYNCEDMESSAGEEND}"));
                    return null;
                }

                Thread.Sleep(spec.messageReadTimerDelay);
            }

            attemptsAtReading = 0;

            object ret = SyncReturns[WatchedGuid];
            SyncReturns.Remove(WatchedGuid);

            ConsoleEx.WriteLine("GetValue:Returned -> " + type.ToString());
            //spec.OnSyncedMessageEnd(null);
            spec.Connector.hub.QueueMessage(new NetCoreAdvancedMessage("{EVENT_SYNCEDMESSAGEEND}"));
            return ret;
        }

    }


}
