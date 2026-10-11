//
//  OKFormUI.h
//  OpenKey
//
//  Building blocks for the System Settings style windows: rounded groups of
//  rows, switches mirrored from storyboard checkboxes and scrolling pages.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

// Document view of a scrolling page; flipped so short pages hug the top.
@interface OKFlippedView : NSView
@end

// Rounded inset group, as used by System Settings.
@interface OKFormGroupView : NSView
@end

// Rounded, bordered container that clips its content (tables, text).
@interface OKRoundedContainerView : NSView
@end

// Mirrors storyboard buttons (which keep their actions and the state written
// by fillData) onto a modern control: an NSSwitch, a checkbox or the segments
// of an NSSegmentedControl. The sources are retained because they leave the
// view hierarchy while their outlets stay weak.
@interface OKControlMirror : NSObject
- (instancetype)initWithSources:(NSArray<NSButton *> *)sources control:(NSControl *)control;
@end

@interface OKFormBuilder : NSObject
@property (nonatomic, readonly) CGFloat width;
- (instancetype)initWithWidth:(CGFloat)width;

- (NSTextField *)labelWithString:(NSString *)string font:(NSFont *)font color:(NSColor *)color;
- (NSView *)rowWithTitle:(NSString *)title detail:(nullable NSString *)detail accessory:(nullable NSView *)accessory;
// A switch row driven by a storyboard checkbox.
- (NSView *)toggleRowForButton:(NSButton *)button title:(NSString *)title detail:(nullable NSString *)detail;
- (NSView *)groupWithRows:(NSArray<NSView *> *)rows;
- (void)addSection:(nullable NSString *)header rows:(NSArray<NSView *> *)rows note:(nullable NSString *)note toStack:(NSStackView *)stack;

// Vertical stack with page margins; wrap it with scrollPageWithStack: or pin it.
- (NSStackView *)pageStack;
- (NSScrollView *)scrollPageWithStack:(NSStackView *)stack;

// One multi-select segmented control for a set of modifier checkboxes.
- (NSSegmentedControl *)modifierControlForButtons:(NSArray<NSButton *> *)buttons labels:(NSArray<NSString *> *)labels;
- (void)prepareKeyField:(NSTextField *)field;
- (void)prepareInlinePopup:(NSPopUpButton *)popup;

// Icon, name, tagline and version line of an About view.
- (NSArray<NSView *> *)aboutHeaderWithVersionField:(NSTextField *)version;

+ (NSButton *)pushButtonWithTitle:(NSString *)title target:(nullable id)target action:(nullable SEL)action;
+ (NSButton *)linkButtonWithTitle:(NSString *)title target:(nullable id)target action:(nullable SEL)action;
+ (void)preparePushButton:(NSButton *)button;
@end

NS_ASSUME_NONNULL_END
