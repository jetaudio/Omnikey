//
//  AppDelegate.m
//  ModernKey
//
//  Created by Tuyen on 1/18/19.
//  Copyright © 2019 Tuyen Mai. All rights reserved.
//

#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <ServiceManagement/ServiceManagement.h>
#include <libproc.h>
#include <sys/proc_info.h>
#import "AppDelegate.h"
#import "ViewController.h"
#import "OmnikeyManager.h"
#import "MJAccessibilityUtils.h"
#import "OKRime.h"

AppDelegate* appDelegate;
extern ViewController* viewController;
extern void OnTableCodeChange(void);
extern void OnInputMethodChanged(void);
extern void RequestNewSession(void);
extern void OnActiveAppChanged(void);
extern void ChineseModeReset(void);
extern int vChineseMode;

//see document in Engine.h
int vLanguage = 1;
int vInputType = 0;
int vFreeMark = 0;
int vCodeTable = 0;
int vCheckSpelling = 1;
int vUseModernOrthography = 1;
int vQuickTelex = 0;
#define DEFAULT_SWITCH_STATUS 0x7A000206 //default option + z
int vSwitchKeyStatus = DEFAULT_SWITCH_STATUS;
int vRestoreIfWrongSpelling = 0;
int vFixRecommendBrowser = 1;
int vUseMacro = 1;
int vUseMacroInEnglishMode = 1;
int vAutoCapsMacro = 0;
int vSendKeyStepByStep = 0;
int vUseSmartSwitchKey = 1;
int vUpperCaseFirstChar = 0;
int vTempOffSpelling = 0;
int vAllowConsonantZFWJ = 0;
int vQuickStartConsonant = 0;
int vQuickEndConsonant = 0;
int vRememberCode = 1; //new on version 2.0
int vOtherLanguage = 1; //new on version 2.0
int vTempOffOmnikey = 0; //new on version 2.0

int vShowIconOnDock = 0; //new on version 2.0

int vPerformLayoutCompat = 0;

//beta feature
int vFixChromiumBrowser = 0; //new on version 2.0

extern int convertToolHotKey;
extern bool convertToolDontAlertWhenCompleted;

@interface AppDelegate ()

@end

// Menu bar icon for Chinese mode, drawn like the V and E images.
static NSImage *OKChineseStatusImage(BOOL highlighted, BOOL monochrome) {
    NSImage *image = [NSImage imageWithSize:NSMakeSize(18, 18) flipped:NO drawingHandler:^BOOL(NSRect rect) {
        NSColor *color = monochrome ? [NSColor blackColor] : (highlighted ? [NSColor whiteColor] : [NSColor systemRedColor]);
        NSAttributedString *text = [[NSAttributedString alloc] initWithString:@"中" attributes:@{
            NSFontAttributeName: [NSFont systemFontOfSize:14 weight:NSFontWeightSemibold],
            NSForegroundColorAttributeName: color,
        }];
        NSSize size = text.size;
        [text drawAtPoint:NSMakePoint(floor((NSWidth(rect) - size.width) / 2), floor((NSHeight(rect) - size.height) / 2))];
        return YES;
    }];
    image.template = monochrome;
    return image;
}


@implementation AppDelegate {
    NSWindowController *_mainWC;
    NSWindowController *_macroWC;
    NSWindowController *_convertWC;
    NSWindowController *_aboutWC;
    
    NSStatusItem *statusItem;
    NSMenu *theMenu;
    
    NSMenuItem* menuInputMethod;
    NSMenuItem* menuChinese;
    
    NSMenuItem* mnuTelex;
    NSMenuItem* mnuVNI;
    NSMenuItem* mnuSimpleTelex1;
    NSMenuItem* mnuSimpleTelex2;
    
    NSMenuItem* mnuUnicode;
    NSMenuItem* mnuTCVN;
    NSMenuItem* mnuVNIWindows;
    
    NSMenuItem* mnuUnicodeComposite;
    NSMenuItem* mnuVietnameseLocaleCP1258;
    
    NSMenuItem* mnuQuickConvert;
}

