//
//  OKCandidatePanel.m
//  OpenKey
//

#import "OKCandidatePanel.h"

static const CGFloat kPanelRadius = 14;

// One "1 你好" cell; the highlighted one uses the accent color, like menus.
@interface OKCandidateCell : NSView
@property (nonatomic) NSInteger index;
@property (nonatomic) BOOL highlighted;
@property (nonatomic, copy) void (^onClick)(NSInteger index);
- (instancetype)initWithLabel:(NSString *)label text:(NSString *)text comment:(NSString *)comment highlighted:(BOOL)highlighted;
@end

@implementation OKCandidateCell {
    NSTextField *_label, *_text, *_comment;
}

- (instancetype)initWithLabel:(NSString *)label text:(NSString *)text comment:(NSString *)comment highlighted:(BOOL)highlighted {
    if (self = [super initWithFrame:NSZeroRect]) {
        _highlighted = highlighted;
        self.translatesAutoresizingMaskIntoConstraints = NO;
        _label = [NSTextField labelWithString:label];
        _label.font = [NSFont monospacedDigitSystemFontOfSize:11 weight:NSFontWeightMedium];
        _text = [NSTextField labelWithString:text];
        _text.font = [NSFont systemFontOfSize:17];
        _comment = [NSTextField labelWithString:comment];
        _comment.font = [NSFont systemFontOfSize:11];
        NSMutableArray *views = [NSMutableArray arrayWithObjects:_label, _text, nil];
        if (comment.length > 0) [views addObject:_comment];
        NSStackView *stack = [NSStackView stackViewWithViews:views];
        stack.spacing = 4;
        stack.alignment = NSLayoutAttributeFirstBaseline;
        stack.translatesAutoresizingMaskIntoConstraints = NO;
        [self addSubview:stack];
        [NSLayoutConstraint activateConstraints:@[
            [stack.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:8],
            [stack.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-8],
            [stack.topAnchor constraintEqualToAnchor:self.topAnchor constant:3],
            [stack.bottomAnchor constraintEqualToAnchor:self.bottomAnchor constant:-3],
        ]];
        [self updateColors];
    }
    return self;
}

- (void)updateColors {
    _label.textColor = _highlighted ? [NSColor colorWithWhite:1 alpha:0.85] : [NSColor secondaryLabelColor];
    _text.textColor = _highlighted ? [NSColor whiteColor] : [NSColor labelColor];
    _comment.textColor = _highlighted ? [NSColor colorWithWhite:1 alpha:0.75] : [NSColor tertiaryLabelColor];
}

- (void)drawRect:(NSRect)dirtyRect {
    if (!_highlighted) return;
    [[NSColor controlAccentColor] setFill];
    [[NSBezierPath bezierPathWithRoundedRect:self.bounds xRadius:8 yRadius:8] fill];
}

- (void)mouseDown:(NSEvent *)event {
    if (self.onClick) self.onClick(self.index);
}

@end

@implementation OKCandidatePanel {
    NSView *_content;
    NSStackView *_stack;
    NSRect _caret;
    NSInteger _showToken;
}

+ (instancetype)shared {
    static OKCandidatePanel *instance;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ instance = [[OKCandidatePanel alloc] init]; });
    return instance;
}

