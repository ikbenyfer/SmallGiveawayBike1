/* iOS-kant van platform.h */
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/ES2/gl.h>
#include <mach/mach.h>
#include "../Render/platform.h"

static NSString *docs(void) {
    return [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) firstObject];
}

int plat_load_image(const char *name, PlatImage *out) {
    @autoreleasepool {
        NSString *path = [[NSBundle mainBundle] pathForResource:[NSString stringWithUTF8String:name] ofType:nil];
        UIImage *img = path ? [UIImage imageWithContentsOfFile:path] : nil;
        CGImageRef cg = img.CGImage;
        if (!cg) return 0;
        int w = (int)CGImageGetWidth(cg), h = (int)CGImageGetHeight(cg);
        uint8_t *px = (uint8_t *)calloc((size_t)w * h, 4);
        if (!px) return 0;
        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(px, w, h, 8, w * 4, cs, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
        CGColorSpaceRelease(cs);
        if (!ctx) { free(px); return 0; }
        CGContextSetBlendMode(ctx, kCGBlendModeCopy);
        CGContextDrawImage(ctx, CGRectMake(0, 0, w, h), cg);
        CGContextRelease(ctx);
        out->w = w; out->h = h; out->rgba = px;
        return 1;
    }
}

void plat_free_image(PlatImage *img) {
    free(img->rgba);
    img->rgba = NULL;
}

int plat_make_font(FontData *out, float size) {
    @autoreleasepool {
        UIFont *font = [UIFont fontWithName:@"Futura-CondensedExtraBold" size:size];
        if (!font) font = [UIFont fontWithName:@"AvenirNextCondensed-Heavy" size:size];
        if (!font) font = [UIFont boldSystemFontOfSize:size];
        const int W = 1024, H = 512;
        const float pad = 6.0f;
        uint8_t *px = (uint8_t *)calloc(W * H, 4);
        if (!px) return 0;
        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(px, W, H, 8, W * 4, cs, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
        CGColorSpaceRelease(cs);
        if (!ctx) { free(px); return 0; }
        CGContextTranslateCTM(ctx, 0, H);
        CGContextScaleCTM(ctx, 1, -1);
        UIGraphicsPushContext(ctx);
        NSDictionary *plain = @{NSFontAttributeName: font};
        NSDictionary *attrs = @{NSFontAttributeName: font,
                                NSForegroundColorAttributeName: [UIColor whiteColor],
                                NSStrokeColorAttributeName: [UIColor blackColor],
                                NSStrokeWidthAttributeName: @(-9.0)};
        float lineh = (float)ceil(font.lineHeight);
        float cellh = lineh + pad * 2;
        float x = 1, y = 1;
        int c;
        for (c = 32; c < 128; c++) {
            unichar ch = (unichar)(c == 127 ? '?' : c);
            NSString *s = [NSString stringWithCharacters:&ch length:1];
            CGSize sz = [s sizeWithAttributes:plain];
            float adv = (float)ceil(sz.width);
            float cw = adv + pad * 2;
            Glyph *g = &out->g[c - 32];
            if (x + cw > W - 1) { x = 1; y += cellh + 1; }
            if (y + cellh > H - 1) break;
            [s drawAtPoint:CGPointMake(x + pad, y + pad) withAttributes:attrs];
            g->u0 = x / W; g->v0 = y / H; g->u1 = (x + cw) / W; g->v1 = (y + cellh) / H;
            g->w = cw; g->h = cellh; g->xoff = -pad; g->yoff = -pad; g->adv = adv;
            x += cw + 1;
        }
        UIGraphicsPopContext();
        CGContextRelease(ctx);
        out->img.w = W; out->img.h = H; out->img.rgba = px;
        out->size = size;
        out->lineh = lineh;
        return 1;
    }
}

void plat_log(const char *fmt, ...) {
    va_list ap;
    char buf[1024];
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    NSLog(@"%s", buf);
}

double plat_time(void) { return CACurrentMediaTime(); }

int plat_load_int(const char *key, int def) {
    NSString *k = [NSString stringWithUTF8String:key];
    NSUserDefaults *d = [NSUserDefaults standardUserDefaults];
    if (![d objectForKey:k]) return def;
    return (int)[d integerForKey:k];
}

void plat_save_int(const char *key, int val) {
    [[NSUserDefaults standardUserDefaults] setInteger:val forKey:[NSString stringWithUTF8String:key]];
}

void plat_save_flush(void) { [[NSUserDefaults standardUserDefaults] synchronize]; }

static NSArray *autotest_files(void) {
    NSString *d = docs();
    return @[[d stringByAppendingPathComponent:@"autotest.txt"],
             [d stringByAppendingPathComponent:@"autotest/go.txt"]];
}

int plat_autotest_requested(void) {
    for (NSString *p in autotest_files())
        if ([[NSFileManager defaultManager] fileExistsAtPath:p]) return 1;
    return 0;
}

void plat_autotest_done(void) {
    for (NSString *p in autotest_files()) [[NSFileManager defaultManager] removeItemAtPath:p error:nil];
}

void plat_write_text(const char *name, const char *text) {
    NSString *p = [docs() stringByAppendingPathComponent:[NSString stringWithUTF8String:name]];
    [[NSString stringWithUTF8String:text] writeToFile:p atomically:YES encoding:NSUTF8StringEncoding error:nil];
}

void plat_screenshot(const char *name, int w, int h) {
    @autoreleasepool {
        uint8_t *px = (uint8_t *)malloc((size_t)w * h * 4);
        uint8_t *fl = (uint8_t *)malloc((size_t)w * h * 4);
        int y;
        if (!px || !fl) { free(px); free(fl); return; }
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
        for (y = 0; y < h; y++) {
            memcpy(fl + (size_t)y * w * 4, px + (size_t)(h - 1 - y) * w * 4, (size_t)w * 4);
            {
                int x;
                for (x = 0; x < w; x++) fl[((size_t)y * w + x) * 4 + 3] = 255;
            }
        }
        free(px);
        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(fl, w, h, 8, w * 4, cs, kCGImageAlphaNoneSkipLast | kCGBitmapByteOrder32Big);
        CGColorSpaceRelease(cs);
        if (ctx) {
            CGImageRef img = CGBitmapContextCreateImage(ctx);
            NSData *png = UIImagePNGRepresentation([UIImage imageWithCGImage:img]);
            NSString *dir = [docs() stringByAppendingPathComponent:@"shots"];
            [[NSFileManager defaultManager] createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
            [png writeToFile:[dir stringByAppendingPathComponent:[NSString stringWithFormat:@"%s.png", name]] atomically:YES];
            CGImageRelease(img);
            CGContextRelease(ctx);
        }
        free(fl);
    }
}

long plat_memory_kb(void) {
    struct task_basic_info info;
    mach_msg_type_number_t n = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &n) != KERN_SUCCESS) return -1;
    return (long)(info.resident_size / 1024);
}

/* ------------------------------------------------------------------ netwerk (NSURLSession)
 * iOS 9 kent de Let's Encrypt-root ISRG Root X2 (3D BAG) nog niet. We voegen de officiele
 * rootcertificaten uit de app-bundel toe als extra vertrouwensanker; de systeemroots blijven
 * gewoon gelden en de hostnaam wordt nog steeds gecontroleerd. Geen certificaat wordt overgeslagen. */

@interface SGBNet : NSObject <NSURLSessionDelegate>
@property (strong) NSURLSession *session;
@property (strong) NSMutableDictionary *results;   /* id -> NSData of NSNull (fout) */
@property (strong) NSArray *anchors;
@end

@implementation SGBNet
- (instancetype)init {
    if ((self = [super init])) {
        NSURLSessionConfiguration *cfg = [NSURLSessionConfiguration defaultSessionConfiguration];
        cfg.timeoutIntervalForRequest = 45;
        cfg.timeoutIntervalForResource = 120;
        cfg.HTTPAdditionalHeaders = @{@"User-Agent": @"SmallGiveawayBike1/1.0 (iOS 9)"};
        self.session = [NSURLSession sessionWithConfiguration:cfg delegate:self delegateQueue:nil];
        self.results = [NSMutableDictionary dictionary];
        NSMutableArray *a = [NSMutableArray array];
        for (NSString *n in @[@"isrg-root-x1", @"isrg-root-x2", @"digicert-global-root-g2"]) {
            NSString *p = [[NSBundle mainBundle] pathForResource:n ofType:@"der"];
            NSData *d = p ? [NSData dataWithContentsOfFile:p] : nil;
            SecCertificateRef c = d ? SecCertificateCreateWithData(NULL, (__bridge CFDataRef)d) : NULL;
            if (c) { [a addObject:(__bridge_transfer id)c]; }
            else NSLog(@"SGB-NET kan certificaat %@ niet laden", n);
        }
        self.anchors = a;
    }
    return self;
}

- (void)URLSession:(NSURLSession *)session didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge
 completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential *))completionHandler {
    if ([challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust]) {
        SecTrustRef trust = challenge.protectionSpace.serverTrust;
        SecTrustResultType res = kSecTrustResultInvalid;
        SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)self.anchors);
        SecTrustSetAnchorCertificatesOnly(trust, false);
        OSStatus st = SecTrustEvaluate(trust, &res);
        if (st == errSecSuccess && (res == kSecTrustResultUnspecified || res == kSecTrustResultProceed)) {
            completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:trust]);
        } else {
            NSLog(@"SGB-NET certificaat NIET vertrouwd voor %@ (status %d, resultaat %d)", challenge.protectionSpace.host, (int)st, (int)res);
            completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);
        }
        return;
    }
    completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
}
@end

