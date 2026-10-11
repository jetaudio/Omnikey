//
//  ConvertToolViewController.mm
//  OpenKey
//
//  Created by Tuyen on 9/4/19.
//  Copyright © 2019 Tuyen Mai. All rights reserved.
//

#import "AppDelegate.h"
#import "ConvertToolViewController.h"
#import "OmnikeyManager.h"
#import "ConvertTool.h"
#import "OKFormUI.h"

static const CGFloat kConvertWidth = 520;

extern AppDelegate* appDelegate;

@interface ConvertToolViewController ()

@end

@implementation ConvertToolViewController {
    OKFormBuilder *form;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    self.SHotKey.Parent = self;
    [self fillData];
    [self buildLayout];
}

- (void)buildLayout {
    form = [[OKFormBuilder alloc] initWithWidth:kConvertWidth];
    NSStackView *stack = [form pageStack];
    
    [form prepareInlinePopup:self.FromCode];
    [form prepareInlinePopup:self.ToCode];
    NSButton *reverse = self.ReverseCode;
    reverse.translatesAutoresizingMaskIntoConstraints = NO;
    reverse.bezelStyle = NSBezelStylePush;
    reverse.toolTip = @"Đảo bảng mã nguồn và đích";
    if (@available(macOS 11.0, *)) {
        reverse.image = [NSImage imageWithSystemSymbolName:@"arrow.left.arrow.right" accessibilityDescription:@"Đảo chiều"];
        reverse.imagePosition = NSImageOnly;
    }
    NSStackView *codes = [NSStackView stackViewWithViews:@[self.FromCode, reverse, self.ToCode]];
    codes.spacing = 6;
    [form addSection:nil rows:@[
        [form rowWithTitle:@"Bảng mã" detail:@"Nguồn → đích" accessory:codes],
    ] note:@"Văn bản trong Clipboard được chuyển mã và ghi lại vào Clipboard." toStack:stack];
    
    [form addSection:@"Tuỳ chọn" rows:@[
        [form toggleRowForButton:self.ToAllCaps title:@"Chuyển sang CHỮ HOA" detail:nil],
        [form toggleRowForButton:self.ToNonCaps title:@"Chuyển sang chữ thường" detail:nil],
        [form toggleRowForButton:self.ToCapsFirstLetter title:@"Viết hoa chữ cái đầu câu" detail:nil],
        [form toggleRowForButton:self.ToCapsCharEachWord title:@"Viết Hoa Chữ Cái Đầu Mỗi Từ" detail:nil],
        [form toggleRowForButton:self.ToRemoveSign title:@"Loại bỏ dấu" detail:@"Ví dụ: Tiếng Việt → Tieng Viet"],
    ] note:@"Chỉ dùng được một kiểu chữ hoa/thường mỗi lần." toStack:stack];
    
    NSArray<NSButton *> *modifiers = @[self.SControl, self.SOption, self.SCommand, self.SShift];
    [modifiers enumerateObjectsUsingBlock:^(NSButton *button, NSUInteger i, BOOL *stop) {
        button.toolTip = @[@"Phím Control", @"Phím Option", @"Phím Command", @"Phím Shift"][i];
    }];
    NSSegmentedControl *modifierControl = [form modifierControlForButtons:modifiers labels:@[@"⌃", @"⌥", @"⌘", @"⇧"]];
    [form prepareKeyField:self.SHotKey];
    NSTextField *plus = [form labelWithString:@"+" font:[NSFont systemFontOfSize:13] color:[NSColor secondaryLabelColor]];
    NSStackView *shortcut = [NSStackView stackViewWithViews:@[modifierControl, plus, self.SHotKey]];
    shortcut.spacing = 8;
    [form addSection:@"Chuyển mã nhanh" rows:@[
        [form rowWithTitle:@"Phím tắt" detail:@"Chuyển mã Clipboard từ bất cứ đâu" accessory:shortcut],
        [form toggleRowForButton:self.AlertWhenComplete title:@"Thông báo khi chuyển xong" detail:nil],
    ] note:nil toStack:stack];
    
    NSButton *close = [OKFormBuilder pushButtonWithTitle:@"Đóng" target:self action:@selector(onOKButton:)];
    close.keyEquivalent = @"\033";
    NSButton *convert = [OKFormBuilder pushButtonWithTitle:@"Chuyển mã" target:self action:@selector(onConvertButton:)];
    convert.keyEquivalent = @"\r";
    NSStackView *actions = [NSStackView stackViewWithViews:@[close, convert]];
    actions.spacing = 10;
    NSView *actionBar = [[NSView alloc] init];
    actionBar.translatesAutoresizingMaskIntoConstraints = NO;
    actions.translatesAutoresizingMaskIntoConstraints = NO;
    [actionBar addSubview:actions];
    [NSLayoutConstraint activateConstraints:@[
        [actions.trailingAnchor constraintEqualToAnchor:actionBar.trailingAnchor],
        [actions.topAnchor constraintEqualToAnchor:actionBar.topAnchor],
        [actions.bottomAnchor constraintEqualToAnchor:actionBar.bottomAnchor],
    ]];
    [stack setCustomSpacing:20 afterView:stack.arrangedSubviews.lastObject];
    [stack addArrangedSubview:actionBar];
    
    // Controls were moved into the form; drop the storyboard layout.
    for (NSView *view in [self.view.subviews copy]) {
        [view removeFromSuperview];
    }
    [self.view addSubview:stack];
    [NSLayoutConstraint activateConstraints:@[
        [stack.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [stack.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [stack.topAnchor constraintEqualToAnchor:self.view.topAnchor],
        [stack.widthAnchor constraintEqualToConstant:kConvertWidth],
    ]];
    [self.view layoutSubtreeIfNeeded];
    [self.view setFrameSize:NSMakeSize(kConvertWidth, ceil(stack.fittingSize.height))];
}

-(void)fillData {
    NSArray* codeData = [OmnikeyManager getTableCodes];
    [self.FromCode removeAllItems];
    [self.FromCode addItemsWithTitles:codeData];
    [self.ToCode removeAllItems];
    [self.ToCode addItemsWithTitles:codeData];
    
    self.AlertWhenComplete.state = !convertToolDontAlertWhenCompleted ? NSControlStateValueOn : NSControlStateValueOff;
    
    self.ToAllCaps.state = convertToolToAllCaps ? NSControlStateValueOn : NSControlStateValueOff;
    self.ToNonCaps.state = convertToolToAllNonCaps ? NSControlStateValueOn : NSControlStateValueOff;
    self.ToCapsFirstLetter.state = convertToolToCapsFirstLetter ? NSControlStateValueOn : NSControlStateValueOff;
    self.ToCapsCharEachWord.state = convertToolToCapsEachWord ? NSControlStateValueOn : NSControlStateValueOff;
    
    self.ToRemoveSign.state = convertToolRemoveMark ? NSControlStateValueOn : NSControlStateValueOff;
    
    [self.FromCode selectItemAtIndex:convertToolFromCode];
    [self.ToCode selectItemAtIndex:convertToolToCode];
    
    self.SControl.state = (convertToolHotKey & 0x100) ? NSControlStateValueOn : NSControlStateValueOff;
    self.SOption.state = (convertToolHotKey & 0x200) ? NSControlStateValueOn : NSControlStateValueOff;
    self.SCommand.state = (convertToolHotKey & 0x400) ? NSControlStateValueOn : NSControlStateValueOff;
    self.SShift.state = (convertToolHotKey & 0x800) ? NSControlStateValueOn : NSControlStateValueOff;
    [self.SHotKey setTextByChar:((convertToolHotKey>>24) & 0xFF)];
}

-(void)turnOffAllOption {
    convertToolToAllCaps = convertToolToAllNonCaps = convertToolToCapsFirstLetter = convertToolToCapsEachWord = false;
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    for (NSString *key in @[@"convertToolToAllCaps", @"convertToolToAllNonCaps", @"convertToolToCapsFirstLetter", @"convertToolToCapsEachWord"])
        [defaults setInteger:0 forKey:key];
}

- (IBAction)onAlertWhenCompleted:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"convertToolDontAlertWhenCompleted"];
    convertToolDontAlertWhenCompleted = (int)!val;
}

