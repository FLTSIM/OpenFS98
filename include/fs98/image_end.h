/* FS98_IMAGE_END(obj): a local assembler symbol <obj>_end (the assembler name of obj plus "_end") equal to obj + sizeof obj, so that
 * tools/check_data_image.py reads each compared image object's real extent from the linked binary.  The compiler evaluates sizeof.
 * It adds no section bytes and no export: the end symbol is a local alias (never .globl), and the helper function holding the
 * directive is empty, byte-aligned and has a .L assembler name (a local label).  GCC 13 rejects file-scope extended asm, hence the helper.
 * Use it once per image object, in the TU that defines it, outside initializers.  A helper changes a TU's .eh_frame where the TU is
 * built with unwind tables: HOST puts FS98_IMAGE_END_IN(obj), the bare directive, at the start of an existing function (DllMain), and
 * SIMSUI's image TU is built with -fno-asynchronous-unwind-tables.  FS98_IMAGE_END_IN must sit in an entry or exported function that every
 * build of the TU keeps (not a static one the linker or LTO may drop); a dropped marker is caught as "no end symbol".  tools/data_images.txt names each object (obj=) and its window. */
#ifndef OPENFS98_IMAGE_END_H
#define OPENFS98_IMAGE_END_H
#define FS98_IMAGE_END_IN(obj) __asm__ volatile(".set %c0_end, %c0 + %c1" : : "i"(&(obj)), "i"(sizeof(obj)))
#define FS98_IMAGE_END(obj) \
    __attribute__((used, aligned(1))) static void fs98_image_end_##obj(void) __asm__(".Lfs98_image_end_" #obj); \
    static void fs98_image_end_##obj(void) { FS98_IMAGE_END_IN(obj); __builtin_unreachable(); }
#endif
