//
//  OKRime.mm
//  OpenKey
//

#import "OKRime.h"
#import <Carbon/Carbon.h>
#include <dlfcn.h>
#include "../Rime/rime_api.h"

// Rime modifier masks (X11 values used by librime).
static const int kRimeShiftMask = 1 << 0;
static const int kRimeControlMask = 1 << 2;
static const int kRimeAltMask = 1 << 3;

@implementation OKRimeComposition
@end

@implementation OKRime {
    NSString *_library, *_sharedData, *_prebuilt, *_userData;
    void *_handle;
    RimeApi *_api;
    RimeSessionId _session;
    BOOL _starting;
    NSMutableArray *_completions;
}

+ (instancetype)shared {
    static OKRime *instance;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        NSBundle *bundle = [NSBundle mainBundle];
        NSString *rime = [bundle.resourcePath stringByAppendingPathComponent:@"Rime"];
        NSString *support = [NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES).firstObject
                             stringByAppendingPathComponent:@"Omnikey/Rime"];
        instance = [[OKRime alloc] initWithLibrary:[bundle.privateFrameworksPath stringByAppendingPathComponent:@"librime.1.dylib"]
                                        sharedData:[rime stringByAppendingPathComponent:@"shared"]
                                          prebuilt:[rime stringByAppendingPathComponent:@"build"]
                                          userData:support];
    });
    return instance;
}

- (instancetype)initWithLibrary:(NSString *)library sharedData:(NSString *)sharedData
                       prebuilt:(NSString *)prebuilt userData:(NSString *)userData {
    if (self = [super init]) {
        _library = [library copy];
        _sharedData = [sharedData copy];
        _prebuilt = [prebuilt copy];
        _userData = [userData copy];
        _completions = [NSMutableArray array];
    }
    return self;
}

- (void)finishStart:(BOOL)ready reason:(NSString *)reason {
    _starting = NO;
    _failureReason = reason;
    _ready = ready;
    if (!ready) NSLog(@"Omnikey: Chinese input unavailable: %@", reason);
    NSArray *completions = [_completions copy];
    [_completions removeAllObjects];
    for (void (^completion)(BOOL) in completions) completion(ready);
}

- (void)startWithCompletion:(void (^)(BOOL))completion {
    if (_ready || _failureReason != nil) {
        if (completion) completion(_ready);
        return;
    }
    if (completion) [_completions addObject:[completion copy]];
    if (_starting) return;
    _starting = YES;
    NSString *library = _library, *sharedData = _sharedData, *prebuilt = _prebuilt, *userData = _userData;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSString *reason = nil;
        void *handle = dlopen(library.fileSystemRepresentation, RTLD_NOW | RTLD_LOCAL);
        RimeApi *api = NULL;
        if (handle == NULL) {
            reason = [NSString stringWithFormat:@"cannot load librime: %s", dlerror()];
        } else {
            RimeApi *(*getApi)(void) = (RimeApi *(*)(void))dlsym(handle, "rime_get_api");
            api = getApi ? getApi() : NULL;
            if (api == NULL) reason = @"librime has no rime_get_api";
        }
        if (api != NULL) {
            [[NSFileManager defaultManager] createDirectoryAtPath:userData withIntermediateDirectories:YES attributes:nil error:nil];
            NSString *staging = [userData stringByAppendingPathComponent:@"build"];
            RIME_STRUCT(RimeTraits, traits);
            traits.shared_data_dir = sharedData.fileSystemRepresentation;
            traits.user_data_dir = userData.fileSystemRepresentation;
            traits.prebuilt_data_dir = prebuilt.fileSystemRepresentation;
            traits.staging_dir = staging.fileSystemRepresentation;
            traits.distribution_name = "Omnikey";
            traits.distribution_code_name = "Omnikey";
            traits.distribution_version = "1";
            traits.app_name = "rime.omnikey";
            traits.min_log_level = 2;
            traits.log_dir = "";
            api->setup(&traits);
            api->initialize(&traits);
            if (api->start_maintenance(False)) api->join_maintenance_thread();
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            if (api == NULL) {
                [self finishStart:NO reason:reason];
                return;
            }
            self->_handle = handle;
            self->_api = api;
            self->_session = api->create_session();
            if (self->_session == 0) {
                [self finishStart:NO reason:@"cannot create a Rime session"];
                return;
            }
            [self finishStart:YES reason:nil];
        });
    });
}

- (BOOL)ensureSession {
    if (!_ready) return NO;
    if (!_api->find_session(_session)) _session = _api->create_session();
    return _session != 0;
}

- (BOOL)processKeysym:(int)keysym mask:(int)mask {
    if (keysym == 0 || ![self ensureSession]) return NO;
    return _api->process_key(_session, keysym, mask) ? YES : NO;
}

