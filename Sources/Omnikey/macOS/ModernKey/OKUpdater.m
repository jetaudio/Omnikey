#import "OKUpdater.h"
#import <CommonCrypto/CommonDigest.h>

static BOOL updating;
static NSError *UpdateError(NSString *message) {
    return [NSError errorWithDomain:@"OmnikeyUpdater" code:1 userInfo:@{NSLocalizedDescriptionKey:message}];
}
// All blocking work runs on the worker queue, never on the UI thread.
static NSData *Fetch(NSString *address, NSError **error) {
    NSURL *url = [NSURL URLWithString:address];
    if (![url.scheme isEqual:@"https"]) { *error = UpdateError(@"Địa chỉ tải không hợp lệ."); return nil; }
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
    request.timeoutInterval = 120;
    [request setValue:@"Omnikey-macOS-Updater" forHTTPHeaderField:@"User-Agent"];
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    __block NSData *result;
    __block NSError *failure;
    NSURLSessionConfiguration *config = [NSURLSessionConfiguration ephemeralSessionConfiguration];
    config.timeoutIntervalForResource = 600;
    NSURLSession *session = [NSURLSession sessionWithConfiguration:config];
    [[session dataTaskWithRequest:request completionHandler:^(NSData *data, NSURLResponse *response, NSError *err) {
        failure = err;
        if (!failure && ((NSHTTPURLResponse *)response).statusCode != 200)
            failure = UpdateError(@"Máy chủ chưa cung cấp bản cập nhật. Hãy thử lại sau.");
        if (!failure) result = data;
        dispatch_semaphore_signal(done);
    }] resume];
    dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
    [session finishTasksAndInvalidate];
    if (!result) *error = failure ?: UpdateError(@"Không tải được bản cập nhật.");
    return result;
}
static BOOL Run(NSString *program, NSArray *arguments, NSError **error) {
    NSTask *task = [NSTask new];
    task.executableURL = [NSURL fileURLWithPath:program];
    task.arguments = arguments;
    // Output is discarded to avoid a full pipe blocking the subprocess.
    task.standardOutput = [NSFileHandle fileHandleWithNullDevice];
    task.standardError = [NSFileHandle fileHandleWithNullDevice];
    if (![task launchAndReturnError:error]) return NO;
    [task waitUntilExit];
    if (task.terminationStatus != 0) { *error = UpdateError(@"Không thể xác minh hoặc chuẩn bị ứng dụng mới."); return NO; }
    return YES;
}
static BOOL ReleaseURL(NSString *value) {
    return [value isKindOfClass:NSString.class] &&
        [value hasPrefix:@"https://github.com/jetaudio/Omnikey/releases/download/"];
}
static BOOL VerifyChecksum(NSData *image, NSString *expected) {
    if (expected.length != 64 || [expected rangeOfCharacterFromSet:[[NSCharacterSet characterSetWithCharactersInString:@"0123456789abcdef"] invertedSet]].location != NSNotFound) return NO;
    CC_SHA256_CTX ctx; CC_SHA256_Init(&ctx);
    const unsigned char *bytes = image.bytes;
    for (NSUInteger offset = 0; offset < image.length;) {
        CC_LONG count = (CC_LONG)MIN(image.length - offset, 1024 * 1024);
        CC_SHA256_Update(&ctx, bytes + offset, count); offset += count;
    }
    unsigned char digest[CC_SHA256_DIGEST_LENGTH]; CC_SHA256_Final(digest, &ctx);
    NSMutableString *actual = [NSMutableString new];
    for (int i = 0; i < CC_SHA256_DIGEST_LENGTH; i++) [actual appendFormat:@"%02x", digest[i]];
    return [expected isEqual:actual];
}
static BOOL NewerBundle(NSDictionary *info, NSString *identifier, NSInteger current) {
    return [info[@"CFBundleIdentifier"] isEqual:identifier] &&
        [info[@"CFBundleVersion"] isKindOfClass:NSString.class] &&
        [info[@"CFBundleVersion"] integerValue] > current;
}
@implementation OKUpdater
+ (void)start {
    if (updating) return;
    NSString *target = NSBundle.mainBundle.bundlePath.stringByStandardizingPath;
    NSFileManager *fm = NSFileManager.defaultManager;
    NSString *parent = target.stringByDeletingLastPathComponent;
    if ([target hasPrefix:@"/Volumes/"] || ![fm isWritableFileAtPath:parent] ||
        ![fm isWritableFileAtPath:target] || ![target isEqual:target.stringByResolvingSymlinksInPath]) {
        NSAlert *alert = [NSAlert new];
        alert.messageText = @"Chưa thể cập nhật tại vị trí này";
        alert.informativeText = @"Hãy đặt Omnikey trong Applications hoặc thư mục có quyền ghi, rồi thử lại.";
        [alert runModal]; return;
    }
    updating = YES;
    NSAlert *progress = [NSAlert new];
    progress.messageText = @"Đang tải và xác minh bản cập nhật…";
    progress.informativeText = @"Omnikey sẽ tự mở lại sau khi cài xong. Bạn vẫn có thể tiếp tục gõ trong lúc tải.";
    [progress addButtonWithTitle:@"Đang cập nhật…"].enabled = NO;
    NSProgressIndicator *spinner = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(0, 0, 260, 20)];
    spinner.style = NSProgressIndicatorStyleBar; spinner.indeterminate = YES;
    [spinner startAnimation:nil]; progress.accessoryView = spinner;
    [progress.window center]; [progress.window makeKeyAndOrderFront:nil];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
        NSError *error = nil;
        NSString *work = [NSTemporaryDirectory() stringByAppendingPathComponent:[@"OmnikeyUpdate-" stringByAppendingString:NSUUID.UUID.UUIDString]];
        NSString *mount = [work stringByAppendingPathComponent:@"mount"];
        NSString *stage = [parent stringByAppendingPathComponent:[NSString stringWithFormat:@".Omnikey-update-%@.app", NSUUID.UUID.UUIDString]];
        BOOL mounted = NO, ready = NO;
        NSString *helper = [work stringByAppendingPathComponent:@"install.sh"];
        do {
            if (![fm createDirectoryAtPath:mount withIntermediateDirectories:YES attributes:@{NSFilePosixPermissions:@0700} error:&error]) break;
            NSData *metadata = Fetch(@"https://api.github.com/repos/jetaudio/Omnikey/releases/latest", &error);
            if (!metadata) break;
            id release = [NSJSONSerialization JSONObjectWithData:metadata options:0 error:&error];
            if (![release isKindOfClass:NSDictionary.class] || ![release[@"assets"] isKindOfClass:NSArray.class] || [release[@"draft"] boolValue] || [release[@"prerelease"] boolValue]) {
                error = UpdateError(@"Dữ liệu bản phát hành không hợp lệ."); break;
            }
            NSDictionary *dmg = nil, *checksum = nil;
            for (id asset in release[@"assets"]) {
                if (![asset isKindOfClass:NSDictionary.class] || ![asset[@"name"] isKindOfClass:NSString.class]) continue;
                NSString *name = asset[@"name"];
                if ([name hasPrefix:@"Omnikey-"] && [name hasSuffix:@"-universal.dmg"]) {
                    if (dmg) { error = UpdateError(@"Bản phát hành có nhiều DMG; không thể chọn an toàn."); break; }
                    dmg = asset;
                }
            }
            if (error) break;
            for (id asset in release[@"assets"]) {
                if ([asset isKindOfClass:NSDictionary.class] && [asset[@"name"] isEqual:[dmg[@"name"] stringByAppendingString:@".sha256"]]) checksum = asset;
            }
            if (!ReleaseURL(dmg[@"browser_download_url"]) || !ReleaseURL(checksum[@"browser_download_url"])) {
                error = UpdateError(@"Bản phát hành cần có DMG universal và file .dmg.sha256 đi kèm."); break;
            }
            NSData *sumData = Fetch(checksum[@"browser_download_url"], &error);
            if (!sumData) break;
            NSString *sumText = [[NSString alloc] initWithData:sumData encoding:NSUTF8StringEncoding];
            NSArray<NSString *> *tokens = [sumText componentsSeparatedByCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
            NSString *expected = tokens.firstObject.lowercaseString;
            if (expected.length != 64 || [expected rangeOfCharacterFromSet:[[NSCharacterSet characterSetWithCharactersInString:@"0123456789abcdef"] invertedSet]].location != NSNotFound) {
                error = UpdateError(@"Checksum của bản phát hành không hợp lệ."); break;
            }
            NSData *image = Fetch(dmg[@"browser_download_url"], &error);
            if (!image) break;
            if (!VerifyChecksum(image, expected)) { error = UpdateError(@"Checksum không khớp. Bản đang dùng được giữ nguyên."); break; }
            NSString *imagePath = [work stringByAppendingPathComponent:@"update.dmg"];
            if (![image writeToFile:imagePath options:NSDataWritingAtomic error:&error]) break;
            if (!Run(@"/usr/bin/hdiutil", @[@"attach", @"-readonly", @"-nobrowse", @"-mountpoint", mount, imagePath], &error)) break;
            mounted = YES;
            NSString *app = [mount stringByAppendingPathComponent:@"Omnikey.app"];
            NSDictionary *info = [NSDictionary dictionaryWithContentsOfFile:[app stringByAppendingPathComponent:@"Contents/Info.plist"]];
            NSInteger current = [NSBundle.mainBundle.infoDictionary[@"CFBundleVersion"] integerValue];
            if (!NewerBundle(info, NSBundle.mainBundle.bundleIdentifier, current)) {
                error = UpdateError(@"DMG không chứa Omnikey có mã phiên bản mới hơn bản đang chạy."); break;
            }
            if (!Run(@"/usr/bin/codesign", @[@"--verify", @"--deep", @"--strict", app], &error)) break;
            NSString *binary = [app stringByAppendingPathComponent:@"Contents/MacOS/Omnikey"];
#if defined(__arm64__)
            NSString *arch = @"arm64";
#else
            NSString *arch = @"x86_64";
#endif
            if (!Run(@"/usr/bin/lipo", @[binary, @"-verify_arch", arch], &error)) break;
            if (!Run(@"/usr/bin/ditto", @[app, stage], &error)) break;
            if (!Run(@"/usr/bin/codesign", @[@"--verify", @"--deep", @"--strict", stage], &error)) break;
            NSString *bundledHelper = [NSBundle.mainBundle pathForResource:@"update-install" ofType:@"sh"];
            if (!bundledHelper || ![fm copyItemAtPath:bundledHelper toPath:helper error:&error]) {
                if (!error) error = UpdateError(@"Không tìm thấy trình cài cập nhật."); break;
            }
            ready = YES;
        } while (NO);
        if (mounted) { NSError *detachError; Run(@"/usr/bin/hdiutil", @[@"detach", mount], &detachError); }
        dispatch_async(dispatch_get_main_queue(), ^{
            [progress.window orderOut:nil];
            NSError *launchError = error;
            if (ready) {
                NSTask *installer = [NSTask new];
                installer.executableURL = [NSURL fileURLWithPath:@"/bin/bash"];
                installer.arguments = @[helper, stage, target, [NSString stringWithFormat:@"%d", NSProcessInfo.processInfo.processIdentifier], work];
                NSString *logs = [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Logs/Omnikey"];
                [fm createDirectoryAtPath:logs withIntermediateDirectories:YES attributes:nil error:nil];
                NSString *log = [logs stringByAppendingPathComponent:@"updater.log"];
                [fm createFileAtPath:log contents:nil attributes:nil];
                NSFileHandle *handle = [NSFileHandle fileHandleForWritingAtPath:log];
                installer.standardOutput = handle ?: NSFileHandle.fileHandleWithNullDevice;
                installer.standardError = installer.standardOutput;
                if ([installer launchAndReturnError:&launchError]) {
                    [NSUserDefaults.standardUserDefaults synchronize];
                    [NSApp terminate:nil]; return;
                }
            }
            updating = NO;
            [fm removeItemAtPath:stage error:nil]; [fm removeItemAtPath:work error:nil];
            NSAlert *alert = [NSAlert new]; alert.messageText = @"Không thể cập nhật Omnikey";
            alert.informativeText = launchError.localizedDescription ?: @"Hãy thử lại sau.";
            [alert runModal];
        });
    });
}
@end
