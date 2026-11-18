#import "GameViewController.h"
#import <QuartzCore/QuartzCore.h>
#include "../Render/frontend.h"

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