- (NSString *)takeCommit {
    if (!_ready) return nil;
    RIME_STRUCT(RimeCommit, commit);
    if (!_api->get_commit(_session, &commit)) return nil;
    NSString *text = commit.text ? [NSString stringWithUTF8String:commit.text] : nil;
    _api->free_commit(&commit);
    return text.length > 0 ? text : nil;
}

static BOOL hasComposition(const RimeContext &context) {
    return context.composition.length > 0 || context.menu.num_candidates > 0;
}

- (BOOL)isComposing {
    if (!_ready) return NO;
    RIME_STRUCT(RimeContext, context);
    if (!_api->get_context(_session, &context)) return NO;
    BOOL composing = hasComposition(context);
    _api->free_context(&context);
    return composing;
}

- (OKRimeComposition *)composition {
    if (!_ready) return nil;
    RIME_STRUCT(RimeContext, context);
    if (!_api->get_context(_session, &context)) return nil;
    OKRimeComposition *result = nil;
    if (hasComposition(context)) {
        result = [[OKRimeComposition alloc] init];
        result.preedit = context.composition.preedit ? [NSString stringWithUTF8String:context.composition.preedit] : @"";
        NSMutableArray *candidates = [NSMutableArray array], *comments = [NSMutableArray array];
        for (int i = 0; i < context.menu.num_candidates; i++) {
            RimeCandidate candidate = context.menu.candidates[i];
            [candidates addObject:candidate.text ? [NSString stringWithUTF8String:candidate.text] : @""];
            [comments addObject:candidate.comment ? [NSString stringWithUTF8String:candidate.comment] : @""];
        }
        result.candidates = candidates;
        result.comments = comments;
        result.selectLabels = context.menu.select_keys ? [NSString stringWithUTF8String:context.menu.select_keys] : @"1234567890";
        result.highlightedIndex = context.menu.highlighted_candidate_index;
        result.pageNumber = context.menu.page_no;
        result.lastPage = context.menu.is_last_page ? YES : NO;
    }
    _api->free_context(&context);
    return result;
}

- (BOOL)selectCandidateOnCurrentPage:(NSInteger)index {
    if (![self ensureSession]) return NO;
    return _api->select_candidate_on_current_page(_session, (size_t)index) ? YES : NO;
}

- (void)clearComposition {
    if (_ready) _api->clear_composition(_session);
}

+ (int)keysymForEvent:(CGEventRef)event keyCode:(CGKeyCode)keyCode flags:(CGEventFlags)flags
            composing:(BOOL)composing mask:(int *)mask {
    int modifiers = 0;
    if (flags & kCGEventFlagMaskControl) modifiers |= kRimeControlMask;
    if (flags & kCGEventFlagMaskAlternate) modifiers |= kRimeAltMask;
    // Shift+Delete removes the highlighted phrase from what Rime learned.
    int shift = (flags & kCGEventFlagMaskShift) ? kRimeShiftMask : 0;
    switch (keyCode) {
        case kVK_Delete: *mask = modifiers | shift; return 0xff08;         // BackSpace
        case kVK_ForwardDelete: *mask = modifiers | shift; return 0xffff;  // Delete
        case kVK_Return: case kVK_ANSI_KeypadEnter: *mask = modifiers; return 0xff0d;
        case kVK_Escape: *mask = modifiers; return 0xff1b;
        case kVK_Tab: *mask = modifiers | shift; return 0xff09;
        case kVK_LeftArrow: *mask = modifiers; return 0xff51;
        case kVK_UpArrow: *mask = modifiers; return 0xff52;
        case kVK_RightArrow: *mask = modifiers; return 0xff53;
        case kVK_DownArrow: *mask = modifiers; return 0xff54;
        case kVK_PageUp: *mask = modifiers; return 0xff55;
        case kVK_PageDown: *mask = modifiers; return 0xff56;
        case kVK_Home: *mask = modifiers; return 0xff50;
        case kVK_End: *mask = modifiers; return 0xff57;
        case kVK_Space: *mask = modifiers; return 0x20;
        default: break;
    }
    if (flags & kCGEventFlagMaskCommand) return 0;
    UniChar characters[4];
    UniCharCount length = 0;
    CGEventKeyboardGetUnicodeString(event, 4, &length, characters);
    if (flags & kCGEventFlagMaskControl) {
        // Control turns letters into ASCII control codes 1-26.
        if (!composing || length != 1 || characters[0] < 1 || characters[0] > 26) return 0;
        *mask = modifiers;
        return 'a' + characters[0] - 1;
    }
    // Printable keys: the character already carries Shift (and the layout).
    if (length == 1 && characters[0] > 0x20 && characters[0] < 0x7f) {
        *mask = modifiers;
        return characters[0];
    }
    return 0;
}

@end
