#ifndef VISTLE_GODOT_TESTGODOTLIB_H
#define VISTLE_GODOT_TESTGODOTLIB_H

#if defined(testGodotLib_EXPORTS)
#define V_GODOTEXPORT _declspec(dllexport)
#else
#define V_GODOTEXPORT __declspec(dllimport)
#endif

V_GODOTEXPORT void testFunction();

#endif // VISTLE_GODOT_TESTGODOTLIB_H
