//
//  ViewController.m
//  ModernKey
//
//  Created by Tuyen on 1/18/19.
//  Copyright © 2019 Tuyen Mai. All rights reserved.
//

#import "ViewController.h"
#import "OmnikeyManager.h"
#import "AppDelegate.h"
#import "MyTextField.h"
#import "OKFormUI.h"

extern AppDelegate* appDelegate;
extern void OnSpellCheckingChanged(void);

ViewController* viewController;
extern int vFreeMark;
extern int vCheckSpelling;
extern int vUseModernOrthography;
extern int vSwitchKeyStatus;
extern int vQuickTelex;
extern int vRestoreIfWrongSpelling;
extern int vFixRecommendBrowser;
extern int vUseMacro;
extern int vUseMacroInEnglishMode;
extern int vSendKeyStepByStep;
extern int vUseSmartSwitchKey;
extern int vUpperCaseFirstChar;
extern int vTempOffSpelling;
extern int vAllowConsonantZFWJ;
extern int vQuickStartConsonant;
extern int vQuickEndConsonant;
extern int vRememberCode;
extern int vOtherLanguage;
extern int vTempOffOmnikey;
extern int vShowIconOnDock;
extern int vAutoCapsMacro;
extern int vFixChromiumBrowser;
extern int vPerformLayoutCompat;

static const CGFloat kSettingsWidth = 620;
static const CGFloat kSettingsMaxHeight = 640;

@interface ViewController () <NSToolbarDelegate>
@end

@implementation ViewController {
    __weak IBOutlet NSButton *CustomSwitchCommand;
    __weak IBOutlet NSButton *CustomSwitchOption;
    __weak IBOutlet NSButton *CustomSwitchControl;
    __weak IBOutlet NSButton *CustomSwitchShift;
    __weak IBOutlet NSButton *CustomSwitchFn;
    __weak IBOutlet MyTextField *CustomSwitchKey;
    __weak IBOutlet NSButton *CustomBeepSound;
    NSArray<NSScrollView *> *pages;
    NSArray<NSString *> *pageTitles;
    NSArray<NSString *> *pageIdentifiers;
    OKFormBuilder *form;
    NSSegmentedControl *languageControl;
    NSInteger currentTab;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    viewController = self;
    CustomSwitchKey.Parent = self;

    NSArray* inputTypeData = [[NSArray alloc] initWithObjects:@"Telex", @"VNI", @"Simple Telex 1", @"Simple Telex 2", nil];
    NSArray* codeData = [OmnikeyManager getTableCodes];

    //preset data
    [_popupInputType removeAllItems];
    [_popupInputType addItemsWithTitles:inputTypeData];

    [self.popupCode removeAllItems];
    [self.popupCode addItemsWithTitles:codeData];

    // set version info
    self.VersionInfo.stringValue = [NSString stringWithFormat:@"Phiên bản %@ (build %@) · Cập nhật %@",
    [[NSBundle mainBundle] objectForInfoDictionaryKey: @"CFBundleShortVersionString"],
    [[NSBundle mainBundle] objectForInfoDictionaryKey: @"CFBundleVersion"],
    [OmnikeyManager getBuildDate]] ;

    [self buildSettingsPages];

    [self initKey];

    [self fillData];

    [self showTab:0];
}

- (void)viewWillAppear {
    [super viewWillAppear];
    [self installToolbarIfNeeded];
    [self initKey];
}

- (void)viewDidAppear {
    [super viewDidAppear];
    self.view.window.title = pageTitles[currentTab];
}

-(void)initKey {
    dispatch_async(dispatch_get_main_queue(), ^{
        [OmnikeyManager initEventTap];
    });
}

- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];

    // Update the view, if already loaded.
}

#pragma mark - Settings layout

