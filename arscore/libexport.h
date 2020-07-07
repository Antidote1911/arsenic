#ifndef LIBEXPORT_H
#define LIBEXPORT_H

#if defined EXPORT_LIB
#define LIB_EXPORT Q_DECL_EXPORT
#else
#define LIB_EXPORT Q_DECL_IMPORT
#endif

#endif // LIBEXPORT_H
