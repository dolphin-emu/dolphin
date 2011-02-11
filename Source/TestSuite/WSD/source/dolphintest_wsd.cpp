// SD card insertion/removal callback demo

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gccore.h>
#include <wiiuse/wpad.h>


#define IOCTL_SDIO_SENDCMD      0x07

#define SDIO_CMD_REGINTR      0x40
#define SDIO_CMD_UNREGINTR      0x41

#define SD_INSERT_EVENT        0x01
#define SD_REMOVE_EVENT        0x02

static struct _sdiorequest
{
  u32 cmd;
  u32 cmd_type;
  u32 rsp_type;
  u32 arg;
  u32 blk_cnt;
  u32 blk_size;
  void *dma_addr;
  u32 isdma;
  u32 pad0;
} sd_req;

static struct _sdioresponse
{
  u32 rsp_fields[3];
  u32 acmd12_response;
} sd_resp;

static s32 __sd0_fd = -1;
static const char _sd0_fs[] ATTRIBUTE_ALIGN(32) = "/dev/sdio/slot0";

static s32 __sdio_getinterrupt(u32 hook, ipccallback cb)
{
  memset(&sd_req, 0, sizeof(sd_req));

  sd_req.cmd = SDIO_CMD_REGINTR;
  sd_req.arg = hook;

  if (hook==SD_INSERT_EVENT)
    printf("Requesting insert event %p %p\n", cb, &sd_resp);
  else if (hook==SD_REMOVE_EVENT)
    printf("Requesting removal event %p %p\n", cb, &sd_resp);
  else
    printf("I don't know what I'm requesting here: hook=%u\n", hook);

  if (__sd0_fd<0)
    __sd0_fd = IOS_Open(_sd0_fs,1);

  if (__sd0_fd>=0)
    return IOS_IoctlAsync(__sd0_fd,IOCTL_SDIO_SENDCMD,&sd_req,sizeof(sd_req),&sd_resp,sizeof(sd_resp),cb,NULL);

  // else return the IOS_Open error code
  return __sd0_fd;
}

static s32 __sdio_releaseinterrupt(void)
{
  if (__sd0_fd<0)
    return 0;

  // this command makes the IOS_IoctlAsync call above return a response immediately

  // sd_req.arg is already set to the right value (1 or 2, whatever was used last)
  sd_req.cmd = SDIO_CMD_UNREGINTR;
  return IOS_Ioctl(__sd0_fd,IOCTL_SDIO_SENDCMD,&sd_req,sizeof(sd_req),&sd_resp,sizeof(sd_resp));
}

static void sd_cb(u32 ret, void* unused)
{
  printf("Got an SD interrupt, ret = %08X\n", ret);
  printf("Response data: %08X %08X %08X %08X\n", sd_resp.rsp_fields[0], sd_resp.rsp_fields[1], sd_resp.rsp_fields[2], sd_resp.acmd12_response);

  if (ret==SD_INSERT_EVENT)
  {
    printf("SD card was inserted\n");
    // tell us when it gets removed
    __sdio_getinterrupt(SD_REMOVE_EVENT, (ipccallback)sd_cb);
  }
  else if (ret==SD_REMOVE_EVENT)
  {
    printf("SD card was removed\n");
    // tell us when something is inserted
    __sdio_getinterrupt(SD_INSERT_EVENT, (ipccallback)sd_cb);
  }
  else
    printf("Unknown SD int: %08X\n", ret);
}



static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

int main(int argc, char **argv) {
  VIDEO_Init();
  WPAD_Init();
  rmode = VIDEO_GetPreferredMode(NULL);
  xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
  console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
  VIDEO_Configure(rmode);
  VIDEO_SetNextFramebuffer(xfb);
  VIDEO_SetBlack(FALSE);
  VIDEO_Flush();
  VIDEO_WaitVSync();
  if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

  printf("\x1b[2;0H");
  printf("SD Card insertion/removal demo\n");
  printf("Press HOME at any time to exit\n");

  __sdio_getinterrupt(SD_INSERT_EVENT, (ipccallback)sd_cb);

  while(1) {
    WPAD_ScanPads();
    u32 pressed = WPAD_ButtonsDown(0);
    if ( pressed & WPAD_BUTTON_HOME ) break;
    VIDEO_WaitVSync();
  }

  printf("SD event release returned %d\n", __sdio_releaseinterrupt());
  IOS_Close(__sd0_fd);

  sleep(4);

  return 0;
}