- (NSScrollView *)buildGeneralPage {
    NSStackView *stack = [form pageStack];

    languageControl = [NSSegmentedControl segmentedControlWithLabels:@[@"Tiếng Việt", @"English", @"中文"]
                                                        trackingMode:NSSegmentSwitchTrackingSelectOne
                                                              target:self
                                                              action:@selector(onLanguageSegment:)];
    [languageControl setToolTip:@"Chế độ gõ Tiếng Việt" forSegment:0];
    [languageControl setToolTip:@"Chế độ gõ Tiếng Anh" forSegment:1];
    [languageControl setToolTip:@"Gõ tiếng Trung bằng Pinyin" forSegment:2];
    [form prepareInlinePopup:self.popupInputType];
    [form prepareInlinePopup:self.popupCode];
    [form addSection:nil rows:@[
        [form rowWithTitle:@"Chế độ gõ" detail:nil accessory:languageControl],
        [form rowWithTitle:@"Kiểu gõ" detail:nil accessory:self.popupInputType],
        [form rowWithTitle:@"Bảng mã" detail:@"Thường dùng Unicode" accessory:self.popupCode],
    ] note:nil toStack:stack];

    // Modifier keys of the switching shortcut, shown as one multi-select control.
    NSArray<NSButton *> *modifiers = @[CustomSwitchControl, CustomSwitchOption, CustomSwitchCommand, CustomSwitchShift, CustomSwitchFn];
    NSSegmentedControl *modifierControl = [form modifierControlForButtons:modifiers labels:@[@"⌃", @"⌥", @"⌘", @"⇧", @"🌐"]];
    MyTextField *keyField = CustomSwitchKey;
    [form prepareKeyField:keyField];
    NSTextField *plus = [form labelWithString:@"+" font:[NSFont systemFontOfSize:13] color:[NSColor secondaryLabelColor]];
    NSStackView *shortcut = [NSStackView stackViewWithViews:@[modifierControl, plus, keyField]];
    shortcut.spacing = 8;

    [form addSection:@"Phím chuyển chế độ" rows:@[
        [form rowWithTitle:@"Phím tắt" detail:@"Xoay vòng Tiếng Việt → English → 中文" accessory:shortcut],
        [form toggleRowForButton:CustomBeepSound title:@"Kêu beep khi chuyển chế độ"
                          detail:@"Không áp dụng với chuyển chế độ thông minh"],
    ] note:@"Để dùng phím 🌐, vào Cài đặt Hệ thống → Bàn phím và đặt “Nhấn phím 🌐” thành “Không làm gì”." toStack:stack];

    [form addSection:@"Chuyển chế độ thông minh" rows:@[
        [form toggleRowForButton:self.AutoRememberSwitchKey title:@"Chuyển chế độ thông minh"
                          detail:@"Tự ghi nhớ chế độ gõ theo từng ứng dụng"],
        [form toggleRowForButton:self.RememberTableCode title:@"Tự ghi nhớ bảng mã theo ứng dụng" detail:nil],
        [form toggleRowForButton:(NSButton *)self.OtherLanguage.controlView title:@"Tắt tiếng Việt khi bộ gõ hệ thống khác tiếng Anh"
                          detail:nil],
    ] note:nil toStack:stack];
    return [form scrollPageWithStack:stack];
}

- (NSScrollView *)buildTypingPage {
    NSStackView *stack = [form pageStack];
    [form addSection:@"Chính tả" rows:@[
        [form toggleRowForButton:self.CheckSpellingButton title:@"Kiểm tra chính tả"
                          detail:@"Hạn chế gõ sai từ tiếng Việt"],
        [form toggleRowForButton:self.RestoreIfInvalidWord title:@"Tự khôi phục phím với từ sai"
                          detail:@"Từ không đúng chính tả tiếng Việt được trả lại các phím đã gõ"],
        [form toggleRowForButton:self.AllowZWJF title:@"Cho phép “z w j f” làm phụ âm đầu" detail:nil],
    ] note:nil toStack:stack];
    [form addSection:@"Dấu và chữ hoa" rows:@[
        [form toggleRowForButton:self.UseModernOrthography title:@"Đặt dấu kiểu mới: oà, uý"
                          detail:@"Thay vì kiểu cũ òa, úy"],
        [form toggleRowForButton:self.UpperCaseFirstChar title:@"Viết hoa chữ cái đầu câu"
                          detail:@"Sau dấu chấm câu và khi xuống dòng"],
    ] note:nil toStack:stack];
    [form addSection:@"Phím tạm tắt" rows:@[
        [form toggleRowForButton:self.TempOffSpellChecking title:@"Tạm tắt chính tả bằng phím ⌃"
                          detail:@"Cho từ như Đắk Lắk, Krông…; tự bật lại ở từ tiếp theo"],
        [form toggleRowForButton:self.TempOffOmnikey title:@"Tạm tắt Omnikey bằng phím ⌘"
                          detail:@"Tạm ngừng cho tới khi bạn gõ một từ mới"],
    ] note:nil toStack:stack];
    return [form scrollPageWithStack:stack];
}

