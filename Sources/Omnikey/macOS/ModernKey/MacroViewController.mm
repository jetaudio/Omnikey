//
//  MacroViewController.mm
//  OpenKey
//
//  Created by Tuyen on 8/4/19.
//  Copyright © 2019 Tuyen Mai. All rights reserved.
//

#import "MacroViewController.h"
#include "Engine.h"
#import "OKFormUI.h"

#define MACRO_ADD_TEXT @"Thêm"
#define MACRO_EDIT_TEXT @"Sửa"

@interface MacroViewController ()

@end

@implementation MacroViewController{
    vector<vector<Uint32>> keys;
    vector<string> macroText;
    vector<string> macroContent;
    NSTextField *emptyLabel;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.tableView.dataSource = self;
    self.tableView.delegate = self;
    
    self.macroName.delegate = self;
    self.macroContent.delegate = self;
    
    self.AutoCapsMacro.state = vAutoCapsMacro ? NSControlStateValueOn : NSControlStateValueOff;
    
    //load data
    getAllMacro(keys, macroText, macroContent);
    
    [self buildLayout];
}

- (void)viewWillAppear {
    [super viewWillAppear];
    NSWindow *window = self.view.window;
    window.title = @"Bảng gõ tắt";
    window.contentMinSize = NSMakeSize(520, 360);
}

- (void)prepareField:(NSTextField *)field placeholder:(NSString *)placeholder {
    field.translatesAutoresizingMaskIntoConstraints = NO;
    field.font = [NSFont systemFontOfSize:13];
    field.placeholderString = placeholder;
    field.bezeled = YES;
    field.bezelStyle = NSTextFieldRoundedBezel;
}

