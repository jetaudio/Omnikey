//
//  OmnikeyManager.m
//  OpenKey
//
//  Created by Tuyen on 1/27/19.
//  Copyright © 2019 Tuyen Mai. All rights reserved.
//

#import "OmnikeyManager.h"

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
    //load new version config
    NSURLSession *aSession = [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration defaultSessionConfiguration]];
    [[aSession dataTaskWithURL:[NSURL URLWithString:@"https://raw.githubusercontent.com/jetaudio/Omnikey/master/version.json"] completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        if (((NSHTTPURLResponse *)response).statusCode == 200) {
            if (data) {
                if(NSClassFromString(@"NSJSONSerialization")) {
                    NSError *error = nil;
                    id object = [NSJSONSerialization
                                 JSONObjectWithData:data
                                 options:0
                                 error:&error];
                    
                    if(error) {  }
                    if([object isKindOfClass:[NSDictionary class]]) {
                        NSDictionary *results = object;
                        NSDictionary *ver = [results valueForKey:@"latestVersion"];
                        NSString* versionCodeString = [ver valueForKey:@"versionCode"];
                        int versionCode = (int)[versionCodeString integerValue];
                        int currentVersionCode = (int)[((NSString*)[[NSBundle mainBundle] objectForInfoDictionaryKey: @"CFBundleVersion"]) integerValue];
                        
                        dispatch_async(dispatch_get_main_queue(), ^{
                            if (callback != nil) {
                                callback();
                            }
                            if (versionCode > currentVersionCode || callback != nil) {
                                [self showUpdateMessage:parent needUpdating:versionCode > currentVersionCode newVersion:[ver valueForKey:@"versionName"]];
                            }
                        });
                    }
                    else {
                        //oh my god
                    }
                }
                else {
                    //can not parse json
                }
            }
        }
    }] resume];
}

+(void)showUpdateMessage:(NSWindow*)parent needUpdating:(BOOL)needUpdating newVersion:(NSString*)versionString {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:(needUpdating ? [NSString stringWithFormat:@"Omnikey Có phiên bản mới (%@), bạn có muốn cập nhật không?", versionString] : @"Bạn đang dùng phiên bản mới nhất!")];
    [alert setInformativeText:(needUpdating ? @"Bấm 'Có' để cập nhật Omnikey." : @"")];
    
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
    // The app bundles no update helper: open the release page to download
    // the new DMG instead of quitting.
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"https://github.com/jetaudio/Omnikey/releases/latest"]];
}

@end