- (NSScrollView *)buildMacroPage {
    NSStackView *stack = [form pageStack];
    [form addSection:@"Gõ tắt" rows:@[
        [form toggleRowForButton:self.UseMacro title:@"Cho phép gõ tắt"
                          detail:@"Tiết kiệm thời gian gõ các cụm từ thường dùng"],
        [form toggleRowForButton:self.UseMacroInEnglishMode title:@"Gõ tắt cả khi tắt tiếng Việt" detail:nil],
        [form toggleRowForButton:self.AutoCapsMacro title:@"Tự động viết hoa theo phím tắt" detail:nil],
        [form rowWithTitle:@"Bảng gõ tắt" detail:@"Quản lý danh sách từ gõ tắt"
                 accessory:[OKFormBuilder pushButtonWithTitle:@"Chỉnh sửa…" target:self action:@selector(onMacroButton:)]],
    ] note:nil toStack:stack];
    [form addSection:@"Gõ nhanh" rows:@[
        [form toggleRowForButton:self.QuickTelex title:@"Gõ nhanh phụ âm"
                          detail:@"cc→ch, gg→gi, kk→kh, nn→ng, qq→qu, pp→ph, tt→th"],
        [form toggleRowForButton:self.QuickStartConsonant title:@"Gõ tắt phụ âm đầu"
                          detail:@"f→ph, j→gi, w→qu  ·  fải→phải, jảng→giảng"],
        [form toggleRowForButton:self.QuickEndConsonant title:@"Gõ tắt phụ âm cuối"
                          detail:@"g→ng, h→nh, k→ch  ·  nhah→nhanh, bák→bách"],
    ] note:nil toStack:stack];
    return [form scrollPageWithStack:stack];
}

- (NSScrollView *)buildSystemPage {
    NSStackView *stack = [form pageStack];
    [form addSection:@"Khởi động" rows:@[
        [form toggleRowForButton:self.RunOnStartupButton title:@"Khởi động cùng macOS" detail:nil],
        [form toggleRowForButton:self.ShowUIButton title:@"Mở cửa sổ này khi khởi động" detail:nil],
        [form toggleRowForButton:self.CheckNewVersionOnStartup title:@"Kiểm tra bản mới khi khởi động" detail:nil],
    ] note:nil toStack:stack];
    [form addSection:@"Giao diện" rows:@[
        [form toggleRowForButton:self.ShowIconOnDock title:@"Hiện biểu tượng trên Dock" detail:nil],
        [form toggleRowForButton:self.UseGrayIcon title:@"Biểu tượng hiện đại trên thanh menu"
                          detail:@"Biểu tượng đơn sắc, hợp với Dark Mode"],
    ] note:nil toStack:stack];
    [form addSection:@"Tương thích" rows:@[
        [form toggleRowForButton:self.FixRecommendBrowser title:@"Sửa lỗi gợi ý"
                          detail:@"Tránh lặp chữ trên thanh địa chỉ trình duyệt, Excel…"],
        [form toggleRowForButton:self.FixChromiumBrowser title:@"Sửa lỗi trên Chromium (beta)" detail:nil],
        [form toggleRowForButton:self.SendKeyStepByStep title:@"Gửi từng phím"
                          detail:@"Mặc định nên tắt; chỉ bật khi ứng dụng gõ bị lỗi"],
        [form toggleRowForButton:self.PerformLayoutCompat title:@"Tương thích Telex trên layout khác"
                          detail:@"Dvorak, Colemak…"],
    ] note:nil toStack:stack];

    NSButton *checkButton = self.CheckNewVersionButton;
    [OKFormBuilder preparePushButton:checkButton];
    checkButton.title = @"Kiểm tra bản mới…";
    [form addSection:@"Omnikey" rows:@[
        [form rowWithTitle:@"Cập nhật" detail:nil accessory:checkButton],
        [form rowWithTitle:@"Khôi phục cài đặt mặc định" detail:nil
                 accessory:[OKFormBuilder pushButtonWithTitle:@"Khôi phục…" target:self action:@selector(onDefaultConfig:)]],
        [form rowWithTitle:@"Thoát Omnikey" detail:@"Ngừng bộ gõ và ẩn biểu tượng trên thanh menu"
                 accessory:[OKFormBuilder pushButtonWithTitle:@"Thoát" target:self action:@selector(onTerminateApp:)]],
    ] note:nil toStack:stack];
    return [form scrollPageWithStack:stack];
}

