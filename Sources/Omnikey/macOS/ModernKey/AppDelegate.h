//
//  AppDelegate.h
//  ModernKey
//
//  Created by Tuyen on 1/18/19.
//  Copyright © 2019 Tuyen Mai. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "ViewController.h"

#define OMNIKEY_BUNDLE @"io.github.jetaudio.omnikey"

@interface AppDelegate : NSObject <NSApplicationDelegate>

-(void)setInputMethod:(int)targetLanguage willNotify:(BOOL)willNotify;

-(void)onImputMethodChanged:(BOOL)willNotify;
-(void)onInputMethodSelected;

// Input modes: 0 = English, 1 = Vietnamese, 2 = Chinese (Pinyin).
-(int)currentInputMode;
-(void)selectInputMode:(int)mode;

-(void)askPermission;

-(void)onInputTypeSelectedIndex:(int)index;
-(void)onCodeTableChanged:(int)index;

-(void)setRunOnStartup:(BOOL)val;
-(void)loadDefaultConfig;

-(void)setGrayIcon:(BOOL)val;

-(void)onMacroSelected;
-(void)onQuickConvert;
-(void)setQuickConvertString;

-(void)showIconOnDock:(BOOL)val;
@end
