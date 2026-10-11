//
//  VirtualKeyboard.cpp
//  Omnikey - Linux IBus engine
//

#include "VirtualKeyboard.h"

#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <sys/ioctl.h>
#include <linux/uinput.h>

static int fd = -1;

static void emit(int type, int code, int value) {
    struct input_event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.code = code;
    event.value = value;
    if (write(fd, &event, sizeof(event)) < 0) { /* the compositor drops the key; nothing to recover */ }
}

namespace VirtualKeyboard {

bool open() {
    if (fd >= 0) return true;
    fd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return false;

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_SYN);
    // A full key range makes udev classify the device as a keyboard.
    for (int key = KEY_ESC; key <= KEY_MICMUTE; key++) ioctl(fd, UI_SET_KEYBIT, key);

    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor = 0x4f4b; // "OK"
    setup.id.product = 0x0001;
    strncpy(setup.name, "Omnikey virtual keyboard", UINPUT_MAX_NAME_SIZE - 1);
    if (ioctl(fd, UI_DEV_SETUP, &setup) < 0 || ioctl(fd, UI_DEV_CREATE) < 0) {
        ::close(fd);
        fd = -1;
        return false;
    }
    return true;
}

bool isOpen() {
    return fd >= 0;
}

void close() {
    if (fd < 0) return;
    ioctl(fd, UI_DEV_DESTROY);
    ::close(fd);
    fd = -1;
}

void tap(int keycode) {
    if (fd < 0) return;
    emit(EV_KEY, keycode, 1);
    emit(EV_SYN, SYN_REPORT, 0);
    emit(EV_KEY, keycode, 0);
    emit(EV_SYN, SYN_REPORT, 0);
}

}