- (NSScrollView *)buildAboutPage {
    NSArray<NSView *> *header = [form aboutHeaderWithVersionField:self.VersionInfo];
    NSTextField *about = [form labelWithString:@"Omnikey cho macOS là phần mềm mã nguồn mở, phát hành miễn phí. Bạn có thể góp ý và đề xuất tính năng mới qua email của tác giả hoặc trên GitHub."
                                          font:[NSFont systemFontOfSize:12] color:[NSColor secondaryLabelColor]];
    about.alignment = NSTextAlignmentCenter;
    about.preferredMaxLayoutWidth = 420;

    NSStackView *links = [NSStackView stackViewWithViews:@[
        [OKFormBuilder pushButtonWithTitle:@"Trang chủ" target:self action:@selector(onHomePageLink:)],
        [OKFormBuilder pushButtonWithTitle:@"Facebook" target:self action:@selector(onFanpageLink:)],
        [OKFormBuilder pushButtonWithTitle:@"Mã nguồn" target:self action:@selector(onSourceCode:)],
    ]];
    links.spacing = 8;

    NSTextField *copyright = [form labelWithString:@"© 2019 Mai Vũ Tuyên" font:[NSFont systemFontOfSize:11] color:[NSColor tertiaryLabelColor]];
    NSStackView *footer = [NSStackView stackViewWithViews:@[copyright,
        [OKFormBuilder linkButtonWithTitle:@"maivutuyen.91@gmail.com" target:self action:@selector(onEmailLink:)]]];
    footer.spacing = 6;

    for (NSTextField *label in @[about, copyright]) {
        label.alignment = NSTextAlignmentCenter;
        [label setContentHuggingPriority:NSLayoutPriorityDefaultHigh forOrientation:NSLayoutConstraintOrientationHorizontal];
    }
    
    NSStackView *stack = [NSStackView stackViewWithViews:[header arrayByAddingObjectsFromArray:@[about, links, footer]]];
    stack.orientation = NSUserInterfaceLayoutOrientationVertical;
    stack.alignment = NSLayoutAttributeCenterX;
    stack.spacing = 4;
    stack.edgeInsets = NSEdgeInsetsMake(28, 24, 24, 24);
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    [stack setCustomSpacing:12 afterView:header[0]];
    [stack setCustomSpacing:8 afterView:header[2]];
    [stack setCustomSpacing:18 afterView:header[3]];
    [stack setCustomSpacing:20 afterView:about];
    [stack setCustomSpacing:24 afterView:links];
    return [form scrollPageWithStack:stack];
}

- (void)buildSettingsPages {
    form = [[OKFormBuilder alloc] initWithWidth:kSettingsWidth];
    pageTitles = @[@"Chung", @"Bộ gõ", @"Gõ tắt", @"Hệ thống", @"Thông tin"];
    pageIdentifiers = @[@"general", @"typing", @"macro", @"system", @"about"];
    // Controls are moved into the new pages before the storyboard layout is
    // discarded, so their weak outlets stay valid.
    pages = @[[self buildGeneralPage], [self buildTypingPage], [self buildMacroPage],
              [self buildSystemPage], [self buildAboutPage]];
    for (NSView *view in [self.view.subviews copy]) {
        [view removeFromSuperview];
    }
}

- (CGFloat)heightForPage:(NSScrollView *)page {
    NSView *document = page.documentView;
    [document layoutSubtreeIfNeeded];
    CGFloat maxHeight = kSettingsMaxHeight;
    NSScreen *screen = self.view.window.screen ?: [NSScreen mainScreen];
    if (screen != nil) maxHeight = MIN(maxHeight, NSHeight(screen.visibleFrame) - 140);
    return MIN(ceil(document.fittingSize.height), maxHeight);
}

- (void)resizeWindowForTab:(NSInteger)index animate:(BOOL)animate {
    CGFloat height = [self heightForPage:pages[index]];
    NSWindow *window = self.view.window;
    if (window == nil) {
        [self.view setFrameSize:NSMakeSize(kSettingsWidth, height)];
        return;
    }
    NSRect frame = window.frame;
    NSSize content = window.contentView.frame.size;
    frame.size.width += kSettingsWidth - content.width;
    frame.size.height += height - content.height;
    frame.origin.y = NSMaxY(window.frame) - NSHeight(frame);
    [window setFrame:frame display:YES animate:animate && window.isVisible];
}

