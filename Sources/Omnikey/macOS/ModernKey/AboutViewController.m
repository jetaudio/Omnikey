//
//  AboutViewController.m
//  OpenKey
//
//  Created by Tuyen on 2/15/19.
//  Copyright © 2019 Tuyen Mai. All rights reserved.
//

#import "AboutViewController.h"
#import "OmnikeyManager.h"
#import "OKFormUI.h"

static const CGFloat kAboutWidth = 400;

@interface AboutViewController ()

@end

@implementation AboutViewController {
    OKFormBuilder *form;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do view setup here.

    self.VersionInfo.stringValue = [NSString stringWithFormat:@"Phiên bản %@ (build %@) · Cập nhật %@",
                                    [[NSBundle mainBundle] objectForInfoDictionaryKey: @"CFBundleShortVersionString"],
                                    [[NSBundle mainBundle] objectForInfoDictionaryKey: @"CFBundleVersion"],
                                    [OmnikeyManager getBuildDate]] ;

    NSInteger dontCheckUpdate = [[NSUserDefaults standardUserDefaults] integerForKey:@"DontCheckUpdate"];
    self.CheckUpdateOnStatus.state = dontCheckUpdate ? NSControlStateValueOff :NSControlStateValueOn;

    [self buildLayout];
}

- (void)buildLayout {
    form = [[OKFormBuilder alloc] initWithWidth:kAboutWidth];
    NSArray<NSView *> *header = [form aboutHeaderWithVersionField:self.VersionInfo];

    NSStackView *links = [NSStackView stackViewWithViews:@[
        [OKFormBuilder pushButtonWithTitle:@"Trang chủ" target:self action:@selector(onHomePage:)],
        [OKFormBuilder pushButtonWithTitle:@"Bản phát hành" target:self action:@selector(onLatestReleaseVersion:)],
        [OKFormBuilder pushButtonWithTitle:@"Góp ý" target:self action:@selector(onFanPage:)],
    ]];
    links.spacing = 8;

    NSButton *checkButton = self.CheckNewVersionButton;
    [OKFormBuilder preparePushButton:checkButton];
    checkButton.title = @"Kiểm tra bản mới…";
    checkButton.translatesAutoresizingMaskIntoConstraints = NO;
    NSButton *checkOnStartup = self.CheckUpdateOnStatus;
    checkOnStartup.title = @"Tự kiểm tra khi khởi động";
    checkOnStartup.font = [NSFont systemFontOfSize:12];
    checkOnStartup.translatesAutoresizingMaskIntoConstraints = NO;

    NSTextField *copyright = [form labelWithString:@"Omnikey · mã nguồn mở theo GPL-3.0" font:[NSFont systemFontOfSize:11] color:[NSColor tertiaryLabelColor]];
    copyright.alignment = NSTextAlignmentCenter;
    [copyright setContentHuggingPriority:NSLayoutPriorityDefaultHigh forOrientation:NSLayoutConstraintOrientationHorizontal];

    NSStackView *stack = [NSStackView stackViewWithViews:[header arrayByAddingObjectsFromArray:@[links, checkButton, checkOnStartup, copyright]]];
    stack.orientation = NSUserInterfaceLayoutOrientationVertical;
    stack.alignment = NSLayoutAttributeCenterX;
    stack.spacing = 4;
    stack.edgeInsets = NSEdgeInsetsMake(24, 28, 22, 28);
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    [stack setCustomSpacing:12 afterView:header[0]];
    [stack setCustomSpacing:8 afterView:header[2]];
    [stack setCustomSpacing:22 afterView:header[3]];
    [stack setCustomSpacing:14 afterView:links];
    [stack setCustomSpacing:8 afterView:checkButton];
    [stack setCustomSpacing:20 afterView:checkOnStartup];

    // Controls were moved into the stack; drop the storyboard layout.
    for (NSView *view in [self.view.subviews copy]) {
        [view removeFromSuperview];
    }
    [self.view addSubview:stack];
    [NSLayoutConstraint activateConstraints:@[
        [stack.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [stack.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [stack.topAnchor constraintEqualToAnchor:self.view.topAnchor],
        [stack.widthAnchor constraintEqualToConstant:kAboutWidth],
    ]];
    [self.view layoutSubtreeIfNeeded];
    [self.view setFrameSize:NSMakeSize(kAboutWidth, ceil(stack.fittingSize.height))];
}

- (void)viewWillAppear {
    [super viewWillAppear];
    self.view.window.title = @"Giới thiệu Omnikey";
}

- (IBAction)onHomePage:(id)sender {
    [[NSWorkspace sharedWorkspace] openURL: [NSURL URLWithString:@"https://github.com/jetaudio/Omnikey"]];
}

- (IBAction)onFanPage:(id)sender {
    [[NSWorkspace sharedWorkspace] openURL: [NSURL URLWithString:@"https://github.com/jetaudio/Omnikey/issues"]];
}

- (IBAction)onLatestReleaseVersion:(id)sender {
    [[NSWorkspace sharedWorkspace] openURL: [NSURL URLWithString:@"https://github.com/jetaudio/Omnikey/releases"]];
}

- (IBAction)onCheckUpdateOnStartup:(NSButton *)sender {
    NSInteger val = sender.state == NSControlStateValueOn ? 0 : 1;
    [[NSUserDefaults standardUserDefaults] setInteger:val forKey:@"DontCheckUpdate"];
}

- (IBAction)onCheckNewVersion:(id)sender {

    self.CheckNewVersionButton.title = @"Đang kiểm tra…";
    self.CheckNewVersionButton.enabled = false;

    [OmnikeyManager checkNewVersion: self.view.window callbackFunc:^{
        self.CheckNewVersionButton.enabled = true;
        self.CheckNewVersionButton.title = @"Kiểm tra bản mới…";
    }];
}

@end
