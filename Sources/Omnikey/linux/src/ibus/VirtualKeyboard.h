//
//  VirtualKeyboard.h
//  Omnikey - Linux IBus engine
//
//  A uinput keyboard that sends real key presses through the compositor. Apps
//  such as terminals ignore IBus' delete-surrounding-text and forwarded keys,
//  but every app handles a real BackSpace.
//

#ifndef OMNIKEY_VIRTUAL_KEYBOARD_H
#define OMNIKEY_VIRTUAL_KEYBOARD_H

namespace VirtualKeyboard {
    // Creates the device; returns false when /dev/uinput is not accessible.
    bool open();
    bool isOpen();
    void close();
    // Presses and releases an evdev key code.
    void tap(int keycode);
}

#endif