-(void)showTab:(NSInteger)index {
    currentTab = index;
    NSScrollView *page = pages[index];
    for (NSScrollView *other in pages) {
        if (other != page) [other removeFromSuperview];
    }
    if (page.superview == nil) {
        [self.view addSubview:page];
        [NSLayoutConstraint activateConstraints:@[
            [page.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
            [page.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
            [page.topAnchor constraintEqualToAnchor:self.view.topAnchor],
            [page.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
        ]];
    }
    [page.contentView scrollToPoint:NSZeroPoint];
    [page reflectScrolledClipView:page.contentView];

    NSWindow *window = self.view.window;
    window.toolbar.selectedItemIdentifier = pageIdentifiers[index];
    if (window != nil) window.title = pageTitles[index];
    [self resizeWindowForTab:index animate:YES];
}

- (IBAction)onTabButton:(NSButton *)sender {
    [self showTab:sender.tag];
}

- (void)onToolbarItem:(NSToolbarItem *)sender {
    NSUInteger index = [pageIdentifiers indexOfObject:sender.itemIdentifier];
    if (index != NSNotFound && (NSInteger)index != currentTab) [self showTab:index];
}

- (void)installToolbarIfNeeded {
    NSWindow *window = self.view.window;
    if (window == nil || window.toolbar != nil) return;
    NSToolbar *toolbar = [[NSToolbar alloc] initWithIdentifier:@"OmnikeySettings"];
    toolbar.delegate = self;
    toolbar.displayMode = NSToolbarDisplayModeIconAndLabel;
    toolbar.allowsUserCustomization = NO;
    if (@available(macOS 11.0, *)) {
        window.toolbarStyle = NSWindowToolbarStylePreference;
    }
    window.toolbar = toolbar;
    toolbar.selectedItemIdentifier = pageIdentifiers[currentTab];
    window.title = pageTitles[currentTab];
    [self resizeWindowForTab:currentTab animate:NO];
}

- (NSImage *)toolbarImageForIndex:(NSUInteger)index {
    if (@available(macOS 11.0, *)) {
        NSArray *symbols = @[@"gearshape", @"keyboard", @"text.badge.plus", @"desktopcomputer", @"info.circle"];
        NSImage *image = [NSImage imageWithSystemSymbolName:symbols[index] accessibilityDescription:pageTitles[index]];
        if (image != nil) return image;
    }
    NSArray *fallbacks = @[NSImageNamePreferencesGeneral, NSImageNameFontPanel, NSImageNameMultipleDocuments,
                           NSImageNameAdvanced, NSImageNameInfo];
    return [NSImage imageNamed:fallbacks[index]];
}

- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSToolbarItemIdentifier)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag {
    NSUInteger index = [pageIdentifiers indexOfObject:itemIdentifier];
    if (index == NSNotFound) return nil;
    NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];
    item.label = pageTitles[index];
    item.paletteLabel = pageTitles[index];
    item.image = [self toolbarImageForIndex:index];
    item.target = self;
    item.action = @selector(onToolbarItem:);
    return item;
}

- (NSArray<NSToolbarItemIdentifier> *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar {
    return pageIdentifiers;
}

- (NSArray<NSToolbarItemIdentifier> *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar {
    return pageIdentifiers;
}

- (NSArray<NSToolbarItemIdentifier> *)toolbarSelectableItemIdentifiers:(NSToolbar *)toolbar {
    return pageIdentifiers;
}

- (void)onLanguageSegment:(NSSegmentedControl *)sender {
    int modes[] = {1, 0, 2};
    int mode = modes[MIN(MAX(sender.selectedSegment, 0), 2)];
    if (mode != [appDelegate currentInputMode]) {
        [appDelegate selectInputMode:mode];
    }
}

- (IBAction)onInputTypeChanged:(NSPopUpButton *)sender {
    [appDelegate onInputTypeSelectedIndex:(int)[self.popupInputType indexOfSelectedItem]];
}

- (IBAction)onCodeTableChanged:(NSPopUpButton *)sender {
    [appDelegate onCodeTableChanged:(int)[self.popupCode indexOfSelectedItem]];
}

- (IBAction)onLanguageChanged:(id)sender {
    [appDelegate onInputMethodSelected];
}

- (IBAction)onRestart:(id)sender {
    self.appOK.hidden = YES;
    self.permissionWarning.hidden = YES;
    self.retryButton.enabled = NO;
    
    [self initKey];
}

- (IBAction)onFreeMark:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"FreeMark"];
    vFreeMark = (int)val;
}

- (IBAction)onModernOrthography:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"ModernOrthography"];
    vUseModernOrthography = (int)val;
}