- (IBAction)onToAllCaps:(NSButton *)sender {
    [self turnOffAllOption];
    NSInteger val = [self setCustomValue:sender keyToSet:@"convertToolToAllCaps"];
    convertToolToAllCaps = (int)val;
    [self fillData];
}

- (IBAction)onToNonCaps:(NSButton *)sender {
    [self turnOffAllOption];
    NSInteger val = [self setCustomValue:sender keyToSet:@"convertToolToAllNonCaps"];
    convertToolToAllNonCaps = (int)val;
    [self fillData];
}

- (IBAction)onToCapsFirstLetter:(NSButton *)sender {
    [self turnOffAllOption];
    NSInteger val = [self setCustomValue:sender keyToSet:@"convertToolToCapsFirstLetter"];
    convertToolToCapsFirstLetter = (int)val;
    [self fillData];
}

- (IBAction)onToCapsCharEachWord:(NSButton *)sender {
    [self turnOffAllOption];
    NSInteger val = [self setCustomValue:sender keyToSet:@"convertToolToCapsEachWord"];
    convertToolToCapsEachWord = (int)val;
    [self fillData];
}

- (IBAction)onToRemoveSign:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"convertToolRemoveMark"];
    convertToolRemoveMark = (int)val;
}

- (IBAction)onFromCodeSelected:(NSPopUpButton *)sender {
    convertToolFromCode = [self.FromCode indexOfSelectedItem];
    [[NSUserDefaults standardUserDefaults] setInteger:convertToolFromCode forKey:@"convertToolFromCode"];
}

