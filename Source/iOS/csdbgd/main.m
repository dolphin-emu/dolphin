#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>

#define PT_ATTACHEXC  14    /* attach to running process with signal exception */
#define PT_DETACH  11    /* stop tracing a process */
extern int ptrace(int a, int b, int c, int d);

CFDataRef callback(CFMessagePortRef port, SInt32 message_id, CFDataRef data, void* info)
{
  // Get the process ID
  int process_id;
  NSData* serialized_data = (__bridge NSData*)data;
  [serialized_data getBytes:&process_id length:sizeof(process_id)];
  
  // Attempt to attach to the process
  int retval = ptrace(PT_ATTACHEXC, process_id, 0, 0);
  if (retval)
  {
    NSLog(@"Unable to attach to %d", process_id);
    return NULL;
  }
  
  // Attempt to detach from the process
  for (int i = 0; i < 100; i++)
  {
    usleep(1000);
    
    retval = ptrace(PT_DETACH, process_id, 0, 0);
    if (retval == 0)
    {
      break;
    }
  }
  
  if (retval != 0)
  {
    NSLog(@"Unable to detach from %d", process_id);
  }
  
  return NULL;
}


int main(int argc, char **argv)
{
  CFStringRef port_name = CFSTR("me.oatmealdome.csdbgd-port");
  CFMessagePortRef port = CFMessagePortCreateLocal(kCFAllocatorDefault, port_name, &callback, NULL, NULL);
  
  if (port == NULL)
  {
    NSLog(@"Failed to create port");
    return -1;
  }
  
  CFMessagePortSetDispatchQueue(port, dispatch_queue_create("me.oatmealdome.dolphinios.csdbgd.queue", NULL));
  
  // Spin here
  while (true)
  {
    usleep(1000000);
  }
  
  return 0;
}