- (IBAction)onCheckSpelling:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"Spelling"];
    vCheckSpelling = (int)val;
    [self.RestoreIfInvalidWord setEnabled:val];
    [self.AllowZWJF setEnabled:val];
    [self.TempOffSpellChecking setEnabled:val];
    OnSpellCheckingChanged();
}

- (IBAction)onShowUIOnStartup:(NSButton *)sender {
    [self setCustomValue:sender keyToSet:@"ShowUIOnStartup"];
}

- (IBAction)onRunOnStartup:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"RunOnStartup"];
    [appDelegate setRunOnStartup:val];
}

- (IBAction)onGrayIcon:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"GrayIcon"];
    [appDelegate setGrayIcon:val];
}

- (IBAction)onQuickTelex:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"QuickTelex"];
    vQuickTelex = (int)val;
}

- (IBAction)onRestoreIfInvalidWord:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"RestoreIfInvalidWord"];
    vRestoreIfWrongSpelling = (int)val;
}

- (IBAction)omTempOffSpellChecking:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"vTempOffSpelling"];
    vTempOffSpelling = (int)val;
}

- (IBAction)onAllowZFWJ:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"vAllowConsonantZFWJ"];
    vAllowConsonantZFWJ = (int)val;
}

- (IBAction)onFixRecommendBrowser:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"FixRecommendBrowser"];
    vFixRecommendBrowser = (int)val;
    [self.FixChromiumBrowser setEnabled:val];
}

- (IBAction)onControlSwitchKey:(NSButton *)sender {
    [self setSwitchKeyBit:0x100 fromButton:sender];
}

- (IBAction)onOptionSwitchKey:(NSButton *)sender {
    [self setSwitchKeyBit:0x200 fromButton:sender];
}

- (IBAction)onCommandSwitchKey:(NSButton *)sender {
    [self setSwitchKeyBit:0x400 fromButton:sender];
}

- (IBAction)onShiftSwitchKey:(NSButton *)sender {
    [self setSwitchKeyBit:0x800 fromButton:sender];
}

- (IBAction)onFnSwitchKey:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:nil];
    if (val) {
        // Selecting Globe configures a single Fn press; other combinations can
        // still be chosen afterwards using the modifier controls and key field.
        vSwitchKeyStatus = (vSwitchKeyStatus & 0x8000) | 0xFE0010FE;
    } else {
        vSwitchKeyStatus &= ~0x1000;
    }
    [[NSUserDefaults standardUserDefaults] setInteger:vSwitchKeyStatus forKey:@"SwitchKeyStatus"];
    [self fillData];
}

-(void)onMyTextFieldKeyChange:(unsigned short)keyCode character:(unsigned short)character {
    vSwitchKeyStatus &= 0xFFFFFF00;
    vSwitchKeyStatus |= keyCode;
    vSwitchKeyStatus &= 0x00FFFFFF;
    vSwitchKeyStatus |= ((unsigned int)character<<24);
    [[NSUserDefaults standardUserDefaults] setInteger:vSwitchKeyStatus forKey:@"SwitchKeyStatus"];
}

- (IBAction)onBeepSound:(NSButton *)sender {
    [self setSwitchKeyBit:0x8000 fromButton:sender];
}

// Sets one flag of the switch key status from a checkbox and saves it.
- (void)setSwitchKeyBit:(int)bit fromButton:(NSButton *)sender {
    if (sender.state == NSControlStateValueOn) vSwitchKeyStatus |= bit;
    else vSwitchKeyStatus &= ~bit;
    [[NSUserDefaults standardUserDefaults] setInteger:vSwitchKeyStatus forKey:@"SwitchKeyStatus"];
}

- (IBAction)onSendKeyStepByStep:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"SendKeyStepByStep"];
    vSendKeyStepByStep = (int)val;
}

- (IBAction)onPerformLayoutCompat:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"vPerformLayoutCompat"];
    vPerformLayoutCompat = (int)val;
}