static SGBNet *gNet;
static int gNetId;

int plat_http_start(const char *url) {
    @autoreleasepool {
        int myId;
        NSURL *u;
        if (!gNet) gNet = [[SGBNet alloc] init];
        myId = ++gNetId;
        u = [NSURL URLWithString:[NSString stringWithUTF8String:url]];
        if (!u) return -1;
        NSLog(@"SGB-NET start %d: %@", myId, u.host);
        double t0 = CACurrentMediaTime();
        NSURLSessionDataTask *task = [gNet.session dataTaskWithURL:u completionHandler:^(NSData *data, NSURLResponse *resp, NSError *err) {
            NSInteger code = [resp isKindOfClass:[NSHTTPURLResponse class]] ? ((NSHTTPURLResponse *)resp).statusCode : 0;
            id val = (!err && code == 200 && data) ? data : (id)[NSNull null];
            NSLog(@"SGB-NET klaar %d: http %ld, %lu bytes, %.1f s, fout=%@", myId, (long)code, (unsigned long)data.length,
                  CACurrentMediaTime() - t0, err ? [NSString stringWithFormat:@"%@ %ld", err.domain, (long)err.code] : @"geen");
            @synchronized (gNet.results) { gNet.results[@(myId)] = val; }
        }];
        [task resume];
        return myId;
    }
}

