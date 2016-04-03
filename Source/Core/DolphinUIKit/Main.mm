#include <string>

#include <UIKit/UIKit.h>

#include "Core/Analytics.h"
#include "Core/BootManager.h"
#include "Core/Core.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/ConfigManager.h"
#include "UICommon/UICommon.h"
#include "VideoCommon/VideoBackendBase.h"

static CAEAGLLayer *windowContext = nil;

void* Host_GetRenderHandle()
{
    return windowContext;
}

void Host_ShowVideoConfig(void* parent, const std::string& backend_name, const std::string& config_name)
{
    NSLog(@"Video config: backend %s, config %s", backend_name.c_str(), config_name.c_str());
}

void Host_Message(int Id)
{
    switch (Id) {
        case WM_USER_JOB_DISPATCH:
            dispatch_async(dispatch_get_main_queue(), ^{
                Core::HostDispatchJobs();
            });
            break;
    }
}

void Host_UpdateTitle(const std::string& title)
{
    NSLog(@"Title: %s", title.c_str());
}

void Host_UpdateMainFrame()
{
}

void Host_RequestRenderWindowSize(int width, int height)
{
}

void Host_RequestFullscreen(bool enable_fullscreen)
{
}

void Host_SetStartupDebuggingParameters()
{
}

void Host_NotifyMapLoaded()
{
}

bool Host_UIHasFocus()
{
    return true;
}

bool Host_RendererHasFocus()
{
    return true;
}

bool Host_RendererIsFullscreen()
{
    return false;
}

void Host_ConnectWiimote(int wm_idx, bool connect)
{
}

void Host_SetWiiMoteConnectionState(int _State)
{
}

void Host_RefreshDSPDebuggerWindow()
{
}

void Host_UpdateDisasmDialog()
{
    NSLog(@"update disasm dialog");
}

@interface ViewController : UIViewController

@end

@implementation ViewController

- (void)loadView {
    [super loadView];

    CAEAGLLayer *eaglLayer = [CAEAGLLayer layer];
    eaglLayer.opaque = YES;
    eaglLayer.contentsScale = [[UIScreen mainScreen] scale];
    [self.view.layer addSublayer:eaglLayer];
    windowContext = eaglLayer;

    self.view.backgroundColor = [UIColor whiteColor];
}

- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];

    NSLog(@"starting boot...");

    NSString *documents = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) firstObject];
    NSLog(@"using %@ as user directory", documents);
    UICommon::SetUserDirectory([documents UTF8String]);
    UICommon::CreateDirectories();

    NSLog(@"initializing UI");
    UICommon::Init();

    DolphinAnalytics::Instance()->ReportDolphinStart("uikit");

    NSLog(@"setting interpreter");
    bool interpreter = true; // TODO JIT
    SConfig::GetInstance().iCPUCore = PowerPC::MODE_INTERPRETER;
    //PowerPC::SetMode(interpreter ? PowerPC::MODE_INTERPRETER : PowerPC::MODE_JIT);

    NSLog(@"activating openGL");
    VideoBackendBase::ActivateBackend("OGL");

#if 0
    NSString *path = [documents stringByAppendingPathComponent:@"game.iso"];
    NSLog(@"path to resource is %@", path);
    std::string iso = std::string([path UTF8String]);
#else
    // TODO don't hardcode this
    std::string = "PATH_TO_ISO";
#endif

    NSLog(@"booting game!");
    if (!BootManager::BootCore(iso)) {
        NSLog(@"failed to boot!");
        return;
    }

    NSLog(@"booted.");
}

- (void)viewWillLayoutSubviews {
    windowContext.frame = self.view.layer.bounds;
}

@end


@interface AppDelegate : UIResponder <UIApplicationDelegate>

@property (strong, nonatomic) UIWindow *window;

@end

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    self.window.rootViewController = [[ViewController alloc] init];
    [self.window makeKeyAndVisible];
    
    return YES;
}

@end

int main(int argc, char * argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}