- (NSInteger)setCustomValue:(NSButton*)sender keyToSet:(NSString*) key {
    NSInteger val = sender.state == NSControlStateValueOn ? 1 : 0;
    if (key != nil)
        [[NSUserDefaults standardUserDefaults] setInteger:val forKey:key];
    return val;
}

- (IBAction)onMacroButton:(id)sender {
    [appDelegate onMacroSelected];
}

- (IBAction)onMacroChanged:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"UseMacro"];
    vUseMacro = (int)val;
}

- (IBAction)onUseMacroInEnglishModeChanged:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"UseMacroInEnglishMode"];
    vUseMacroInEnglishMode = (int)val;
}

- (IBAction)onAutoRememberSwitchKey:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"UseSmartSwitchKey"];
    vUseSmartSwitchKey = (int)val;
}

- (IBAction)onUpperCaseFirstChar:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"UpperCaseFirstChar"];
    vUpperCaseFirstChar = (int)val;
}
- (IBAction)onQuickStartConsonant:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"vQuickStartConsonant"];
    vQuickStartConsonant = (int)val;
}

- (IBAction)onQuickEndConsonant:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"vQuickEndConsonant"];
    vQuickEndConsonant = (int)val;
}

- (IBAction)onTempOffOmnikeyByHotKey:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"vTempOffOmnikey"];
    vTempOffOmnikey = (int)val;
}

- (IBAction)onRememberTableCode:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"vRememberCode"];
    vRememberCode = (int)val;
}
- (IBAction)onOtherLanguage:(id)sender {
    
    NSInteger val = [self setCustomValue:sender keyToSet:@"vOtherLanguage"];
    vOtherLanguage = (int)val;
}


- (IBAction)onAutoCapsMacro:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"vAutoCapsMacro"];
    vAutoCapsMacro = (int)val;
}

- (IBAction)onShowIconOnDock:(id)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"vShowIconOnDock"];
    vShowIconOnDock = (int)val;
    if (!vShowIconOnDock) {
        [self.view.window close];
    }
    [appDelegate showIconOnDock:vShowIconOnDock];
}

- (IBAction)onCheckNewVersionOnStartup:(NSButton *)sender {
    NSInteger val = sender.state == NSControlStateValueOn ? 0 : 1;
    [[NSUserDefaults standardUserDefaults] setInteger:val forKey:@"DontCheckUpdate"];
}

- (IBAction)onFixChromiumBrowser:(NSButton *)sender {
    NSInteger val = [self setCustomValue:sender keyToSet:@"vFixChromiumBrowser"];
    vFixChromiumBrowser = (int)val;
}

- (IBAction)onTerminateApp:(id)sender {
    [NSApp terminate:0];
}

