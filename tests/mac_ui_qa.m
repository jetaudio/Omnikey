// Load and render the compiled preferences without launching Omnikey or creating
// an event tap. Defaults belong to this separate QA executable, not Omnikey.app.
#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>
#import "ViewController.h"
#import "OmnikeyManager.h"
#import "AppDelegate.h"

extern AppDelegate *appDelegate;
extern int vSwitchKeyStatus;
@interface ViewController (ReviewActions)
- (IBAction)onFnSwitchKey:(NSButton *)sender;
@end
static BOOL skipEventTap(id receiver, SEL selector) { return YES; }
static void fail(NSString *message) { NSLog(@"UI QA failed: %@", message); exit(1); }
int main(int argc, const char *argv[]) { @autoreleasepool {
    if (argc != 3) return 2;
    [NSApplication sharedApplication];
    // OMNIKEY_UI_APPEARANCE=dark renders the dark appearance (light by default).
    const char *appearance=getenv("OMNIKEY_UI_APPEARANCE");
    NSApp.appearance=[NSAppearance appearanceNamed:(appearance && strcmp(appearance, "dark")==0) ?
        NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
    Method method=class_getClassMethod([OmnikeyManager class], @selector(initEventTap));
    method_setImplementation(method, (IMP)skipEventTap);
    NSBundle *bundle=[NSBundle bundleWithPath:[NSString stringWithUTF8String:argv[1]]];
    if (bundle==nil) fail(@"missing built app bundle");
    NSWindowController *window=[[NSStoryboard storyboardWithName:@"Main" bundle:bundle]
        instantiateControllerWithIdentifier:@"Omnikey"];
    ViewController *controller=(ViewController *)window.contentViewController;
    [controller.view layoutSubtreeIfNeeded];
    NSButton *fn=[controller valueForKey:@"CustomSwitchFn"];
    MyTextField *key=[controller valueForKey:@"CustomSwitchKey"];
    NSButton *shift=[controller valueForKey:@"CustomSwitchShift"];
    NSButton *beep=[controller valueForKey:@"CustomBeepSound"];
    if (fn==nil || key==nil || shift==nil || beep==nil) fail(@"Fn/key outlets are not connected");
    // The storyboard modifier checkboxes are mirrored by one multi-select
    // segmented control placed next to the key field.
    NSSegmentedControl *modifiers=nil;
    NSMutableArray *pending=[NSMutableArray arrayWithObject:controller.view];
    while (pending.count > 0) {
        NSView *view=pending.lastObject; [pending removeLastObject];
        if ([view isKindOfClass:[NSSegmentedControl class]] && ((NSSegmentedControl *)view).segmentCount==5) modifiers=(NSSegmentedControl *)view;
        [pending addObjectsFromArray:view.subviews];
    }
    if (modifiers==nil) fail(@"missing modifier key control");
    if (key.window!=modifiers.window || key.superview!=modifiers.superview) fail(@"key field is not beside the modifier keys");
    NSRect keyFrame=[controller.view convertRect:key.bounds fromView:key];
    NSRect modifiersFrame=[controller.view convertRect:modifiers.bounds fromView:modifiers];
    if (!NSContainsRect(controller.view.bounds, keyFrame) || !NSContainsRect(controller.view.bounds, modifiersFrame)) fail(@"shortcut controls are outside the view");
    if (NSIntersectsRect(keyFrame, modifiersFrame)) fail(@"modifier keys overlap key field");

    NSString *domain=[NSProcessInfo processInfo].processName;
    NSUserDefaults *defaults=[NSUserDefaults standardUserDefaults];
    vSwitchKeyStatus=0x7A008206;
    fn.state=NSControlStateValueOn;
    [controller onFnSwitchKey:fn];
    if (vSwitchKeyStatus!=0xFE0090FE || key.stringValue.length!=0) fail(@"selecting Fn does not configure Fn alone");
    if (shift.state!=NSControlStateValueOff || beep.state!=NSControlStateValueOn) fail(@"Fn state did not refresh controls");
    if (![modifiers isSelectedForSegment:4] || [modifiers isSelectedForSegment:3]) fail(@"modifier control did not mirror Fn state");
    [modifiers setSelected:NO forSegment:4];
    [NSApp sendAction:modifiers.action to:modifiers.target from:modifiers];
    if (fn.state!=NSControlStateValueOff || (vSwitchKeyStatus & 0x1000)) fail(@"modifier control did not clear Fn");
    [defaults removePersistentDomainForName:domain];

    // The other windows rebuild their storyboard layouts in code.
    for (NSString *identifier in @[@"AboutWindow", @"ConvertWindow", @"MacroWindow"]) {
        NSWindowController *other=[[NSStoryboard storyboardWithName:@"Main" bundle:bundle]
            instantiateControllerWithIdentifier:identifier];
        NSView *view=other.contentViewController.view;
        [view layoutSubtreeIfNeeded];
        if (view.subviews.count==0 || NSWidth(view.frame)<300 || NSHeight(view.frame)<200)
            fail([NSString stringWithFormat:@"%@ has no usable layout", identifier]);
        for (NSView *subview in view.subviews) {
            if (!NSContainsRect(NSInsetRect(view.bounds, -1, -1), subview.frame))
                fail([NSString stringWithFormat:@"%@ lays out %@ outside the window", identifier, subview]);
        }
    }

    NSBitmapImageRep *bitmap=[controller.view bitmapImageRepForCachingDisplayInRect:controller.view.bounds];
    [controller.view cacheDisplayInRect:controller.view.bounds toBitmapImageRep:bitmap];
    NSData *png=[bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    if (![png writeToFile:[NSString stringWithUTF8String:argv[2]] atomically:YES]) fail(@"could not save preferences rendering");
    printf("Preferences UI: compiled storyboard/outlets, geometry, Fn action and About/Convert/Macro layouts verified.\n");
} }