-(void)askPermission {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText: [NSString stringWithFormat:@"Omnikey cần bạn cấp quyền để có thể hoạt động!"]];
    [alert setInformativeText:@"Vui lòng chạy lại ứng dụng sau khi cấp quyền."];

    [alert addButtonWithTitle:@"Không"];
    [alert addButtonWithTitle:@"Cấp quyền"];

    [alert.window makeKeyAndOrderFront:nil];
    [alert.window setLevel:NSStatusWindowLevel];

    NSModalResponse res = [alert runModal];

    if (res == 1001) {
        MJAccessibilityOpenPanel();
    }

    [NSApp terminate:0];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    appDelegate = self;

    // Settings saved before the app was renamed from OpenKey are copied over once.
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if (![defaults boolForKey:@"MigratedFromOpenKey"]) {
        NSString *bundleId = NSBundle.mainBundle.bundleIdentifier;
        NSDictionary *legacy = [defaults persistentDomainForName:@"com.tuyenmai.openkey"];
        if (legacy.count > 0 && [defaults persistentDomainForName:bundleId].count == 0) {
            [defaults setPersistentDomain:legacy forName:bundleId];
        }
        [defaults setBool:YES forKey:@"MigratedFromOpenKey"];
    }

    [self registerSupportedNotification];
    
    //set quick tooltip
    [[NSUserDefaults standardUserDefaults] setObject: [NSNumber numberWithInt: 50]
                                              forKey: @"NSInitialToolTipDelay"];
    
    //check whether this app has been launched before that or not
    //Only check instances owned by current user (for multi-user/Fast User Switching support)
    uid_t currentUID = getuid();
    NSArray<NSRunningApplication *>* runningApps = [[NSWorkspace sharedWorkspace] runningApplications];
    pid_t myPID = [[NSProcessInfo processInfo] processIdentifier];
    BOOL alreadyRunning = NO;

    for (NSRunningApplication *app in runningApps) {
        if ([app.bundleIdentifier isEqualToString:OMNIKEY_BUNDLE] &&
            app.processIdentifier != myPID) {
            pid_t pid = app.processIdentifier;
            struct proc_bsdinfo proc;
            int size = proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &proc, sizeof(proc));
            if (size == sizeof(proc) && proc.pbi_uid == currentUID) {
                alreadyRunning = YES;
                break;
            }
        }
    }

    if (alreadyRunning) {
        [NSApp terminate:nil];
        return;
    }
    
    // check if user granted Accessabilty permission
    if (!MJAccessibilityIsEnabled()) {
        [self askPermission];
        return;
    }
    
    vShowIconOnDock = (int)[[NSUserDefaults standardUserDefaults] integerForKey:@"vShowIconOnDock"];
    if (vShowIconOnDock)
        [NSApp setActivationPolicy: NSApplicationActivationPolicyRegular];
    
    if (vSwitchKeyStatus & 0x8000)
        NSBeep();

    vChineseMode = (int)[[NSUserDefaults standardUserDefaults] integerForKey:@"ChineseMode"];
    [self createStatusBarMenu];
    
    //init
    dispatch_async(dispatch_get_main_queue(), ^{
        if (![OmnikeyManager initEventTap]) {
            [self onControlPanelSelected];
        } else {
            NSInteger showui = [[NSUserDefaults standardUserDefaults] integerForKey:@"ShowUIOnStartup"];
            if (showui == 1) {
                [self onControlPanelSelected];
            }
        }
        [self setQuickConvertString];
        // Chinese input loads in the background; until then 中 types Latin.
        [[OKRime shared] startWithCompletion:nil];
    });
    
    //load default config if is first launch
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"NonFirstTime"] == 0) {
        [self loadDefaultConfig];
    }
    [[NSUserDefaults standardUserDefaults] setInteger:1 forKey:@"NonFirstTime"];
    
    //check update if enable
    NSInteger dontCheckUpdate = [[NSUserDefaults standardUserDefaults] integerForKey:@"DontCheckUpdate"];
    if (!dontCheckUpdate)
        [OmnikeyManager checkNewVersion:nil callbackFunc:nil];
    
    //correct run on startup
    NSInteger val = [[NSUserDefaults standardUserDefaults] integerForKey:@"RunOnStartup"];
    [appDelegate setRunOnStartup:val];
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag {
    [self onControlPanelSelected];
    return YES;
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
    // Insert code here to tear down your application
}

