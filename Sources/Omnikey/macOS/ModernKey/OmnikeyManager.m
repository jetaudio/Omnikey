//
//  OmnikeyManager.m
//  OpenKey
//
//  Created by Tuyen on 1/27/19.
//  Copyright © 2019 Tuyen Mai. All rights reserved.
//

#import "OmnikeyManager.h"
#import "OKUpdater.h"

extern void OmnikeyInit(void);
extern void OmnikeyFree(void);
void OmnikeyReEnableEventTap(void);

extern CGEventRef OmnikeyCallback(CGEventTapProxy proxy,
                                  CGEventType type,
                                  CGEventRef event,
                                  void *refcon);

extern NSString* ConvertUtil(NSString* str);

@interface OmnikeyManager ()

@end

@implementation OmnikeyManager {

}
static BOOL _isInited = NO;

static CFMachPortRef      eventTap;
static CGEventMask        eventMask;
static CFRunLoopSourceRef runLoopSource;
static CFRunLoopTimerRef watchdogTimer;

+(BOOL)isInited {
    return _isInited;
}

+(BOOL)initEventTap {
    if (_isInited)
        return true;
    
    //init modernKey
    OmnikeyInit();
    
    // Create an event tap. We are interested in key presses.
    eventMask = ((1 << kCGEventKeyDown) |
                 (1 << kCGEventKeyUp) |
                 (1 << kCGEventFlagsChanged) |
                 (1 << kCGEventLeftMouseDown) |
                 (1 << kCGEventRightMouseDown));
    
    eventTap = CGEventTapCreate(kCGSessionEventTap,
                                kCGHeadInsertEventTap,
                                0,
                                eventMask,
                                OmnikeyCallback,
                                NULL);
    
    if (!eventTap) {
        
        fprintf(stderr, "failed to create event tap\n");
        OmnikeyFree();
        return NO;
    }
    
    // Create a run loop source.
    runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
    if (runLoopSource == NULL) {
        CFMachPortInvalidate(eventTap);
        CFRelease(eventTap);
        eventTap = NULL;
        OmnikeyFree();
        return NO;
    }
    
    // Add to the current run loop.
    CFRunLoopAddSource(CFRunLoopGetMain(), runLoopSource, kCFRunLoopCommonModes);
    
    // Enable the event tap.
    CGEventTapEnable(eventTap, true);
    _isInited = YES;
    watchdogTimer = CFRunLoopTimerCreateWithHandler(kCFAllocatorDefault,
        CFAbsoluteTimeGetCurrent() + 0.5, 0.5, 0, 0, ^(CFRunLoopTimerRef timer) {
            if (eventTap != NULL && !CGEventTapIsEnabled(eventTap)) {
                OmnikeyReEnableEventTap();
            }
        });
    if (watchdogTimer != NULL) {
        CFRunLoopAddTimer(CFRunLoopGetMain(), watchdogTimer, kCFRunLoopCommonModes);
    }
    
    return YES;
}

// Called from OmnikeyCallback when macOS reports that it turned our tap off.
// Only this file owns `eventTap`, so re-enabling has to happen here.
void OmnikeyReEnableEventTap(void) {
    if (eventTap) {
        CGEventTapEnable(eventTap, true);
    }
}

+(BOOL)stopEventTap {
    if (watchdogTimer != NULL) {
        CFRunLoopTimerInvalidate(watchdogTimer);
        CFRelease(watchdogTimer);
        watchdogTimer = NULL;
    }
    if (_isInited) { //release all object
        CFRunLoopRemoveSource(CFRunLoopGetMain(), runLoopSource, kCFRunLoopCommonModes);
        CFRelease(runLoopSource);
        runLoopSource = nil;
        
        CFMachPortInvalidate(eventTap);
        CFRelease(eventTap);
        eventTap = nil;
        
        _isInited = false;
        OmnikeyFree();
    }
    return YES;
}

+(NSArray*)getTableCodes {
    return [[NSArray alloc] initWithObjects:
            @"Unicode",
            @"TCVN3 (ABC)",
            @"VNI Windows",
            @"Unicode tổ hợp",
            @"Vietnamese Locale CP 1258", nil];
}

+(NSString*)getBuildDate {
    return [NSString stringWithUTF8String:__DATE__];
}