- (void)buildLayout {
    OKFormBuilder *form = [[OKFormBuilder alloc] initWithWidth:600];
    
    // Entry bar: shortcut, expansion, delete and add (Return).
    [self prepareField:self.macroName placeholder:@"Từ gõ tắt"];
    [self prepareField:self.macroContent placeholder:@"Nội dung đầy đủ"];
    [self.macroName.widthAnchor constraintEqualToConstant:140].active = YES;
    [self.macroContent setContentHuggingPriority:NSLayoutPriorityDefaultLow forOrientation:NSLayoutConstraintOrientationHorizontal];
    NSButton *add = self.buttonAdd;
    [OKFormBuilder preparePushButton:add];
    add.image = nil;
    add.contentTintColor = nil;
    add.keyEquivalent = @"\r";
    add.keyEquivalentModifierMask = 0;
    add.translatesAutoresizingMaskIntoConstraints = NO;
    [add.widthAnchor constraintGreaterThanOrEqualToConstant:72].active = YES;
    NSButton *remove = [OKFormBuilder pushButtonWithTitle:@"Xoá" target:self action:@selector(onDeleteMacro:)];
    remove.keyEquivalent = [NSString stringWithFormat:@"%C", (unichar)NSBackspaceCharacter];
    remove.keyEquivalentModifierMask = NSEventModifierFlagCommand;
    remove.toolTip = @"Xoá từ gõ tắt đang chọn (⌘⌫)";
    [remove.widthAnchor constraintGreaterThanOrEqualToConstant:72].active = YES;
    NSStackView *entry = [NSStackView stackViewWithViews:@[self.macroName, self.macroContent, remove, add]];
    entry.spacing = 8;
    entry.translatesAutoresizingMaskIntoConstraints = NO;
    
    // Table inside a rounded, borderless-looking container.
    NSScrollView *scrollView = self.tableView.enclosingScrollView;
    scrollView.translatesAutoresizingMaskIntoConstraints = NO;
    scrollView.borderType = NSNoBorder;
    scrollView.autohidesScrollers = YES;
    if (@available(macOS 11.0, *)) {
        self.tableView.style = NSTableViewStyleFullWidth;
    }
    self.tableView.rowHeight = 26;
    self.tableView.intercellSpacing = NSMakeSize(10, 0);
    self.tableView.gridStyleMask = NSTableViewGridNone;
    self.tableView.usesAlternatingRowBackgroundColors = YES;
    self.tableView.columnAutoresizingStyle = NSTableViewLastColumnOnlyAutoresizingStyle;
    NSArray<NSString *> *headers = @[@"Từ gõ tắt", @"Nội dung"];
    [self.tableView.tableColumns enumerateObjectsUsingBlock:^(NSTableColumn *column, NSUInteger i, BOOL *stop) {
        if (i < headers.count) column.title = headers[i];
    }];
    self.tableView.tableColumns.firstObject.width = 150;
    OKRoundedContainerView *tableBox = [[OKRoundedContainerView alloc] init];
    tableBox.translatesAutoresizingMaskIntoConstraints = NO;
    [tableBox addSubview:scrollView];
    
    emptyLabel = [form labelWithString:@"Chưa có từ gõ tắt nào.\nNhập từ gõ tắt và nội dung ở trên rồi bấm Thêm."
                                  font:[NSFont systemFontOfSize:13] color:[NSColor secondaryLabelColor]];
    emptyLabel.alignment = NSTextAlignmentCenter;
    [emptyLabel setContentHuggingPriority:NSLayoutPriorityDefaultHigh forOrientation:NSLayoutConstraintOrientationHorizontal];
    [tableBox addSubview:emptyLabel];
    
    // Footer: import/export on the left, auto-caps on the right.
    NSStackView *files = [NSStackView stackViewWithViews:@[
        [OKFormBuilder pushButtonWithTitle:@"Nạp từ file…" target:self action:@selector(onLoadFromFile:)],
        [OKFormBuilder pushButtonWithTitle:@"Xuất ra file…" target:self action:@selector(onExportToFile:)],
    ]];
    files.spacing = 8;
    files.translatesAutoresizingMaskIntoConstraints = NO;
    NSButton *autoCaps = self.AutoCapsMacro;
    autoCaps.translatesAutoresizingMaskIntoConstraints = NO;
    autoCaps.font = [NSFont systemFontOfSize:13];
    
    for (NSView *view in [self.view.subviews copy]) {
        [view removeFromSuperview];
    }
    for (NSView *view in @[entry, tableBox, files, autoCaps]) {
        [self.view addSubview:view];
    }
    NSView *root = self.view;
    [NSLayoutConstraint activateConstraints:@[
        [entry.topAnchor constraintEqualToAnchor:root.topAnchor constant:20],
        [entry.leadingAnchor constraintEqualToAnchor:root.leadingAnchor constant:20],
        [entry.trailingAnchor constraintEqualToAnchor:root.trailingAnchor constant:-20],
        [tableBox.topAnchor constraintEqualToAnchor:entry.bottomAnchor constant:14],
        [tableBox.leadingAnchor constraintEqualToAnchor:root.leadingAnchor constant:20],
        [tableBox.trailingAnchor constraintEqualToAnchor:root.trailingAnchor constant:-20],
        [scrollView.leadingAnchor constraintEqualToAnchor:tableBox.leadingAnchor],
        [scrollView.trailingAnchor constraintEqualToAnchor:tableBox.trailingAnchor],
        [scrollView.topAnchor constraintEqualToAnchor:tableBox.topAnchor],
        [scrollView.bottomAnchor constraintEqualToAnchor:tableBox.bottomAnchor],
        [emptyLabel.centerXAnchor constraintEqualToAnchor:tableBox.centerXAnchor],
        [emptyLabel.centerYAnchor constraintEqualToAnchor:tableBox.centerYAnchor constant:12],
        [emptyLabel.widthAnchor constraintLessThanOrEqualToAnchor:tableBox.widthAnchor constant:-40],
        [files.topAnchor constraintEqualToAnchor:tableBox.bottomAnchor constant:14],
        [files.leadingAnchor constraintEqualToAnchor:root.leadingAnchor constant:20],
        [files.bottomAnchor constraintEqualToAnchor:root.bottomAnchor constant:-20],
        [autoCaps.centerYAnchor constraintEqualToAnchor:files.centerYAnchor],
        [autoCaps.trailingAnchor constraintEqualToAnchor:root.trailingAnchor constant:-20],
        [autoCaps.leadingAnchor constraintGreaterThanOrEqualToAnchor:files.trailingAnchor constant:16],
    ]];
    [self.view layoutSubtreeIfNeeded];
    [self.tableView sizeLastColumnToFit];
    [self updateEmptyState];
}

- (void)updateEmptyState {
    emptyLabel.hidden = keys.size() > 0;
}

-(void)saveAndReload {
    getAllMacro(keys, macroText, macroContent);
    [self.tableView reloadData];
    [self updateEmptyState];
    
    vector<Byte> macroData;
    getMacroSaveData(macroData);
    NSData* _data = [NSData dataWithBytes:macroData.data() length:macroData.size()];
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    [prefs setObject:_data forKey:@"macroData"];
    [self.buttonAdd setTitle:MACRO_ADD_TEXT];
}

- (IBAction)onDeleteMacro:(id)sender {
    if ([[self.macroName stringValue] compare:@""] == 0) {
        [self showMessage:@"Bạn hãy chọn từ cần xoá!"];
        return;
    }
    string text = [[self.macroName stringValue] UTF8String];
    if (deleteMacro(text)) {
        [self saveAndReload];
        self.macroName.stringValue = @"";
        self.macroContent.stringValue = @"";
        [self.macroName becomeFirstResponder];
    }
}