int plat_http_poll(int rid, char **data, long *len) {
    id val;
    if (!gNet || rid <= 0) return -1;
    @synchronized (gNet.results) {
        val = gNet.results[@(rid)];
        if (val) [gNet.results removeObjectForKey:@(rid)];
    }
    if (!val) return 0;
    if (val == [NSNull null]) return -1;
    {
        NSData *d = (NSData *)val;
        char *buf = (char *)malloc(d.length + 1);
        if (!buf) return -1;
        memcpy(buf, d.bytes, d.length);
        buf[d.length] = 0;
        *data = buf;
        *len = (long)d.length;
    }
    return 1;
}

int plat_read_file(const char *name, char **data, long *len) {
    @autoreleasepool {
        NSData *d = [NSData dataWithContentsOfFile:[docs() stringByAppendingPathComponent:[NSString stringWithUTF8String:name]]];
        char *buf;
        if (!d) return 0;
        buf = (char *)malloc(d.length + 1);
        if (!buf) return 0;
        memcpy(buf, d.bytes, d.length);
        buf[d.length] = 0;
        *data = buf;
        *len = (long)d.length;
        return 1;
    }
}

int plat_write_file(const char *name, const char *data, long len) {
    @autoreleasepool {
        NSString *p = [docs() stringByAppendingPathComponent:[NSString stringWithUTF8String:name]];
        [[NSFileManager defaultManager] createDirectoryAtPath:[p stringByDeletingLastPathComponent] withIntermediateDirectories:YES attributes:nil error:nil];
        return [[NSData dataWithBytes:data length:(NSUInteger)len] writeToFile:p atomically:YES] ? 1 : 0;
    }
}

void plat_delete_file(const char *name) {
    @autoreleasepool {
        [[NSFileManager defaultManager] removeItemAtPath:[docs() stringByAppendingPathComponent:[NSString stringWithUTF8String:name]] error:nil];
    }
}