- (instancetype)init {
    if (self = [super init]) {
        _panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 200, 60)
                                            styleMask:NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel
                                              backing:NSBackingStoreBuffered
                                                defer:YES];
        _panel.level = NSPopUpMenuWindowLevel;
        _panel.opaque = NO;
        _panel.backgroundColor = [NSColor clearColor];
        _panel.hasShadow = YES;
        _panel.hidesOnDeactivate = NO;
        _panel.becomesKeyOnlyIfNeeded = YES;
        _panel.releasedWhenClosed = NO;
        _panel.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces |
            NSWindowCollectionBehaviorFullScreenAuxiliary | NSWindowCollectionBehaviorIgnoresCycle;

        _stack = [[NSStackView alloc] init];
        _stack.orientation = NSUserInterfaceLayoutOrientationVertical;
        _stack.alignment = NSLayoutAttributeLeading;
        _stack.spacing = 4;
        _stack.edgeInsets = NSEdgeInsetsMake(8, 8, 8, 8);
        _stack.translatesAutoresizingMaskIntoConstraints = NO;
        NSView *holder = [[NSView alloc] init];
        [holder addSubview:_stack];
        [NSLayoutConstraint activateConstraints:@[
            [_stack.leadingAnchor constraintEqualToAnchor:holder.leadingAnchor],
            [_stack.trailingAnchor constraintEqualToAnchor:holder.trailingAnchor],
            [_stack.topAnchor constraintEqualToAnchor:holder.topAnchor],
            [_stack.bottomAnchor constraintEqualToAnchor:holder.bottomAnchor],
        ]];

        // Liquid Glass on macOS 26, a translucent material before it.
        if (@available(macOS 26.0, *)) {
            NSGlassEffectView *glass = [[NSGlassEffectView alloc] init];
            glass.cornerRadius = kPanelRadius;
            glass.contentView = holder;
            _content = glass;
        } else {
            NSVisualEffectView *effect = [[NSVisualEffectView alloc] init];
            effect.material = NSVisualEffectMaterialPopover;
            effect.blendingMode = NSVisualEffectBlendingModeBehindWindow;
            effect.state = NSVisualEffectStateActive;
            effect.wantsLayer = YES;
            effect.layer.cornerRadius = kPanelRadius;
            effect.layer.masksToBounds = YES;
            holder.translatesAutoresizingMaskIntoConstraints = NO;
            [effect addSubview:holder];
            [NSLayoutConstraint activateConstraints:@[
                [holder.leadingAnchor constraintEqualToAnchor:effect.leadingAnchor],
                [holder.trailingAnchor constraintEqualToAnchor:effect.trailingAnchor],
                [holder.topAnchor constraintEqualToAnchor:effect.topAnchor],
                [holder.bottomAnchor constraintEqualToAnchor:effect.bottomAnchor],
            ]];
            _content = effect;
        }
        _panel.contentView = _content;
    }
    return self;
}

- (BOOL)isVisible {
    return _panel.isVisible;
}

- (BOOL)containsScreenPoint:(NSPoint)point {
    return _panel.isVisible && NSPointInRect(point, _panel.frame);
}

- (void)hide {
    _showToken++;
    [_panel orderOut:nil];
}

// Caret bounds in Cocoa screen coordinates, from the focused element's
// selected text range; falls back to the mouse when an app does not tell.
+ (NSRect)caretRect {
    NSRect result = NSZeroRect;
    AXUIElementRef system = AXUIElementCreateSystemWide();
    if (system != NULL) {
        AXUIElementSetMessagingTimeout(system, 0.05f);
        CFTypeRef focused = NULL;
        if (AXUIElementCopyAttributeValue(system, kAXFocusedUIElementAttribute, &focused) == kAXErrorSuccess && focused != NULL) {
            AXUIElementRef element = (AXUIElementRef)focused;
            AXUIElementSetMessagingTimeout(element, 0.05f);
            CFTypeRef range = NULL;
            if (AXUIElementCopyAttributeValue(element, kAXSelectedTextRangeAttribute, &range) == kAXErrorSuccess && range != NULL) {
                CFTypeRef bounds = NULL;
                if (AXUIElementCopyParameterizedAttributeValue(element, kAXBoundsForRangeParameterizedAttribute, range, &bounds) == kAXErrorSuccess &&
                    bounds != NULL) {
                    CGRect rect;
                    if (AXValueGetValue((AXValueRef)bounds, (AXValueType)kAXValueCGRectType, &rect) && rect.size.height > 0)
                        result = rect;
                    CFRelease(bounds);
                }
                CFRelease(range);
            }
            CFRelease(focused);
        }
        CFRelease(system);
    }
    if (result.size.height > 0) {
        // Accessibility uses a top-left origin on the primary display.
        CGFloat primaryHeight = NSHeight(NSScreen.screens.firstObject.frame);
        result.origin.y = primaryHeight - result.origin.y - result.size.height;
        return result;
    }
    NSPoint mouse = [NSEvent mouseLocation];
    return NSMakeRect(mouse.x, mouse.y - 20, 1, 20);
}

