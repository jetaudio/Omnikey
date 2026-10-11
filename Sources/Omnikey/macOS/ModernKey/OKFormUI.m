//
//  OKFormUI.m
//  OpenKey
//

#import "OKFormUI.h"

// Metrics measured from System Settings on macOS 26 Tahoe.
static const CGFloat kRowInset = 10;
static const CGFloat kGroupRadius = 18;

static BOOL OKIsDarkAppearance(NSAppearance *appearance) {
    NSString *match = [appearance bestMatchFromAppearancesWithNames:@[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]];
    return [match isEqualToString:NSAppearanceNameDarkAqua];
}

@implementation OKFlippedView
- (BOOL)isFlipped { return YES; }
@end

@implementation OKFormGroupView
- (void)drawRect:(NSRect)dirtyRect {
    BOOL dark = OKIsDarkAppearance(self.effectiveAppearance);
    // Tahoe groups are a borderless tint over the window background.
    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:self.bounds xRadius:kGroupRadius yRadius:kGroupRadius];
    [[NSColor colorWithWhite:dark ? 1 : 0 alpha:dark ? 0.055 : 0.04] setFill];
    [path fill];
}
- (void)viewDidChangeEffectiveAppearance {
    self.needsDisplay = YES;
}
@end

@implementation OKRoundedContainerView
- (instancetype)initWithFrame:(NSRect)frameRect {
    if (self = [super initWithFrame:frameRect]) {
        self.wantsLayer = YES;
        self.layer.cornerRadius = 14;
        self.layer.masksToBounds = YES;
        self.layer.borderWidth = 1;
    }
    return self;
}
- (BOOL)wantsUpdateLayer { return YES; }
- (void)updateLayer {
    BOOL dark = OKIsDarkAppearance(self.effectiveAppearance);
    self.layer.borderColor = [NSColor colorWithWhite:dark ? 1 : 0 alpha:dark ? 0.12 : 0.1].CGColor;
}
- (void)viewDidChangeEffectiveAppearance {
    self.needsDisplay = YES;
}
@end

@interface OKRowSeparatorView : NSView
@end
@implementation OKRowSeparatorView
- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor separatorColor] setFill];
    CGFloat scale = self.window.backingScaleFactor ?: 2;
    NSRectFillUsingOperation(NSMakeRect(kRowInset, 0, NSWidth(self.bounds) - kRowInset * 2, 1 / scale), NSCompositingOperationSourceOver);
}
@end

@implementation OKControlMirror {
    NSArray<NSButton *> *_sources;
    __weak NSControl *_control;
}
static void *OKMirrorContext = &OKMirrorContext;

- (instancetype)initWithSources:(NSArray<NSButton *> *)sources control:(NSControl *)control {
    if (self = [super init]) {
        _sources = sources;
        _control = control;
        control.target = self;
        control.action = @selector(controlChanged:);
        for (NSButton *source in sources) {
            for (NSString *key in @[@"state", @"enabled"]) {
                [source addObserver:self forKeyPath:key options:0 context:OKMirrorContext];
                [source.cell addObserver:self forKeyPath:key options:0 context:OKMirrorContext];
            }
        }
        [self refresh];
    }
    return self;
}