-(void)fillData {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    // Shows a stored 0/1 option on its checkbox and returns the stored value.
    // (Some outlets are NSButtonCell, which also has setState:.)
    NSInteger (^check)(id, NSString *) = ^NSInteger(id button, NSString *key) {
        NSInteger value = [defaults integerForKey:key];
        [button setState:value ? NSControlStateValueOn : NSControlStateValueOff];
        return value;
    };
    
    // Segments: Tiếng Việt, English, 中文; modes: 1, 0, 2.
    int mode = [appDelegate currentInputMode];
    languageControl.selectedSegment = mode == 1 ? 0 : (mode == 0 ? 1 : 2);
    
    [self.popupInputType selectItemAtIndex:[defaults integerForKey:@"InputType"]];
    [self.popupCode selectItemAtIndex:[defaults integerForKey:@"CodeTable"]];
    
    //option
    check(self.ShowUIButton, @"ShowUIOnStartup");
    check(self.FreeMarkButton, @"FreeMark");
    check(self.UseModernOrthography, @"ModernOrthography");
    NSInteger spelling = check(self.CheckSpellingButton, @"Spelling");
    check(self.RunOnStartupButton, @"RunOnStartup");
    check(self.UseGrayIcon, @"GrayIcon");
    check(self.QuickTelex, @"QuickTelex");
    check(self.RestoreIfInvalidWord, @"RestoreIfInvalidWord");
    [self.RestoreIfInvalidWord setEnabled:spelling];
    check(self.TempOffSpellChecking, @"vTempOffSpelling");
    [self.TempOffSpellChecking setEnabled:spelling];
    check(self.AllowZWJF, @"vAllowConsonantZFWJ");
    [self.AllowZWJF setEnabled:spelling];
    NSInteger fixRecommendBrowser = check(self.FixRecommendBrowser, @"FixRecommendBrowser");
    check(self.UseMacro, @"UseMacro");
    check(self.UseMacroInEnglishMode, @"UseMacroInEnglishMode");
    check(self.SendKeyStepByStep, @"SendKeyStepByStep");
    check(self.AutoRememberSwitchKey, @"UseSmartSwitchKey");
    check(self.UpperCaseFirstChar, @"UpperCaseFirstChar");
    check(self.QuickStartConsonant, @"vQuickStartConsonant");
    check(self.QuickEndConsonant, @"vQuickEndConsonant");
    check(self.RememberTableCode, @"vRememberCode");
    check(self.OtherLanguage, @"vOtherLanguage");
    check(self.TempOffOmnikey, @"vTempOffOmnikey");
    check(self.AutoCapsMacro, @"vAutoCapsMacro");
    check(self.ShowIconOnDock, @"vShowIconOnDock");
    self.CheckNewVersionOnStartup.state = [defaults integerForKey:@"DontCheckUpdate"] ? NSControlStateValueOff : NSControlStateValueOn;
    check(self.FixChromiumBrowser, @"vFixChromiumBrowser");
    self.FixChromiumBrowser.enabled = fixRecommendBrowser ? YES : NO;
    check(self.PerformLayoutCompat, @"vPerformLayoutCompat");
    
    CustomSwitchControl.state = (vSwitchKeyStatus & 0x100) ? NSControlStateValueOn : NSControlStateValueOff;
    CustomSwitchOption.state = (vSwitchKeyStatus & 0x200) ? NSControlStateValueOn : NSControlStateValueOff;
    CustomSwitchCommand.state = (vSwitchKeyStatus & 0x400) ? NSControlStateValueOn : NSControlStateValueOff;
    CustomSwitchShift.state = (vSwitchKeyStatus & 0x800) ? NSControlStateValueOn : NSControlStateValueOff;
    CustomSwitchFn.state = (vSwitchKeyStatus & 0x1000) ? NSControlStateValueOn : NSControlStateValueOff;
    CustomBeepSound.state = (vSwitchKeyStatus & 0x8000) ? NSControlStateValueOn : NSControlStateValueOff;
    [CustomSwitchKey setTextByChar:((vSwitchKeyStatus>>24) & 0xFF)];
    
}

- (IBAction)onOK:(id)sender {
    [self.view.window close];
}

- (IBAction)onDefaultConfig:(id)sender {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Bạn có chắc chắn muốn thiết lập lại cấu hình mặc định?"];
    [alert addButtonWithTitle:@"Có"];
    [alert addButtonWithTitle:@"Không"];
    [alert beginSheetModalForWindow:self.view.window completionHandler:^(NSModalResponse returnCode) {
        if (returnCode == 1000) {
            [appDelegate loadDefaultConfig];
            [[NSUserDefaults standardUserDefaults] setInteger:0 forKey:@"ShowUIOnStartup"];
            self.ShowUIButton.state = NSControlStateValueOff;
            
            [[NSUserDefaults standardUserDefaults] setInteger:1 forKey:@"RunOnStartup"];
            self.RunOnStartupButton.state = NSControlStateValueOn;
        }
    }];
}

- (IBAction)onHomePageLink:(id)sender {
    [[NSWorkspace sharedWorkspace] openURL: [NSURL URLWithString:@"https://open-key.org"]];
}

- (IBAction)onFanpageLink:(id)sender {
    [[NSWorkspace sharedWorkspace] openURL: [NSURL URLWithString:@"https://www.facebook.com/OpenKeyVN"]];
}

- (IBAction)onEmailLink:(id)sender {
    [[NSWorkspace sharedWorkspace] openURL: [NSURL URLWithString:@"mailto:maivutuyen.91@gmail.com"]];
}

- (IBAction)onSourceCode:(id)sender {
  [[NSWorkspace sharedWorkspace] openURL: [NSURL URLWithString:@"https://github.com/jetaudio/Omnikey"]];
}

- (IBAction)onCheckNewVersionButton:(id)sender {
    self.CheckNewVersionButton.title = @"Đang kiểm tra…";
    self.CheckNewVersionButton.enabled = false;
    
    [OmnikeyManager checkNewVersion:self.view.window callbackFunc:^{
        self.CheckNewVersionButton.enabled = true;
        self.CheckNewVersionButton.title = @"Kiểm tra bản mới…";
    }];
}

@end
