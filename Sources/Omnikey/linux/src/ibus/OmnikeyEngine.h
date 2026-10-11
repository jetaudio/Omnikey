//
//  OmnikeyEngine.h
//  Omnikey - Linux IBus engine
//

#ifndef OMNIKEY_IBUS_ENGINE_H
#define OMNIKEY_IBUS_ENGINE_H

#include <ibus.h>

G_BEGIN_DECLS

#define OMNIKEY_TYPE_ENGINE (omnikey_engine_get_type())
GType omnikey_engine_get_type(void);

// Loads settings and macros and starts watching the config directory.
void omnikey_engine_global_init(void);

G_END_DECLS

#endif
