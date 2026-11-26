/* FS98_RETAIL_ABI: the win32-retail target, where retail's register contracts are kept (AD-4).
 * The register-keeping FS98_ENTRY wrappers, the .S entry stubs, FS98_STDCALL and FS98_RETAIL_PTR
 * build only under it.  Only preprocessor lines, so assembler files may include it.
 *
 * INTERIM PROXY: Windows x86 stands in for the target while the Windows native target is x64.  A
 * future Windows-i386 native target needs explicit build selection before this can stay. */
#if defined(_WIN32) && defined(__i386__)
#  define FS98_RETAIL_ABI 1
#endif