#pragma mark -Convert feature
+(BOOL)quickConvert {
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSString *htmlString = [pasteboard stringForType:NSPasteboardTypeHTML];
    NSString *rawString = [pasteboard stringForType:NSPasteboardTypeString];
    bool converted = false;
    if (htmlString != nil) {
        htmlString = ConvertUtil(htmlString);
        converted = true;
    }
    if (rawString != nil) {
        rawString = ConvertUtil(rawString);
        converted = true;
    }
    if (converted) {
        [pasteboard clearContents];
        if (htmlString != nil)
            [pasteboard setString:htmlString forType:NSPasteboardTypeHTML];
        if (rawString != nil)
            [pasteboard setString:rawString forType:NSPasteboardTypeString];
        
        return YES;
    }
    return NO;
}

+(void)showMessage:(NSWindow*)window message:(NSString*)msg subMsg:(NSString*)subMsg {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:msg];
    [alert setInformativeText:subMsg];
    [alert addButtonWithTitle:@"OK"];
    if (window) {
        [alert beginSheetModalForWindow:window completionHandler:^(NSModalResponse returnCode) {
        }];
    } else {
        [alert runModal];
    }
}

#pragma mark -AutoUpdate feature

+(void)checkNewVersion:(NSWindow*)parent callbackFunc:(CheckNewVersionCallback) callback {
    NSURLSessionConfiguration *configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
    configuration.timeoutIntervalForRequest = 30;
    NSURLSession *session = [NSURLSession sessionWithConfiguration:configuration];
    NSURL *url = [NSURL URLWithString:@"https://raw.githubusercontent.com/jetaudio/Omnikey/master/version.json"];
    [[session dataTaskWithURL:url completionHandler:^(NSData *data, NSURLResponse *response, NSError *networkError) {
        NSError *parseError = nil;
        id object = data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:&parseError] : nil;
        NSDictionary *version = [object isKindOfClass:NSDictionary.class] ? object[@"latestVersion"] : nil;
        BOOL valid = !networkError && !parseError && ((NSHTTPURLResponse *)response).statusCode == 200 &&
            [version isKindOfClass:NSDictionary.class] && [version[@"versionCode"] respondsToSelector:@selector(integerValue)] &&
            [version[@"versionName"] isKindOfClass:NSString.class];
        dispatch_async(dispatch_get_main_queue(), ^{
            if (callback) callback();
            if (!valid) {
                if (callback) [self showMessage:parent message:@"Không kiểm tra được bản cập nhật" subMsg:@"Hãy kiểm tra kết nối mạng và thử lại."];
                return;
            }
            NSInteger current = [NSBundle.mainBundle.infoDictionary[@"CFBundleVersion"] integerValue];
            BOOL newer = [version[@"versionCode"] integerValue] > current;
            if (newer || callback) [self showUpdateMessage:parent needUpdating:newer newVersion:version[@"versionName"]];
        });
        [session finishTasksAndInvalidate];
    }] resume];
}

+(void)showUpdateMessage:(NSWindow*)parent needUpdating:(BOOL)needUpdating newVersion:(NSString*)versionString {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:(needUpdating ? [NSString stringWithFormat:@"Omnikey Có phiên bản mới (%@), bạn có muốn cập nhật không?", versionString] : @"Bạn đang dùng phiên bản mới nhất!")];
    [alert setInformativeText:(needUpdating ? @"Bấm 'Có' để tải và cài bản mới. Omnikey sẽ tự mở lại sau khi cập nhật." : @"")];
    
    if (!needUpdating) {
        [alert addButtonWithTitle:@"OK"];
    } else {
        [alert addButtonWithTitle:@"Có"];
        [alert addButtonWithTitle:@"Không"];
    }
    if (parent == nil) {
        [alert.window makeKeyAndOrderFront:nil];
        [alert.window setLevel:NSStatusWindowLevel];
        NSModalResponse res = [alert runModal];
        if (res == 1000 && needUpdating) {
            [self launchUpdateHelper];
        }
    } else {
        [alert beginSheetModalForWindow:parent completionHandler:^(NSModalResponse returnCode) {
            if (returnCode == 1000 && needUpdating) {
                [self launchUpdateHelper];
            }
        }];
    }
}

+(void)launchUpdateHelper {
    [OKUpdater start];
}

@end