- (IBAction)onAddMacro:(id)sender {
    if ([[self.macroName stringValue] compare:@""] == 0 || [[self.macroContent stringValue] compare:@""] == 0) {
        [self showMessage:@"Bạn hãy nhập từ cần gõ tắt!"];
        return;
    }
    
    string text = [[self.macroName stringValue] UTF8String];
    string content = [[self.macroContent stringValue] UTF8String];

    addMacro(text, content);
    self.macroName.stringValue = @"";
    self.macroContent.stringValue = @"";
    [self.macroName becomeFirstResponder];
    [self saveAndReload];
}

- (IBAction)onLoadFromFile:(id)sender {
    NSOpenPanel* openPanel = [NSOpenPanel openPanel];
    [openPanel setMessage:@"Chọn file dữ liệu gõ tắt"];
    [openPanel setCanChooseFiles:YES];
    [openPanel setAllowsMultipleSelection:NO];
    [openPanel setCanChooseDirectories:NO];
    [openPanel setAllowedFileTypes:[NSArray arrayWithObjects:@"txt", nil]];
    [openPanel setExtensionHidden:NO];
    [openPanel setNameFieldStringValue:@"OmnikeyMacro"];
    [openPanel makeKeyAndOrderFront:nil];
    [openPanel setLevel:NSStatusWindowLevel];
    if ([openPanel runModal] == NSModalResponseOK ) {
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setInformativeText:@"Bạn có muốn giữ lại các dữ liệu hiện tại không?"];
        [alert addButtonWithTitle:@"Có"];
        [alert addButtonWithTitle:@"Không"];
        [alert setMessageText:@"Dữ liệu gõ tắt"];
        [alert setAlertStyle:NSCriticalAlertStyle];
        [alert beginSheetModalForWindow:self.view.window completionHandler:^(NSModalResponse returnCode) {
            readFromFile(openPanel.URL.path.UTF8String, returnCode == 1000);
            [self saveAndReload];
        }];
    }
}

- (IBAction)onExportToFile:(id)sender {
    NSSavePanel* savePanel = [NSSavePanel savePanel];
    savePanel.canCreateDirectories = YES;
    [savePanel setMessage:@"Chọn nơi lưu dữ liệu gõ tắt"];
    [savePanel setTitle:@"Chọn nơi lưu dữ liệu gõ tắt"];
    [savePanel setAllowedFileTypes:[NSArray arrayWithObjects:@"txt", nil]];
    [savePanel setExtensionHidden:NO];
    [savePanel setNameFieldStringValue:@"OmnikeyMacro"];
    if ([savePanel runModal] == NSModalResponseOK) {
        saveToFile(savePanel.URL.path.UTF8String);
    }
}

- (void)showMessage:(NSString*)msg {
    NSAlert* alert = [[NSAlert alloc] init];
    [alert setInformativeText:msg];
    [alert addButtonWithTitle:@"OK"];
    [alert setMessageText:@"Gõ tắt"];
    [alert setAlertStyle:NSCriticalAlertStyle];
    [alert beginSheetModalForWindow:self.view.window completionHandler:^(NSModalResponse returnCode) {
        
    }];
}

- (void)controlTextDidChange:(NSNotification *)notification {
    NSTextField *textField = [notification object];
    if (textField == self.macroName) {
        string text = [[self.macroName stringValue] UTF8String];
        if (hasMacro(text)) {
            [self.buttonAdd setTitle:MACRO_EDIT_TEXT];
        } else {
            [self.buttonAdd setTitle:MACRO_ADD_TEXT];
        }
    }
}

- (IBAction)onAutoCapButton:(NSButton *)sender {
    NSInteger val = sender.state == NSControlStateValueOn ? 1 : 0;
    vAutoCapsMacro = (int)val;
    [[NSUserDefaults standardUserDefaults] setInteger:vAutoCapsMacro forKey:@"vAutoCapsMacro"];
}

#pragma mark TableView
- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    return keys.size();
}

- (nullable NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(nullable NSTableColumn *)tableColumn row:(NSInteger)row {
    NSString* cellId;
    NSTableCellView* v = nil;
    if (tableColumn == tableView.tableColumns[0]) {
        cellId = @"MacroCell";
        v = [tableView makeViewWithIdentifier:cellId owner:self];
        [v.textField setStringValue:[NSString stringWithUTF8String:macroText[row].c_str()]];
    } else if (tableColumn == tableView.tableColumns[1]) {
        cellId = @"ContentCell";
        v = [tableView makeViewWithIdentifier:cellId owner:self];
        [v.textField setStringValue:[NSString stringWithUTF8String:macroContent[row].c_str()]];
    }
    return v;
}

- (BOOL)tableView:(NSTableView *)tableView shouldSelectRow:(NSInteger)row {
    [self.macroName setStringValue:[NSString stringWithUTF8String:macroText[row].c_str()]];
    [self.macroContent setStringValue:[NSString stringWithUTF8String:macroContent[row].c_str()]];
    [self.buttonAdd setTitle:MACRO_EDIT_TEXT];
    return YES;
}

@end
