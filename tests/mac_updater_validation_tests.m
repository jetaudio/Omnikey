// Import the production implementation to test its internal validation gates.
#import "../Sources/Omnikey/macOS/ModernKey/OKUpdater.m"
#include <assert.h>
int main(void) {
    @autoreleasepool {
        NSData *abc = [@"abc" dataUsingEncoding:NSUTF8StringEncoding];
        NSString *hash = @"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
        assert(VerifyChecksum(abc, hash));
        assert(!VerifyChecksum([@"abd" dataUsingEncoding:NSUTF8StringEncoding], hash));
        assert(!VerifyChecksum(abc, @""));
        assert(!VerifyChecksum(abc, [@"g" stringByPaddingToLength:64 withString:@"g" startingAtIndex:0]));
        assert(ReleaseURL(@"https://github.com/jetaudio/Omnikey/releases/download/v2/Omnikey.dmg"));
        assert(!ReleaseURL(@"http://github.com/jetaudio/Omnikey/releases/download/v2/a.dmg"));
        assert(!ReleaseURL(@"https://github.com/other/Omnikey/releases/download/v2/a.dmg"));
        assert(!ReleaseURL((id)@42));
        NSString *identifier = @"io.github.jetaudio.omnikey";
        assert(NewerBundle(@{@"CFBundleIdentifier":identifier, @"CFBundleVersion":@"51"}, identifier, 50));
        assert(!NewerBundle(@{@"CFBundleIdentifier":identifier, @"CFBundleVersion":@"50"}, identifier, 50));
        assert(!NewerBundle(@{@"CFBundleIdentifier":identifier, @"CFBundleVersion":@"49"}, identifier, 50));
        assert(!NewerBundle(@{@"CFBundleIdentifier":@"other", @"CFBundleVersion":@"51"}, identifier, 50));
        assert(!NewerBundle(@{}, identifier, 50));
        puts("Updater validation tests passed");
    }
}
