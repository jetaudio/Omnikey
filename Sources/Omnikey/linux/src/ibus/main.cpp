//
//  main.cpp
//  Omnikey - Linux IBus engine
//
//  ibus-daemon starts this with --ibus (see omnikey.xml). Without it the engine
//  registers its own component, which is handy for testing from a terminal.
//

#include "OmnikeyEngine.h"

#include <cstring>
#include <cstdio>
#include <unistd.h>

#ifndef OMNIKEY_VERSION
#define OMNIKEY_VERSION "dev"
#endif

static void onDisconnected(IBusBus*, gpointer) {
    ibus_quit();
}

int main(int argc, char** argv) {
    gboolean startedByIBus = FALSE;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ibus") == 0 || strcmp(argv[i], "-i") == 0) startedByIBus = TRUE;
        if (strcmp(argv[i], "--version") == 0) {
            printf("ibus-engine-omnikey %s\n", OMNIKEY_VERSION);
            return 0;
        }
    }

    ibus_init();
    IBusBus* bus = ibus_bus_new();
    if (!ibus_bus_is_connected(bus)) {
        g_printerr("ibus-engine-omnikey: cannot connect to ibus-daemon\n");
        return 1;
    }
    g_object_ref_sink(bus);
    g_signal_connect(bus, "disconnected", G_CALLBACK(onDisconnected), NULL);

    omnikey_engine_global_init();

    IBusFactory* factory = ibus_factory_new(ibus_bus_get_connection(bus));
    g_object_ref_sink(factory);

    if (startedByIBus) {
        ibus_factory_add_engine(factory, "omnikey", OMNIKEY_TYPE_ENGINE);
        ibus_bus_request_name(bus, "org.freedesktop.IBus.Omnikey", 0);
    } else {
        // ibus-daemon keeps a component and its engines after the process exits,
        // so a test run registers under fresh names each time.
        gchar* name = g_strdup_printf("org.freedesktop.IBus.Omnikey.Dev%d", (int)getpid());
        gchar* engineName = g_strdup_printf("omnikey-dev%d", (int)getpid());
        ibus_factory_add_engine(factory, engineName, OMNIKEY_TYPE_ENGINE);
        printf("%s\n", engineName);
        fflush(stdout);
        IBusComponent* component = ibus_component_new(name,
            "Omnikey Vietnamese input method", OMNIKEY_VERSION, "GPL-3.0",
            "jetaudio, Mai Vu Tuyen", "https://github.com/jetaudio/Omnikey", "", "ibus-omnikey");
        ibus_component_add_engine(component, ibus_engine_desc_new(engineName, "Omnikey (dev)",
            "Bộ gõ tiếng Việt Omnikey", "vi", "GPL-3.0", "jetaudio, Mai Vu Tuyen", "omnikey", "us"));
        if (!ibus_bus_register_component(bus, component))
            g_printerr("ibus-engine-omnikey: RegisterComponent failed\n");
        g_free(name);
        g_free(engineName);
    }

    ibus_main();
    g_object_unref(factory);
    g_object_unref(bus);
    return 0;
}