- (void)dealloc {
    for (NSButton *source in _sources) {
        for (NSString *key in @[@"state", @"enabled"]) {
            [source removeObserver:self forKeyPath:key context:OKMirrorContext];
            [source.cell removeObserver:self forKeyPath:key context:OKMirrorContext];
        }
    }
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context {
    if (context == OKMirrorContext) {
        [self refresh];
    } else {
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
    }
}

- (BOOL)isControlOnAtIndex:(NSUInteger)index {
    NSControl *control = _control;
    if ([control isKindOfClass:[NSSegmentedControl class]])
        return [(NSSegmentedControl *)control isSelectedForSegment:index];
    return [(id)control state] == NSControlStateValueOn;
}

- (void)refresh {
    NSControl *control = _control;
    if ([control isKindOfClass:[NSSegmentedControl class]]) {
        NSSegmentedControl *segmented = (NSSegmentedControl *)control;
        [_sources enumerateObjectsUsingBlock:^(NSButton *source, NSUInteger i, BOOL *stop) {
            [segmented setSelected:source.state == NSControlStateValueOn forSegment:i];
            [segmented setEnabled:source.enabled forSegment:i];
        }];
    } else {
        NSButton *source = _sources.firstObject;
        [(id)control setState:source.state];
        control.enabled = source.enabled;
    }
}

- (void)controlChanged:(id)sender {
    // Only one source changes per click; its action may refresh the others.
    for (NSUInteger i = 0; i < _sources.count; i++) {
        NSButton *source = _sources[i];
        BOOL on = [self isControlOnAtIndex:i];
        if (on != (source.state == NSControlStateValueOn)) {
            source.state = on ? NSControlStateValueOn : NSControlStateValueOff;
            [NSApp sendAction:source.action to:source.target from:source];
            break;
        }
    }
    [self refresh];
}
@end

@implementation OKFormBuilder {
    NSMutableArray<OKControlMirror *> *_mirrors;
}

- (instancetype)initWithWidth:(CGFloat)width {
    if (self = [super init]) {
        _width = width;
        _mirrors = [NSMutableArray array];
    }
    return self;
}

- (NSTextField *)labelWithString:(NSString *)string font:(NSFont *)font color:(NSColor *)color {
    NSTextField *label = [NSTextField wrappingLabelWithString:string];
    label.font = font;
    label.textColor = color;
    label.selectable = NO;
    label.alignment = NSTextAlignmentLeft;
    label.translatesAutoresizingMaskIntoConstraints = NO;
    // Let section headers and notes stretch across a width-aligned stack.
    [label setContentHuggingPriority:100 forOrientation:NSLayoutConstraintOrientationHorizontal];
    return label;
}

- (NSView *)rowWithTitle:(NSString *)title detail:(NSString *)detail accessory:(NSView *)accessory {
    NSView *row = [[NSView alloc] init];
    row.translatesAutoresizingMaskIntoConstraints = NO;

    NSTextField *titleLabel = [self labelWithString:title font:[NSFont systemFontOfSize:13] color:[NSColor labelColor]];
    NSMutableArray *texts = [NSMutableArray arrayWithObject:titleLabel];
    if (detail.length > 0) {
        [texts addObject:[self labelWithString:detail font:[NSFont systemFontOfSize:11] color:[NSColor secondaryLabelColor]]];
    }
    CGFloat textWidth = self.width - 40 - kRowInset * 2 - 16 - MAX(accessory.fittingSize.width, 60);
    for (NSTextField *label in texts) {
        label.preferredMaxLayoutWidth = textWidth;
        [label setContentCompressionResistancePriority:NSLayoutPriorityDefaultLow forOrientation:NSLayoutConstraintOrientationHorizontal];
    }
    NSStackView *text = [NSStackView stackViewWithViews:texts];
    text.orientation = NSUserInterfaceLayoutOrientationVertical;
    text.alignment = NSLayoutAttributeLeading;
    text.spacing = 2;
    text.translatesAutoresizingMaskIntoConstraints = NO;
    [row addSubview:text];

    NSLayoutConstraint *compact = [row.heightAnchor constraintEqualToConstant:40];
    compact.priority = NSLayoutPriorityDefaultLow;
    NSMutableArray *constraints = [NSMutableArray arrayWithArray:@[
        [text.leadingAnchor constraintEqualToAnchor:row.leadingAnchor constant:kRowInset],
        [text.centerYAnchor constraintEqualToAnchor:row.centerYAnchor],
        [text.topAnchor constraintGreaterThanOrEqualToAnchor:row.topAnchor constant:10],
        [row.heightAnchor constraintGreaterThanOrEqualToConstant:40],
        compact,
    ]];
    if (accessory != nil) {
        accessory.translatesAutoresizingMaskIntoConstraints = NO;
        [accessory setContentHuggingPriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationHorizontal];
        [accessory setContentCompressionResistancePriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationHorizontal];
        [row addSubview:accessory];
        [constraints addObjectsFromArray:@[
            [accessory.trailingAnchor constraintEqualToAnchor:row.trailingAnchor constant:-kRowInset],
            [accessory.centerYAnchor constraintEqualToAnchor:row.centerYAnchor],
            [accessory.leadingAnchor constraintGreaterThanOrEqualToAnchor:text.trailingAnchor constant:16],
            [accessory.topAnchor constraintGreaterThanOrEqualToAnchor:row.topAnchor constant:6],
        ]];
    } else {
        [constraints addObject:[text.trailingAnchor constraintLessThanOrEqualToAnchor:row.trailingAnchor constant:-kRowInset]];
    }
    [NSLayoutConstraint activateConstraints:constraints];
    return row;
}

- (NSView *)toggleRowForButton:(NSButton *)button title:(NSString *)title detail:(NSString *)detail {
    // Before NSSwitch exists the checkbox itself sits at the trailing edge.
    NSControl *toggle = button;
    if (@available(macOS 10.15, *)) {
        NSSwitch *control = [[NSSwitch alloc] init];
        control.controlSize = NSControlSizeSmall;
        control.toolTip = button.toolTip;
        [control setAccessibilityLabel:title];
        [_mirrors addObject:[[OKControlMirror alloc] initWithSources:@[button] control:control]];
        toggle = control;
    } else {
        button.title = @"";
    }
    return [self rowWithTitle:title detail:detail accessory:toggle];
}

- (NSView *)groupWithRows:(NSArray<NSView *> *)rows {
    OKFormGroupView *group = [[OKFormGroupView alloc] init];
    group.translatesAutoresizingMaskIntoConstraints = NO;
    NSMutableArray *views = [NSMutableArray array];
    for (NSView *row in rows) {
        if (views.count > 0) {
            OKRowSeparatorView *separator = [[OKRowSeparatorView alloc] init];
            separator.translatesAutoresizingMaskIntoConstraints = NO;
            [separator.heightAnchor constraintEqualToConstant:1].active = YES;
            [views addObject:separator];
        }
        [views addObject:row];
    }
    NSStackView *stack = [NSStackView stackViewWithViews:views];
    stack.orientation = NSUserInterfaceLayoutOrientationVertical;
    stack.alignment = NSLayoutAttributeWidth;
    stack.spacing = 0;
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    [group addSubview:stack];
    [NSLayoutConstraint activateConstraints:@[
        [stack.leadingAnchor constraintEqualToAnchor:group.leadingAnchor],
        [stack.trailingAnchor constraintEqualToAnchor:group.trailingAnchor],
        [stack.topAnchor constraintEqualToAnchor:group.topAnchor constant:4],
        [stack.bottomAnchor constraintEqualToAnchor:group.bottomAnchor constant:-4],
    ]];
    return group;
}

// Section headers and notes line up with the text inside the rows.
- (NSView *)insetLabel:(NSTextField *)label {
    NSView *container = [[NSView alloc] init];
    container.translatesAutoresizingMaskIntoConstraints = NO;
    [container addSubview:label];
    label.preferredMaxLayoutWidth = self.width - 40 - kRowInset * 2;
    [NSLayoutConstraint activateConstraints:@[
        [label.leadingAnchor constraintEqualToAnchor:container.leadingAnchor constant:kRowInset],
        [label.trailingAnchor constraintEqualToAnchor:container.trailingAnchor constant:-kRowInset],
        [label.topAnchor constraintEqualToAnchor:container.topAnchor],
        [label.bottomAnchor constraintEqualToAnchor:container.bottomAnchor],
    ]];
    return container;
}

- (void)addSection:(NSString *)header rows:(NSArray<NSView *> *)rows note:(NSString *)note toStack:(NSStackView *)stack {
    if (stack.arrangedSubviews.count > 0) {
        [stack setCustomSpacing:26 afterView:stack.arrangedSubviews.lastObject];
    }
    if (header.length > 0) {
        NSView *label = [self insetLabel:[self labelWithString:header font:[NSFont boldSystemFontOfSize:13] color:[NSColor labelColor]]];
        [stack addArrangedSubview:label];
        [stack setCustomSpacing:10 afterView:label];
    }
    NSView *group = [self groupWithRows:rows];
    [stack addArrangedSubview:group];
    if (note.length > 0) {
        NSView *label = [self insetLabel:[self labelWithString:note font:[NSFont systemFontOfSize:11] color:[NSColor secondaryLabelColor]]];
        [stack setCustomSpacing:8 afterView:group];
        [stack addArrangedSubview:label];
    }
}

- (NSStackView *)pageStack {
    NSStackView *stack = [[NSStackView alloc] init];
    stack.orientation = NSUserInterfaceLayoutOrientationVertical;
    stack.alignment = NSLayoutAttributeWidth;
    stack.spacing = 8;
    stack.edgeInsets = NSEdgeInsetsMake(20, 20, 20, 20);
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    return stack;
}

- (NSScrollView *)scrollPageWithStack:(NSStackView *)stack {
    OKFlippedView *document = [[OKFlippedView alloc] init];
    document.translatesAutoresizingMaskIntoConstraints = NO;
    [document addSubview:stack];

    NSScrollView *scrollView = [[NSScrollView alloc] init];
    scrollView.translatesAutoresizingMaskIntoConstraints = NO;
    scrollView.drawsBackground = NO;
    scrollView.borderType = NSNoBorder;
    scrollView.hasVerticalScroller = YES;
    scrollView.autohidesScrollers = YES;
    scrollView.automaticallyAdjustsContentInsets = NO;
    scrollView.documentView = document;
    [NSLayoutConstraint activateConstraints:@[
        [stack.leadingAnchor constraintEqualToAnchor:document.leadingAnchor],
        [stack.trailingAnchor constraintEqualToAnchor:document.trailingAnchor],
        [stack.topAnchor constraintEqualToAnchor:document.topAnchor],
        [stack.bottomAnchor constraintEqualToAnchor:document.bottomAnchor],
        [document.widthAnchor constraintEqualToConstant:self.width],
        [document.leadingAnchor constraintEqualToAnchor:scrollView.contentView.leadingAnchor],
        [document.topAnchor constraintEqualToAnchor:scrollView.contentView.topAnchor],
    ]];
    return scrollView;
}

- (NSSegmentedControl *)modifierControlForButtons:(NSArray<NSButton *> *)buttons labels:(NSArray<NSString *> *)labels {
    NSSegmentedControl *control = [NSSegmentedControl segmentedControlWithLabels:labels
                                                                    trackingMode:NSSegmentSwitchTrackingSelectAny
                                                                          target:nil
                                                                          action:nil];
    [buttons enumerateObjectsUsingBlock:^(NSButton *button, NSUInteger i, BOOL *stop) {
        [control setWidth:34 forSegment:i];
        [control setToolTip:button.toolTip forSegment:i];
    }];
    [_mirrors addObject:[[OKControlMirror alloc] initWithSources:buttons control:control]];
    return control;
}

- (void)prepareKeyField:(NSTextField *)field {
    field.translatesAutoresizingMaskIntoConstraints = NO;
    field.bezeled = YES;
    field.bezelStyle = NSTextFieldRoundedBezel;
    field.alignment = NSTextAlignmentCenter;
    field.font = [NSFont systemFontOfSize:13];
    field.placeholderString = @"Phím";
    [field.widthAnchor constraintEqualToConstant:64].active = YES;
}

- (void)prepareInlinePopup:(NSPopUpButton *)popup {
    // Borderless popups read as "value ⌃⌄", like the pickers in System Settings.
    popup.translatesAutoresizingMaskIntoConstraints = NO;
    popup.controlSize = NSControlSizeRegular;
    popup.font = [NSFont systemFontOfSize:13];
    popup.bordered = NO;
    ((NSPopUpButtonCell *)popup.cell).arrowPosition = NSPopUpArrowAtBottom;
    popup.alignment = NSTextAlignmentRight;
}

- (NSArray<NSView *> *)aboutHeaderWithVersionField:(NSTextField *)version {
    NSImageView *icon = [NSImageView imageViewWithImage:[NSImage imageNamed:@"Icon"] ?: NSApp.applicationIconImage];
    icon.imageScaling = NSImageScaleProportionallyUpOrDown;
    icon.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
        [icon.widthAnchor constraintEqualToConstant:96],
        [icon.heightAnchor constraintEqualToConstant:96],
    ]];
    NSTextField *name = [self labelWithString:@"Omnikey" font:[NSFont systemFontOfSize:22 weight:NSFontWeightBold] color:[NSColor labelColor]];
    NSTextField *tagline = [self labelWithString:@"Bộ gõ Tiếng Việt nguồn mở đa nền tảng"
                                            font:[NSFont systemFontOfSize:13] color:[NSColor secondaryLabelColor]];
    for (NSTextField *label in @[name, tagline]) {
        label.alignment = NSTextAlignmentCenter;
        [label setContentHuggingPriority:NSLayoutPriorityDefaultHigh forOrientation:NSLayoutConstraintOrientationHorizontal];
    }
    version.translatesAutoresizingMaskIntoConstraints = NO;
    version.font = [NSFont monospacedDigitSystemFontOfSize:11 weight:NSFontWeightRegular];
    version.textColor = [NSColor tertiaryLabelColor];
    version.alignment = NSTextAlignmentCenter;
    version.selectable = YES;
    return @[icon, name, tagline, version];
}

+ (NSButton *)pushButtonWithTitle:(NSString *)title target:(id)target action:(SEL)action {
    NSButton *button = [NSButton buttonWithTitle:title target:target action:action];
    button.bezelStyle = NSBezelStylePush;
    return button;
}

+ (NSButton *)linkButtonWithTitle:(NSString *)title target:(id)target action:(SEL)action {
    NSButton *button = [NSButton buttonWithTitle:title target:target action:action];
    button.bordered = NO;
    button.attributedTitle = [[NSAttributedString alloc] initWithString:title attributes:@{
        NSForegroundColorAttributeName: [NSColor linkColor],
        NSFontAttributeName: [NSFont systemFontOfSize:11],
    }];
    return button;
}

+ (void)preparePushButton:(NSButton *)button {
    button.bezelStyle = NSBezelStylePush;
    button.controlSize = NSControlSizeRegular;
    button.font = [NSFont systemFontOfSize:13];
}
@end
