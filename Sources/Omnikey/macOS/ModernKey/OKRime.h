//
//  OKRime.h
//  OpenKey
//
//  Chinese Pinyin input through librime (https://github.com/rime/librime),
//  loaded at run time from Omnikey.app/Contents/Frameworks. All calls except
//  -start happen on the main thread, where the event tap runs.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

// Snapshot of the composition shown in the candidate panel.
@interface OKRimeComposition : NSObject
@property (nonatomic, copy) NSString *preedit;
@property (nonatomic, copy) NSArray<NSString *> *candidates;
@property (nonatomic, copy) NSArray<NSString *> *comments;
@property (nonatomic, copy) NSString *selectLabels;
@property (nonatomic) NSInteger highlightedIndex;
@property (nonatomic) NSInteger pageNumber;
@property (nonatomic) BOOL lastPage;
@end

@interface OKRime : NSObject

// Uses the library and data embedded in the app bundle.
+ (instancetype)shared;
- (instancetype)initWithLibrary:(NSString *)library sharedData:(NSString *)sharedData
                       prebuilt:(NSString *)prebuilt userData:(NSString *)userData NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Loads and deploys Rime on a background queue; completion runs on the main
// queue. Safe to call repeatedly.
- (void)startWithCompletion:(nullable void (^)(BOOL ready))completion;
@property (nonatomic, readonly) BOOL ready;
@property (nonatomic, readonly, nullable) NSString *failureReason;

// Feeds one X11 keysym with Rime modifier mask; YES when Rime consumed it.
- (BOOL)processKeysym:(int)keysym mask:(int)mask;
// Text Rime committed since the last call, if any.
- (nullable NSString *)takeCommit;
// Current composition, or nil when nothing is being composed.
- (nullable OKRimeComposition *)composition;
// Same as composition != nil, without building the snapshot.
- (BOOL)isComposing;
- (BOOL)selectCandidateOnCurrentPage:(NSInteger)index;
- (void)clearComposition;

// Translates a macOS key-down event; returns 0 for keys Rime should not see.
// Control+letter reaches Rime only while composing (⌃K forgets a learned
// phrase); otherwise Control and Command shortcuts belong to the application.
+ (int)keysymForEvent:(CGEventRef)event keyCode:(CGKeyCode)keyCode flags:(CGEventFlags)flags
            composing:(BOOL)composing mask:(int *)mask;

@end

NS_ASSUME_NONNULL_END