-(void) createStatusBarMenu {
    NSStatusBar *statusBar = [NSStatusBar systemStatusBar];
    statusItem = [statusBar statusItemWithLength:NSVariableStatusItemLength];
    statusItem.button.image = [NSImage imageNamed:@"Status"];
    statusItem.button.alternateImage = [NSImage imageNamed:@"StatusHighlighted"];
    
    theMenu = [[NSMenu alloc] initWithTitle:@""];
    [theMenu setAutoenablesItems:NO];
    
    menuInputMethod = [theMenu addItemWithTitle:@"Bật Tiếng Việt"
                                                     action:@selector(onInputMethodSelected)
                                              keyEquivalent:@""];
    menuChinese = [theMenu addItemWithTitle:@"Gõ tiếng Trung (Pinyin)"
                                     action:@selector(onChineseSelected)
                              keyEquivalent:@""];
    [theMenu addItem:[NSMenuItem separatorItem]];
    NSMenuItem* menuInputType = [theMenu addItemWithTitle:@"Kiểu gõ" action:nil keyEquivalent:@""];
    
    [theMenu addItem:[NSMenuItem separatorItem]];
    
    mnuUnicode = [theMenu addItemWithTitle:@"Unicode dựng sẵn" action:@selector(onCodeSelected:) keyEquivalent:@""];
    mnuUnicode.tag = 0;
    mnuTCVN = [theMenu addItemWithTitle:@"TCVN3 (ABC)" action:@selector(onCodeSelected:) keyEquivalent:@""];
    mnuTCVN.tag = 1;
    mnuVNIWindows = [theMenu addItemWithTitle:@"VNI Windows" action:@selector(onCodeSelected:) keyEquivalent:@""];
    mnuVNIWindows.tag = 2;
    NSMenuItem* menuCode = [theMenu addItemWithTitle:@"Bảng mã khác" action:nil keyEquivalent:@""];
    
    [theMenu addItem:[NSMenuItem separatorItem]];
    
    [theMenu addItemWithTitle:@"Công cụ chuyển mã..." action:@selector(onConvertTool) keyEquivalent:@""];
    mnuQuickConvert = [theMenu addItemWithTitle:@"Chuyển mã nhanh" action:@selector(onQuickConvert) keyEquivalent:@""];
    
    [theMenu addItem:[NSMenuItem separatorItem]];
    
    [theMenu addItemWithTitle:@"Bảng điều khiển..." action:@selector(onControlPanelSelected) keyEquivalent:@""];
    [theMenu addItemWithTitle:@"Gõ tắt..." action:@selector(onMacroSelected) keyEquivalent:@""];
    [theMenu addItemWithTitle:@"Giới thiệu" action:@selector(onAboutSelected) keyEquivalent:@""];
    [theMenu addItem:[NSMenuItem separatorItem]];
    
    [theMenu addItemWithTitle:@"Thoát" action:@selector(terminate:) keyEquivalent:@"q"];
    
    
    [self setInputTypeMenu:menuInputType];
    [self setCodeMenu:menuCode];
    
    //set menu
    [statusItem setMenu:theMenu];
    
    [self fillData];
}

-(void)setQuickConvertString {
    NSMutableString* hotKey = [NSMutableString stringWithString:@""];
    bool hasAdd = false;
    if (convertToolHotKey & 0x100) {
        [hotKey appendString:@"⌃"];
        hasAdd = true;
    }
    if (convertToolHotKey & 0x200) {
        if (hasAdd)
            [hotKey appendString:@" + "];
        [hotKey appendString:@"⌥"];
        hasAdd = true;
    }
    if (convertToolHotKey & 0x400) {
        if (hasAdd)
            [hotKey appendString:@" + "];
        [hotKey appendString:@"⌘"];
        hasAdd = true;
    }
    if (convertToolHotKey & 0x800) {
        if (hasAdd)
            [hotKey appendString:@" + "];
        [hotKey appendString:@"⇧"];
        hasAdd = true;
    }
    
    unsigned short k = ((convertToolHotKey>>24) & 0xFF);
    if (k != 0xFE) {
        if (hasAdd)
            [hotKey appendString:@" + "];
        if (k == kVK_Space)
            [hotKey appendFormat:@"%@", @"␣ "];
        else
            [hotKey appendFormat:@"%c", k];
    }
    [mnuQuickConvert setTitle: hasAdd ? [NSString stringWithFormat:@"Chuyển mã nhanh - [%@]", [hotKey uppercaseString]] : @"Chuyển mã nhanh"];
}