- (void)rebuildWithComposition:(OKRimeComposition *)composition {
    for (NSView *view in [_stack.arrangedSubviews copy]) {
        [_stack removeArrangedSubview:view];
        [view removeFromSuperview];
    }
    NSTextField *preedit = [NSTextField labelWithString:composition.preedit];
    preedit.font = [NSFont systemFontOfSize:13];
    preedit.textColor = [NSColor secondaryLabelColor];
    NSView *preeditRow = [[NSView alloc] init];
    preedit.translatesAutoresizingMaskIntoConstraints = NO;
    [preeditRow addSubview:preedit];
    [NSLayoutConstraint activateConstraints:@[
        [preedit.leadingAnchor constraintEqualToAnchor:preeditRow.leadingAnchor constant:8],
        [preedit.trailingAnchor constraintLessThanOrEqualToAnchor:preeditRow.trailingAnchor constant:-8],
        [preedit.topAnchor constraintEqualToAnchor:preeditRow.topAnchor],
        [preedit.bottomAnchor constraintEqualToAnchor:preeditRow.bottomAnchor],
    ]];
    [_stack addArrangedSubview:preeditRow];

    if (composition.candidates.count > 0) {
        NSMutableArray *cells = [NSMutableArray array];
        __weak OKCandidatePanel *weakSelf = self;
        [composition.candidates enumerateObjectsUsingBlock:^(NSString *text, NSUInteger i, BOOL *stop) {
            NSString *label = i < composition.selectLabels.length
                ? [composition.selectLabels substringWithRange:NSMakeRange(i, 1)] : [NSString stringWithFormat:@"%lu", (unsigned long)i + 1];
            NSString *comment = i < composition.comments.count ? composition.comments[i] : @"";
            OKCandidateCell *cell = [[OKCandidateCell alloc] initWithLabel:label text:text comment:comment
                                                               highlighted:(NSInteger)i == composition.highlightedIndex];
            cell.index = i;
            cell.onClick = ^(NSInteger index) {
                if (weakSelf.onSelect) weakSelf.onSelect(index);
            };
            [cells addObject:cell];
        }];
        // Page indicator: chevrons dim at the first and last page.
        if (composition.pageNumber > 0 || !composition.lastPage) {
            NSTextField *previous = [NSTextField labelWithString:@"‹"];
            NSTextField *next = [NSTextField labelWithString:@"›"];
            for (NSTextField *chevron in @[previous, next]) chevron.font = [NSFont systemFontOfSize:15 weight:NSFontWeightSemibold];
            previous.textColor = composition.pageNumber > 0 ? [NSColor secondaryLabelColor] : [NSColor quaternaryLabelColor];
            next.textColor = !composition.lastPage ? [NSColor secondaryLabelColor] : [NSColor quaternaryLabelColor];
            NSStackView *pager = [NSStackView stackViewWithViews:@[previous, next]];
            pager.spacing = 6;
            [cells addObject:pager];
        }
        NSStackView *row = [NSStackView stackViewWithViews:cells];
        row.spacing = 2;
        row.alignment = NSLayoutAttributeCenterY;
        [_stack addArrangedSubview:row];
    }
}

- (void)placeNearCaret {
    [_content layoutSubtreeIfNeeded];
    NSSize size = _stack.fittingSize;
    size.width = MAX(ceil(size.width), 120);
    size.height = ceil(size.height);
    NSScreen *screen = NSScreen.mainScreen;
    for (NSScreen *candidate in NSScreen.screens) {
        if (NSIntersectsRect(candidate.frame, _caret)) { screen = candidate; break; }
    }
    NSRect visible = screen.visibleFrame;
    // Below the caret, or above it when there is no room underneath.
    NSPoint origin = NSMakePoint(NSMinX(_caret) - 8, NSMinY(_caret) - size.height - 6);
    if (origin.y < NSMinY(visible)) origin.y = NSMaxY(_caret) + 6;
    origin.x = MIN(MAX(origin.x, NSMinX(visible) + 4), NSMaxX(visible) - size.width - 4);
    origin.y = MIN(MAX(origin.y, NSMinY(visible) + 4), NSMaxY(visible) - size.height - 4);
    [_panel setFrame:NSMakeRect(origin.x, origin.y, size.width, size.height) display:YES];
}

- (void)showComposition:(OKRimeComposition *)composition {
    if (composition == nil) {
        [self hide];
        return;
    }
    [self rebuildWithComposition:composition];
    if (_panel.isVisible) {
        [self placeNearCaret];
        return;
    }
    // Locate the caret after the event tap returns; Accessibility queries to
    // the focused app must not delay key handling.
    NSInteger token = ++_showToken;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (token != self->_showToken) return;
        self->_caret = [OKCandidatePanel caretRect];
        [self placeNearCaret];
        [self->_panel orderFrontRegardless];
    });
}

@end
