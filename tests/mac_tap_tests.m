#import <Cocoa/Cocoa.h>

static BOOL failTap=NO, failSource=NO, tapEnabled=NO;
static int initializations=0, frees=0, enables=0, invalidations=0, additions=0, removals=0;
static CFMachPortRef createTap(CGEventTapLocation, CGEventTapPlacement, CGEventTapOptions,
                              CGEventMask, CGEventTapCallBack, void *);
static CFRunLoopSourceRef createSource(CFAllocatorRef, CFMachPortRef, CFIndex);
static void invalidateTap(CFMachPortRef);
static void addSource(CFRunLoopRef, CFRunLoopSourceRef, CFStringRef);
static void removeSource(CFRunLoopRef, CFRunLoopSourceRef, CFStringRef);
static void enableTap(CFMachPortRef, bool);
static bool isEnabled(CFMachPortRef);
#define CGEventTapCreate createTap
#define CFMachPortCreateRunLoopSource createSource
#define CFMachPortInvalidate invalidateTap
#define CFRunLoopAddSource addSource
#define CFRunLoopRemoveSource removeSource
#define CGEventTapEnable enableTap
#define CGEventTapIsEnabled isEnabled
#include "../Sources/Omnikey/macOS/ModernKey/OmnikeyManager.m"

void OmnikeyInit(void) { ++initializations; }
void OmnikeyFree(void) { ++frees; }
CGEventRef OmnikeyCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) { return event; }
NSString *ConvertUtil(NSString *str) { return str; }

static CFMachPortRef createTap(CGEventTapLocation location, CGEventTapPlacement placement,
        CGEventTapOptions options, CGEventMask mask, CGEventTapCallBack callback, void *refcon) {
    return failTap ? NULL : (CFMachPortRef)CFStringCreateCopy(NULL, CFSTR("fake event tap"));
}
static CFRunLoopSourceRef createSource(CFAllocatorRef allocator, CFMachPortRef port, CFIndex order) {
    return failSource ? NULL : (CFRunLoopSourceRef)CFStringCreateCopy(NULL, CFSTR("fake run loop source"));
}
static void invalidateTap(CFMachPortRef port) { ++invalidations; }
static void addSource(CFRunLoopRef loop, CFRunLoopSourceRef source, CFStringRef mode) { ++additions; }
static void removeSource(CFRunLoopRef loop, CFRunLoopSourceRef source, CFStringRef mode) { ++removals; }
static void enableTap(CFMachPortRef port, bool enabled) { ++enables; tapEnabled=enabled; }
static bool isEnabled(CFMachPortRef port) { return tapEnabled; }

static int assertions=0;
static void expect(BOOL condition, const char *label) {
    ++assertions;
    if (!condition) { fprintf(stderr, "FAIL: %s\n", label); exit(1); }
}
int main(void) { @autoreleasepool {
    failTap=YES;
    expect(![OmnikeyManager initEventTap], "failed tap creation is reported");
    expect(![OmnikeyManager isInited] && frees==1 && watchdogTimer==NULL, "failed initialization cleans engine state");
    failTap=NO; failSource=YES;
    expect(![OmnikeyManager initEventTap], "failed source creation is reported");
    expect(eventTap==NULL && frees==2 && invalidations==1, "failed source creation releases tap");
    failSource=NO;
    expect([OmnikeyManager initEventTap], "successful initialization");
    expect([OmnikeyManager isInited] && additions==1 && watchdogTimer!=NULL, "watchdog and source initialized");
    expect([OmnikeyManager initEventTap] && initializations==3, "repeated initialization is idempotent");
    int before=enables;
    tapEnabled=NO;
    CFRunLoopTimerSetNextFireDate(watchdogTimer, CFAbsoluteTimeGetCurrent()-1);
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.02, false);
    expect(tapEnabled && enables==before+1, "watchdog re-enables a disabled tap");
    [OmnikeyManager stopEventTap];
    expect(![OmnikeyManager isInited] && eventTap==NULL && runLoopSource==NULL && watchdogTimer==NULL,
           "stop releases all lifecycle objects");
    expect(removals==1 && frees==3, "stop frees engine and removes source");
    [OmnikeyManager stopEventTap];
    expect(frees==3, "repeated stop is idempotent");
    before=enables;
    OmnikeyReEnableEventTap();
    expect(enables==before, "stopped tap cannot be resurrected");
    printf("macOS event tap lifecycle tests: %d assertions passed.\n", assertions);
} }