-(void)loadDefaultConfig {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    vChineseMode = 0; [defaults setInteger:vChineseMode forKey:@"ChineseMode"];
    ChineseModeReset();
    vLanguage = 1; [defaults setInteger:vLanguage forKey:@"InputMethod"];
    vInputType = 0; [defaults setInteger:vInputType forKey:@"InputType"];
    vFreeMark = 0; [defaults setInteger:vFreeMark forKey:@"FreeMark"];
    vCheckSpelling = 1; [defaults setInteger:vCheckSpelling forKey:@"Spelling"];
    vCodeTable = 0; [defaults setInteger:vCodeTable forKey:@"CodeTable"];
    vSwitchKeyStatus = DEFAULT_SWITCH_STATUS; [defaults setInteger:vSwitchKeyStatus forKey:@"SwitchKeyStatus"];
    vQuickTelex = 0; [defaults setInteger:vQuickTelex forKey:@"QuickTelex"];
    vUseModernOrthography = 0; [defaults setInteger:vUseModernOrthography forKey:@"ModernOrthography"];
    vRestoreIfWrongSpelling = 0; [defaults setInteger:vRestoreIfWrongSpelling forKey:@"RestoreIfInvalidWord"];
    vFixRecommendBrowser = 1; [defaults setInteger:vFixRecommendBrowser forKey:@"FixRecommendBrowser"];
    vUseMacro = 1; [defaults setInteger:vUseMacro forKey:@"UseMacro"];
    vUseMacroInEnglishMode = 0; [defaults setInteger:vUseMacroInEnglishMode forKey:@"UseMacroInEnglishMode"];
    vSendKeyStepByStep = 0;[defaults setInteger:vSendKeyStepByStep forKey:@"SendKeyStepByStep"];
    vUseSmartSwitchKey = 1;[defaults setInteger:vUseSmartSwitchKey forKey:@"UseSmartSwitchKey"];
    vUpperCaseFirstChar = 0;[defaults setInteger:vUpperCaseFirstChar forKey:@"UpperCaseFirstChar"];
    vTempOffSpelling = 0;[defaults setInteger:vTempOffSpelling forKey:@"vTempOffSpelling"];
    vAllowConsonantZFWJ = 0;[defaults setInteger:vAllowConsonantZFWJ forKey:@"vAllowConsonantZFWJ"];
    vQuickStartConsonant = 0;[defaults setInteger:vQuickStartConsonant forKey:@"vQuickStartConsonant"];
    vQuickEndConsonant = 0;[defaults setInteger:vQuickEndConsonant forKey:@"vQuickEndConsonant"];
    vRememberCode = 1;[defaults setInteger:vRememberCode forKey:@"vRememberCode"];
    vOtherLanguage = 1;[defaults setInteger:vOtherLanguage forKey:@"vOtherLanguage"];
    vTempOffOmnikey = 0;[defaults setInteger:vTempOffOmnikey forKey:@"vTempOffOmnikey"];
    vShowIconOnDock = 0;[defaults setInteger:vShowIconOnDock forKey:@"vShowIconOnDock"];
    vFixChromiumBrowser = 0;[defaults setInteger:vFixChromiumBrowser forKey:@"vFixChromiumBrowser"];
    vPerformLayoutCompat = 0;[defaults setInteger:vPerformLayoutCompat forKey:@"vPerformLayoutCompat"];

    [defaults setInteger:1 forKey:@"GrayIcon"];
    [defaults setInteger:1 forKey:@"RunOnStartup"];
    [self setRunOnStartup:YES];

    [self fillData];
    [viewController fillData];
}

