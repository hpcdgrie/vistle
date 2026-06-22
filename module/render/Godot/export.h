#ifndef VISTLE_GODOT_EXPORT_H
#define VISTLE_GODOT_EXPORT_H

#include <vistle/util/export.h>
#if defined(GodotRenderer_EXPORTS)
#define V_GODOTEXPORT V_EXPORT
#else
#define V_GODOTEXPORT V_IMPORT
#endif

#endif