- (IBAction)onToCodeSelected:(NSPopUpButton *)sender {
    convertToolToCode = [self.ToCode indexOfSelectedItem];
    [[NSUserDefaults standardUserDefaults] setInteger:convertToolToCode forKey:@"convertToolToCode"];
}

- (NSInteger)setCustomValue:(NSButton*)sender keyToSet:(NSString*) key {
    NSInteger val = sender.state == NSControlStateValueOn ? 1 : 0;
    if (key != nil)
        [[NSUserDefaults standardUserDefaults] setInteger:val forKey:key];
    return val;
}

- (IBAction)onReverseCode:(id)sender {
    NSInteger code = [self.ToCode indexOfSelectedItem];
    [self.ToCode selectItemAtIndex:[self.FromCode indexOfSelectedItem]];
    [self.FromCode selectItemAtIndex:code];
    convertToolFromCode = [self.FromCode indexOfSelectedItem];
    convertToolToCode = [self.ToCode indexOfSelectedItem];
}

- (IBAction)onSControl:(NSButton *)sender {
    [self setHotKeyBit:0x100 fromButton:sender];
}

- (IBAction)onSOption:(NSButton *)sender {
    [self setHotKeyBit:0x200 fromButton:sender];
}

- (IBAction)onSCommand:(NSButton *)sender {
    [self setHotKeyBit:0x400 fromButton:sender];
}

- (IBAction)onSShift:(NSButton *)sender {
    [self setHotKeyBit:0x800 fromButton:sender];
}

// Sets one modifier of the quick convert hotkey from a checkbox and saves it.
- (void)setHotKeyBit:(int)bit fromButton:(NSButton *)sender {
    if (sender.state == NSControlStateValueOn) convertToolHotKey |= bit;
    else convertToolHotKey &= ~bit;
    [[NSUserDefaults standardUserDefaults] setInteger:convertToolHotKey forKey:@"convertToolHotKey"];
    [appDelegate setQuickConvertString];
}

-(void)onMyTextFieldKeyChange:(unsigned short)keyCode character:(unsigned short)character {
    convertToolHotKey &= 0xFFFFFF00;
    convertToolHotKey |= keyCode;
    convertToolHotKey &= 0x00FFFFFF;
    convertToolHotKey |= ((unsigned int)character<<24);
    [[NSUserDefaults standardUserDefaults] setInteger:convertToolHotKey forKey:@"convertToolHotKey"];
    [appDelegate setQuickConvertString];
}

- (IBAction)onConvertButton:(id)sender {
    if ([OmnikeyManager quickConvert]) {
        if (!convertToolDontAlertWhenCompleted) {
            [OmnikeyManager showMessage: self.view.window message:@"Chuyển mã thành công!" subMsg:@"Kết quả đã được lưu trong clipboard."];
        }
    } else {
        [OmnikeyManager showMessage: self.view.window message:@"Không có dữ liệu trong clipboard!" subMsg:@"Hãy sao chép một đoạn text để chuyển đổi!"];
    }
}

- (IBAction)onOKButton:(id)sender {
    [self.view.window close];
}


@end
