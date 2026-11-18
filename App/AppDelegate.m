#import "AppDelegate.h"
#import "GameViewController.h"
#include "../Render/frontend.h"

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    self.window.backgroundColor = [UIColor blackColor];
    self.window.rootViewController = [[GameViewController alloc] init];
    [self.window makeKeyAndVisible];
    [UIApplication sharedApplication].idleTimerDisabled = YES;
    return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application {
    fe_background();
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
    fe_background();
}

@end