-(void)setRunOnStartup:(BOOL)val {
    if (@available(macOS 13.0, *)) {
        SMAppService *service = [SMAppService mainAppService];
        NSError *error = nil;
        BOOL success = YES;
        // An app that was never registered reports NotFound rather than
        // NotRegistered, so register whenever the login item is not active yet.
        if (val && (service.status == SMAppServiceStatusNotRegistered ||
                    service.status == SMAppServiceStatusNotFound)) {
            success = [service registerAndReturnError:&error];
        } else if (!val && (service.status == SMAppServiceStatusEnabled ||
                            service.status == SMAppServiceStatusRequiresApproval)) {
            success = [service unregisterAndReturnError:&error];
        }
        if (!success) NSLog(@"Omnikey login item update failed: %@", error);
        return;
    }
    CFStringRef appId = (__bridge CFStringRef)@"com.tuyenmai.OmnikeyHelper";
    SMLoginItemSetEnabled(appId, val);
}

-(void)setGrayIcon:(BOOL)val {
    [self fillData];
}

-(void)showIconOnDock:(BOOL)val {
    [NSApp setActivationPolicy: val ? NSApplicationActivationPolicyRegular : NSApplicationActivationPolicyAccessory];
}

#pragma mark -StatusBar menu data

- (void)setInputTypeMenu:(NSMenuItem*) parent {
    //sub for Kieu Go
    NSMenu *sub = [[NSMenu alloc] initWithTitle:@""];
    [sub setAutoenablesItems:NO];
    mnuTelex = [sub addItemWithTitle:@"Telex" action:@selector(onInputTypeSelected:) keyEquivalent:@""];
    mnuTelex.tag = 0;
    mnuVNI = [sub addItemWithTitle:@"VNI" action:@selector(onInputTypeSelected:) keyEquivalent:@""];
    mnuVNI.tag = 1;
    mnuSimpleTelex1 = [sub addItemWithTitle:@"Simple Telex 1" action:@selector(onInputTypeSelected:) keyEquivalent:@""];
    mnuSimpleTelex1.tag = 2;
    mnuSimpleTelex2 = [sub addItemWithTitle:@"Simple Telex 2" action:@selector(onInputTypeSelected:) keyEquivalent:@""];
    mnuSimpleTelex2.tag = 3;
    [theMenu setSubmenu:sub forItem:parent];
}

- (void)setCodeMenu:(NSMenuItem*) parent {
    //sub for Code
    NSMenu *sub = [[NSMenu alloc] initWithTitle:@""];
    [sub setAutoenablesItems:NO];
    mnuUnicodeComposite = [sub addItemWithTitle:@"Unicode tổ hợp" action:@selector(onCodeSelected:) keyEquivalent:@""];
    mnuUnicodeComposite.tag = 3;
    mnuVietnameseLocaleCP1258 = [sub addItemWithTitle:@"Vietnamese Locale CP 1258" action:@selector(onCodeSelected:) keyEquivalent:@""];
    mnuVietnameseLocaleCP1258.tag = 4;
    
    [theMenu setSubmenu:sub forItem:parent];
}

// Checks the item whose tag matches the selected index and unchecks the rest.
static void OKSelectMenuItemWithTag(NSArray<NSMenuItem *> *items, NSInteger tag) {
    for (NSMenuItem *item in items) {
        [item setState:(item.tag == tag ? NSControlStateValueOn : NSControlStateValueOff)];
    }
}

