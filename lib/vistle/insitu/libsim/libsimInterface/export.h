#ifndef VISTLE_INSITU_LIBSIM_LIBSIMINTERFACE_EXPORT_H
#define VISTLE_INSITU_LIBSIM_LIBSIMINTERFACE_EXPORT_H

#include <vistle/util/export.h>

#if defined(vistle_libsim_EXPORTS)
#define V_LIBSIMXPORT V_EXPORT
#else
#define V_LIBSIMXPORT V_IMPORT
#endif

#endif
