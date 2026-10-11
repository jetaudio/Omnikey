//
//  OKCandidatePanel.h
//  OpenKey
//
//  Floating candidate window for Chinese input, shown next to the text caret
//  without taking keyboard focus from the application being typed into.
//

#import <Cocoa/Cocoa.h>
#import "OKRime.h"

NS_ASSUME_NONNULL_BEGIN

@interface OKCandidatePanel : NSObject

+ (instancetype)shared;

// Shows or refreshes the composition. The caret is located when a new
// composition starts; pass nil to hide.
- (void)showComposition:(nullable OKRimeComposition *)composition;
- (void)hide;
@property (nonatomic, readonly, getter=isVisible) BOOL visible;
- (BOOL)containsScreenPoint:(NSPoint)point;

// Called with the index on the current page when a candidate is clicked.
@property (nonatomic, copy, nullable) void (^onSelect)(NSInteger index);

// Exposed for rendering checks.
@property (nonatomic, readonly) NSPanel *panel;

@end

NS_ASSUME_NONNULL_END
