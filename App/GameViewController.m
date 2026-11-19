#import "GameViewController.h"
#import <QuartzCore/QuartzCore.h>
#import <GameController/GameController.h>
#include "../Render/frontend.h"

/* MFi-controller (Bluetooth of bedraad): elke frame uitlezen en als Xbox-indeling doorgeven */
static int g_pauseFrames;

static void poll_controller(void) {
    NSArray *cs = [GCController controllers];
    GCController *c = cs.count > 0 ? cs[0] : nil;
    float ax[6] = {0, 0, 0, 0, 0, 0};
    unsigned b = 0;
    if (c) {
        if (!c.controllerPausedHandler) c.controllerPausedHandler = ^(GCController *ctrl) { (void)ctrl; g_pauseFrames = 3; };
        if (c.extendedGamepad) {
            GCExtendedGamepad *e = c.extendedGamepad;
            ax[0] = e.leftThumbstick.xAxis.value; ax[1] = -e.leftThumbstick.yAxis.value;
            ax[2] = e.rightThumbstick.xAxis.value; ax[3] = -e.rightThumbstick.yAxis.value;
            ax[4] = e.leftTrigger.value; ax[5] = e.rightTrigger.value;
            if (e.buttonA.pressed) b |= 1u << PB_A;
            if (e.buttonB.pressed) b |= 1u << PB_B;
            if (e.buttonX.pressed) b |= 1u << PB_X;
            if (e.buttonY.pressed) b |= 1u << PB_Y;
            if (e.leftShoulder.pressed) b |= 1u << PB_LB;
            if (e.rightShoulder.pressed) b |= 1u << PB_RB;
            if (e.dpad.up.pressed) b |= 1u << PB_UP;
            if (e.dpad.down.pressed) b |= 1u << PB_DOWN;
            if (e.dpad.left.pressed) b |= 1u << PB_LEFT;
            if (e.dpad.right.pressed) b |= 1u << PB_RIGHT;
        } else if (c.gamepad) {
            GCGamepad *gp = c.gamepad;
            ax[0] = gp.dpad.xAxis.value; ax[1] = -gp.dpad.yAxis.value;
            if (gp.buttonA.pressed) b |= 1u << PB_A;
            if (gp.buttonB.pressed) b |= 1u << PB_B;
            if (gp.buttonX.pressed) b |= 1u << PB_X;
            if (gp.buttonY.pressed) b |= 1u << PB_Y;
            if (gp.leftShoulder.pressed) b |= 1u << PB_LB;
            if (gp.rightShoulder.pressed) b |= 1u << PB_RB;
        }
    }
    if (g_pauseFrames > 0) { g_pauseFrames--; if (g_pauseFrames >= 1) b |= 1u << PB_START; }
    fe_pad_input(c != nil, ax, b, c && c.vendorName ? [c.vendorName UTF8String] : "MFi controller");
}

@interface GameViewController () {
    EAGLContext *_ctx;
    BOOL _started;
}
@end

@implementation GameViewController

- (void)loadView {
    GLKView *v = [[GLKView alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    self.view = v;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    _ctx = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    GLKView *v = (GLKView *)self.view;
    v.context = _ctx;
    v.drawableColorFormat = GLKViewDrawableColorFormatRGBA8888;
    v.drawableDepthFormat = GLKViewDrawableDepthFormat24;
    v.drawableMultisample = GLKViewDrawableMultisampleNone;
    v.contentScaleFactor = 1.0;          /* altijd 1024x768 renderen, ook op retina */
    v.multipleTouchEnabled = YES;
    v.enableSetNeedsDisplay = NO;
    [EAGLContext setCurrentContext:_ctx];
    self.preferredFramesPerSecond = 30;
    self.pauseOnWillResignActive = YES;
    self.resumeOnDidBecomeActive = YES;
}

- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect {
    if (!_started) {
        int w = (int)view.drawableWidth, h = (int)view.drawableHeight;
        if (w < h) { int t = w; w = h; h = t; }
        if (w <= 0 || h <= 0) return;
        fe_init(w, h);
        _started = YES;
    }
    poll_controller();
    fe_frame(CACurrentMediaTime());
}

- (void)update {
}

- (void)sendTouches:(NSSet *)touches phase:(int)phase {
    CGFloat s = self.view.contentScaleFactor;
    for (UITouch *t in touches) {
        CGPoint p = [t locationInView:self.view];
        fe_touch((long)(__bridge void *)t, phase, (float)(p.x * s), (float)(p.y * s));
    }
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event { [self sendTouches:touches phase:TOUCH_BEGIN]; }
- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event { [self sendTouches:touches phase:TOUCH_MOVE]; }
- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event { [self sendTouches:touches phase:TOUCH_END]; }
- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event { [self sendTouches:touches phase:TOUCH_END]; }

- (BOOL)prefersStatusBarHidden { return YES; }
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskLandscape; }
- (BOOL)shouldAutorotate { return YES; }

@end