- (void) fillData {
    //fill data
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSInteger intInputMethod = [defaults integerForKey:@"InputMethod"];
    NSInteger grayIcon = [defaults integerForKey:@"GrayIcon"];
    [menuChinese setState:vChineseMode ? NSControlStateValueOn : NSControlStateValueOff];
    if (vChineseMode) {
        [menuInputMethod setState:NSControlStateValueOff];
        statusItem.button.image = OKChineseStatusImage(NO, grayIcon ? YES : NO);
        statusItem.button.alternateImage = OKChineseStatusImage(YES, grayIcon ? YES : NO);
    } else if (intInputMethod == 1) {
        [menuInputMethod setState:NSControlStateValueOn];
        statusItem.button.image = [NSImage imageNamed:@"Status"];
        [statusItem.button.image setTemplate:(grayIcon ? YES : NO)];
        statusItem.button.alternateImage = [NSImage imageNamed:@"StatusHighlighted"];
    } else {
        [menuInputMethod setState:NSControlStateValueOff];
        statusItem.button.image = [NSImage imageNamed:@"StatusEng"];
        [statusItem.button.image setTemplate:(grayIcon ? YES : NO)];
        statusItem.button.alternateImage = [NSImage imageNamed:@"StatusHighlightedEng"];
    }
    vLanguage = (int)intInputMethod;
    
    // Menu item tags are the stored input type and code table indexes; the
    // menus do not exist yet while Accessibility permission is pending.
    NSInteger intInputType = [defaults integerForKey:@"InputType"];
    if (mnuTelex) OKSelectMenuItemWithTag(@[mnuTelex, mnuVNI, mnuSimpleTelex1, mnuSimpleTelex2], intInputType);
    vInputType = (int)intInputType;
    
    NSInteger intSwitchKeyStatus = [defaults integerForKey:@"SwitchKeyStatus"];
    vSwitchKeyStatus = (int)intSwitchKeyStatus;
    if (vSwitchKeyStatus == 0)
        vSwitchKeyStatus = DEFAULT_SWITCH_STATUS;
    
    NSInteger intCode = [defaults integerForKey:@"CodeTable"];
    if (mnuUnicode) OKSelectMenuItemWithTag(@[mnuUnicode, mnuTCVN, mnuVNIWindows, mnuUnicodeComposite, mnuVietnameseLocaleCP1258], intCode);
    vCodeTable = (int)intCode;
    
}

-(void)onImputMethodChanged:(BOOL)willNotify {
    if (vChineseMode) {
        [self selectInputMode:1];
        return;
    }
    [self setInputMethod:(vLanguage == 0 ? 1 : 0) willNotify:willNotify];
}

-(int)currentInputMode {
    return vChineseMode ? 2 : vLanguage;
}

-(void)selectInputMode:(int)mode {
    BOOL chinese = mode == 2;
    if (chinese != (vChineseMode != 0)) {
        vChineseMode = chinese ? 1 : 0;
        [[NSUserDefaults standardUserDefaults] setInteger:vChineseMode forKey:@"ChineseMode"];
        ChineseModeReset();
        // The Vietnamese engine sees no keys in 中, so its word and line
        // history is stale on either side of the switch.
        RequestNewSession();
        if (chinese) [[OKRime shared] startWithCompletion:nil];
    }
    // The Vietnamese engine stays in English while Chinese input is active.
    [self setInputMethod:(chinese ? 0 : mode) willNotify:YES];
}

-(void)setInputMethod:(int)targetLanguage willNotify:(BOOL)willNotify {
    vLanguage = targetLanguage & 1;
    [[NSUserDefaults standardUserDefaults] setInteger:vLanguage forKey:@"InputMethod"];

    [self fillData];
    [viewController fillData];
    
    if (willNotify)
        OnInputMethodChanged();
}

#pragma mark -StatusBar menu action
- (void)onInputMethodSelected {
    [self onImputMethodChanged:YES];
}

- (void)onChineseSelected {
    [self selectInputMode:(vChineseMode ? 1 : 2)];
}

- (void)onInputTypeSelected:(id)sender {
    NSMenuItem *menuItem = (NSMenuItem*) sender;
    [self onInputTypeSelectedIndex:(int)menuItem.tag];
}

- (void)onInputTypeSelectedIndex:(int)index {
    [[NSUserDefaults standardUserDefaults] setInteger:index forKey:@"InputType"];
    vInputType = index;
    [self fillData];
    [viewController fillData];
}

- (void)onCodeTableChanged:(int)index {
    [[NSUserDefaults standardUserDefaults] setInteger:index forKey:@"CodeTable"];
    vCodeTable = index;
    [self fillData];
    [viewController fillData];
    OnTableCodeChange();
}

- (void)onCodeSelected:(id)sender {
    NSMenuItem *menuItem = (NSMenuItem*) sender;
    [self onCodeTableChanged:(int)menuItem.tag];
}

// Recover only windows whose title bar is no longer reachable on any display.
- (void)showWindowController:(NSWindowController *)controller {
    NSWindow *window = controller.window;
    NSRect frame = window.frame;
    NSRect titleBar = NSMakeRect(NSMinX(frame), NSMaxY(frame) - 30, NSWidth(frame), 30);
    BOOL reachable = NO;
    for (NSScreen *screen in [NSScreen screens]) {
        NSRect overlap = NSIntersectionRect(titleBar, screen.visibleFrame);
        if (overlap.size.width >= 80 && overlap.size.height >= 20) {
            reachable = YES;
            break;
        }
    }
    if (!reachable) {
        NSScreen *screen = [NSScreen mainScreen] ?: [NSScreen screens].firstObject;
        if (screen != nil) {
            NSRect visible = screen.visibleFrame;
            [window setFrameOrigin:NSMakePoint(NSMidX(visible) - NSWidth(frame) / 2,
                                               NSMidY(visible) - NSHeight(frame) / 2)];
        }
    }
    [window setLevel:NSFloatingWindowLevel];
    [NSApp activateIgnoringOtherApps:YES];
    [window makeKeyAndOrderFront:nil];
}

-(void)onConvertTool {
    if (_convertWC == nil) {
        _convertWC = [[NSStoryboard storyboardWithName:@"Main" bundle:nil] instantiateControllerWithIdentifier:@"ConvertWindow"];
    }
    [self showWindowController:_convertWC];
}

-(void)onQuickConvert {
    if ([OmnikeyManager quickConvert]) {
        if (!convertToolDontAlertWhenCompleted) {
            [OmnikeyManager showMessage: nil message:@"Chuyển mã thành công!" subMsg:@"Kết quả đã được lưu trong clipboard."];
        }
    } else {
        [OmnikeyManager showMessage: nil message:@"Không có dữ liệu trong clipboard!" subMsg:@"Hãy sao chép một đoạn text để chuyển đổi!"];
    }
}

-(void) onControlPanelSelected {
    if (_mainWC == nil) {
        _mainWC = [[NSStoryboard storyboardWithName:@"Main" bundle:nil] instantiateControllerWithIdentifier:@"Omnikey"];
    }
    [self showWindowController:_mainWC];
}

-(void) onMacroSelected {
    if (_macroWC == nil) {
        _macroWC = [[NSStoryboard storyboardWithName:@"Main" bundle:nil] instantiateControllerWithIdentifier:@"MacroWindow"];
    }
    [self showWindowController:_macroWC];
}

-(void) onAboutSelected {
    if (_aboutWC == nil) {
        _aboutWC = [[NSStoryboard storyboardWithName:@"Main" bundle:nil] instantiateControllerWithIdentifier:@"AboutWindow"];
    }
    [self showWindowController:_aboutWC];
}

#pragma mark -Short key event
-(void)onSwitchLanguage {
    [self onInputMethodSelected];
    [viewController fillData];
}

#pragma mark Reset Omnikey after mac computer awake
-(void)receiveWakeNote: (NSNotification*)note {
    [OmnikeyManager initEventTap];
}

-(void)receiveSleepNote: (NSNotification*)note {
    [OmnikeyManager stopEventTap];
}

-(void)receiveActiveSpaceChanged: (NSNotification*)note {
    RequestNewSession();
}

-(void)activeAppChanged: (NSNotification*)note {
    if (vUseSmartSwitchKey && [OmnikeyManager isInited]) {
        OnActiveAppChanged();
    }
}

-(void)registerSupportedNotification {
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver: self
                                                           selector: @selector(receiveWakeNote:)
                                                               name: NSWorkspaceDidWakeNotification object: NULL];
    
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver: self
                                                           selector: @selector(receiveSleepNote:)
                                                               name: NSWorkspaceWillSleepNotification object: NULL];
    
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver: self
                                                           selector: @selector(receiveActiveSpaceChanged:)
                                                               name: NSWorkspaceActiveSpaceDidChangeNotification object: NULL];
    
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver: self
                                                           selector: @selector(activeAppChanged:)
                                                               name: NSWorkspaceDidActivateApplicationNotification object: NULL];
}
@end
