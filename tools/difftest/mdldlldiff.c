/* Loaded normal-MDL DLL comparison, including generated data and lifecycle.
 * Retail and rebuilt DLLs are independently loaded and initialized. The
 * generated metadata supplies original offsets, not executable payloads.
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct model_reloc { uint32_t site, target; };
struct model_resource { uint32_t type, id, language, size; };
struct model_spec {
    const char *retail, *ours;
    uint32_t kind, data_rva, data_size, imports_rva, linkage_rva, header_rva, active_end;
    const struct model_reloc *relocs;
    uint32_t reloc_count;
    const struct model_resource *resources;
    uint32_t resource_count;
};
#include "modules.inc"

static const struct model_spec *model;
static uint32_t vector, seed = 0x4d444c32;
static uint8_t global_blob[0x2000], global_before[0x2000], host_blob[0x320];
#ifndef MDL_STATE_CAPACITY
#define MDL_STATE_CAPACITY 0x200
#endif
static uint8_t state[MDL_STATE_CAPACITY], initial[MDL_STATE_CAPACITY], expected[MDL_STATE_CAPACITY];
static uint32_t calls[2][3], call_count;
static volatile uint32_t returned[8] __attribute__((used));
static volatile uint32_t before_sp __attribute__((used));

/* Captures every general register, with either zero or one stack argument. */
void model_call(uint32_t fn, uint8_t *arg, uint32_t argc);
__asm__(".text\n.globl _model_call\n_model_call:\n\t"
        "pushl %ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
        "movl 20(%esp), %eax\n\tmovl 24(%esp), %edx\n\tmovl 28(%esp), %ecx\n\t"
        "movl %esp, _before_sp\n\tcmpl $2, %ecx\n\tjne 2f\n\tpushl $0\n2:\n\t"
        "testl %ecx, %ecx\n\tjz 1f\n\tpushl %edx\n1:\n\t"
        "movl $0x31415926, %ebx\n\tmovl $0x27182818, %esi\n\t"
        "movl $0x16180339, %edi\n\tmovl $0x14142135, %ebp\n\t"
        "movl $0x12345678, %ecx\n\tmovl $0x98765432, %edx\n\tcall *%eax\n\t"
        "movl %eax, _returned\n\tmovl %ebx, _returned+4\n\t"
        "movl %ecx, _returned+8\n\tmovl %edx, _returned+12\n\t"
        "movl %esi, _returned+16\n\tmovl %edi, _returned+20\n\t"
        "movl %ebp, _returned+24\n\tmovl %esp, %eax\n\tsubl _before_sp, %eax\n\t"
        "movl %eax, _returned+28\n\t"
        "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tpopl %ebp\n\tret\n");

static uint32_t read32(const void *p)
{ uint32_t value; memcpy(&value, p, 4); return value; }
static void write32(void *p, uint32_t value)
{ memcpy(p, &value, 4); }
static void fail(const char *what, uint32_t off)
{
    fprintf(stderr, "%s vector %u: %s +0x%x\n", model ? model->retail : "MDL", vector, what, off);
    ExitProcess(1);
}
static LONG WINAPI fault(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr, "%s vector %u: exception %08lx at %p\n", model ? model->retail : "MDL",
            vector, exception->ExceptionRecord->ExceptionCode,
            exception->ExceptionRecord->ExceptionAddress);
    ExitProcess(2);
    return EXCEPTION_EXECUTE_HANDLER;
}
static uint32_t rng(void)
{ seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
static void fill(uint8_t *p, uint32_t size)
{ while (size--) *p++ = (uint8_t)rng(); }
static void compare(const void *a, const void *b, uint32_t size, const char *what)
{
    uint32_t i;
    if (!memcmp(a, b, size)) return;
    for (i = 0; i < size; i++) if (((const uint8_t *)a)[i] != ((const uint8_t *)b)[i]) {
        fprintf(stderr, "retail=%02x ours=%02x\n", ((const uint8_t *)a)[i], ((const uint8_t *)b)[i]);
        fail(what, i);
    }
}
static int32_t __attribute__((stdcall)) copy_stub(void *dst, const void *src, uint32_t size)
{
    if (call_count >= 2) fail("extra copy", call_count);
    calls[call_count][0] = (uint32_t)((uint8_t *)dst - state - 16);
    calls[call_count][1] = (uint32_t)((const uint8_t *)src - global_blob);
    calls[call_count++][2] = size;
    memmove(dst, src, size);
    return 0;
}
static void invoke(uint32_t fn, uint8_t *arg, uint32_t argc, uint32_t role)
{
    model_call(fn, arg, argc);
    if (returned[1] != 0x31415926 || returned[4] != 0x27182818 ||
        returned[5] != 0x16180339 || returned[6] != 0x14142135 || returned[7])
        fail("preserved register/ESP", role);
    if (role != 0x100 && (returned[2] != 0x12345678 || returned[3] != 0x98765432))
        fail("lifecycle ecx/edx", role);
    if (role == 0x18 && returned[0] != (uint32_t)(uintptr_t)arg)
        fail("attach eax", role);
    if (role != 0x18 && role != 0x100 && returned[0] != fn)
        fail("noop eax", role);
}
static int executable(HMODULE dll, uint32_t pointer)
{
    uint8_t *base = (uint8_t *)dll;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_SECTION_HEADER *section = IMAGE_FIRST_SECTION(nt);
    uint32_t i, rva = pointer - (uint32_t)(uintptr_t)base;
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, section++)
        if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) && rva >= section->VirtualAddress &&
            rva < section->VirtualAddress + section->Misc.VirtualSize) return 1;
    return 0;
}

static uint32_t preferred_base(HMODULE dll)
{
    uint8_t *base = (uint8_t *)dll;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    return nt->OptionalHeader.ImageBase;
}

static void resources(HMODULE retail, HMODULE ours)
{
    uint32_t i;
    for (i = 0; i < model->resource_count; i++) {
        const struct model_resource *r = model->resources + i;
        HRSRC a = FindResourceExA(retail, MAKEINTRESOURCEA(r->type), MAKEINTRESOURCEA(r->id), r->language);
        HRSRC b = FindResourceExA(ours, MAKEINTRESOURCEA(r->type), MAKEINTRESOURCEA(r->id), r->language);
        if (!a || !b || SizeofResource(retail, a) != r->size || SizeofResource(ours, b) != r->size)
            fail("resource size/type/language", i);
        compare(LockResource(LoadResource(retail, a)), LockResource(LoadResource(ours, b)),
                r->size, "resource bytes");
    }
}

static void normalize(uint8_t *buffer, const uint8_t *retail, const uint8_t *ours,
                      const uint32_t *code_targets)
{
    uint32_t i;
    for (i = 0; i < model->reloc_count; i++) {
        const struct model_reloc *r = model->relocs + i;
        uint32_t actual = read32(buffer + r->site - model->data_rva);
        uint32_t wanted = r->target >= model->data_rva
            ? (uint32_t)(uintptr_t)(ours + r->target) : code_targets[i];
        if (actual != wanted) fail("relocated pointer", r->site);
        write32(buffer + r->site - model->data_rva, (uint32_t)(uintptr_t)(retail + r->target));
    }
}

int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 0) : 70000;
    const char *mode = argc > 2 ? argv[2] : "retail-first";
    uint32_t k;
    if (!count) return 2;
    SetUnhandledExceptionFilter(fault);
    write32(host_blob + 0x21c, (uint32_t)(uintptr_t)copy_stub);
    for (k = 0; k < sizeof models / sizeof models[0]; k++) {
        HMODULE retail, rebuilt;
        uint8_t *base, *ours, *imports, *link, *header;
        uint8_t *pristine[2], *retail_data, *normalized;
        uint32_t code_targets[16], i, j, end, state_size, update[2];
        uint8_t craft[32], craft_expected[32];
        void *reservation = NULL;
        model = models + k;
        vector = 0;
        if (!strcmp(mode, "relocate")) {
            reservation = VirtualAlloc((void *)0x22100000, 0x60000, MEM_RESERVE, PAGE_NOACCESS);
            if (!reservation) fail("reserve preferred bases", GetLastError());
        }
        if (!strcmp(mode, "ours-first")) {
            rebuilt = LoadLibraryA(k == 0 && argc > 3 ? argv[3] : model->ours);
            retail = LoadLibraryA(model->retail);
        } else {
            retail = LoadLibraryA(model->retail);
            rebuilt = LoadLibraryA(k == 0 && argc > 3 ? argv[3] : model->ours);
        }
        if (!retail || !rebuilt) fail("LoadLibrary", GetLastError());
        if (!strcmp(mode, "relocate")) {
            if ((uintptr_t)retail == preferred_base(retail) || (uintptr_t)rebuilt == preferred_base(rebuilt))
                fail("forced relocation did not move both DLLs", 0);
        } else {
            HMODULE first = !strcmp(mode, "ours-first") ? rebuilt : retail;
            HMODULE second = first == rebuilt ? retail : rebuilt;
            if ((uintptr_t)first != preferred_base(first) || (uintptr_t)second == preferred_base(second))
                fail("load-order addresses did not exercise preferred/relocated", 0);
        }
        base = (uint8_t *)retail;
        imports = (uint8_t *)GetProcAddress(rebuilt, "ImportTable");
        link = (uint8_t *)GetProcAddress(rebuilt, "Linkage");
        if (!imports || !link) fail("exports", 0);
        fill(craft, sizeof craft);
        memcpy(craft_expected, craft, sizeof craft);
        invoke(read32(link + 0x18), craft, 1, 0x18);
        header = (uint8_t *)(uintptr_t)read32(craft);
        ours = header - model->header_rva;
        write32(craft_expected, (uint32_t)(uintptr_t)header);
        compare(craft, craft_expected, sizeof craft, "attach writes");
        if (imports != ours + model->imports_rva || link != ours + model->linkage_rva)
            fail("export/image layout", 0);
        if (read32(link) != 31 || model->reloc_count > 16) fail("module identity", 0);
        for (i = 0; i < model->reloc_count; i++) {
            const struct model_reloc *r = model->relocs + i;
            code_targets[i] = read32(ours + r->site);
            if (r->target < model->data_rva && !executable(rebuilt, code_targets[i]))
                fail("callback outside rebuilt code", r->site);
            for (j = 0; j < i; j++)
                if (model->relocs[j].target == r->target && code_targets[j] != code_targets[i])
                    fail("shared callback pointer", r->site);
        }
        resources(retail, rebuilt);
        state_size = read32(base + model->header_rva + 0x18);
        end = model->active_end - model->data_rva;
        pristine[0] = malloc(model->data_size); pristine[1] = malloc(model->data_size);
        retail_data = malloc(model->data_size); normalized = malloc(model->data_size);
        if (!pristine[0] || !pristine[1] || !retail_data || !normalized) fail("allocation", 0);
        for (i = 0; i < 2; i++) {
            uint8_t *image = i ? ours : base;
            write32(image + model->imports_rva + 4, (uint32_t)(uintptr_t)host_blob);
            write32(image + model->imports_rva + 12, (uint32_t)(uintptr_t)global_blob);
            memcpy(pristine[i], image + model->data_rva, model->data_size);
            update[i] = read32(image + model->header_rva + 0x10);
        }
        memcpy(normalized, pristine[1], model->data_size);
        normalize(normalized, base, ours, code_targets);
        compare(pristine[0], normalized, end, "initial model data/header/Linkage");
        for (vector = 0; vector < count; vector++) {
            static const uint32_t wanted_calls[2][3] = {{4, 0x560, 0x24}, {0x28, 0x5b0, 0x18}};
            uint32_t value = vector < 65536 ? vector : rng();
            fill(global_blob, sizeof global_blob); fill(initial, sizeof initial);
            if ((vector & 3) && vector) memcpy(initial, expected, sizeof initial);
            global_blob[0xb6a] = vector & 1 ? 0 : 1;
            global_blob[0x280] = (uint8_t)(vector >> 1);
            global_blob[0x28c] = (uint8_t)(vector >> 2);
            global_blob[0x282] = (uint8_t)vector;
            for (i = 0; i < 3; i++) {
                uint16_t word = (uint16_t)value;
                write32(global_blob + 0xbec + 4 * i, value);
                memcpy(global_blob + 0x11c0 + 2 * i, &word, 2);
            }
            memcpy(global_before, global_blob, sizeof global_blob);
            for (i = 0; i < 2; i++) {
                uint8_t *image = i ? ours : base;
                uint8_t *table = image + model->linkage_rva;
                memcpy(image + model->data_rva, pristine[i], model->data_size);
                memcpy(state, initial, sizeof state);
                fill(craft, sizeof craft);
                memcpy(craft_expected, craft, sizeof craft);
                invoke(read32(table + 4), NULL, 0, 4);
                invoke(read32(table + 8), NULL, 0, 8);
                invoke(read32(table + 0x18), craft, 1, 0x18);
                write32(craft_expected, (uint32_t)(uintptr_t)(image + model->header_rva));
                compare(craft, craft_expected, sizeof craft, "attach writes");
                invoke(read32(table + 0x1c), state + 16, 1, 0x1c);
                compare(state, initial, sizeof state, "state init");
                call_count = 0;
                invoke(update[i], state + 16, 1, 0x100);
                if (call_count != 2) fail("copy count", call_count);
                compare(calls, wanted_calls, sizeof calls, "copy calls");
                invoke(read32(table + 0x20), NULL, 0, 0x20);
                compare(global_blob, global_before, sizeof global_blob, "GLOBAL");
                compare(state, initial, 16, "leading canary");
                compare(state + 16 + state_size, initial + 16 + state_size,
                        sizeof state - 16 - state_size, "trailing canary");
                if (!i) {
                    memcpy(expected, state, sizeof state);
                    memcpy(retail_data, image + model->data_rva, model->data_size);
                } else {
                    if (model->kind == 7) {
                        uint32_t pointer = (uint32_t)(uintptr_t)(ours + 0x1037);
                        compare(ours + 0x1037, base + 0x1037, 14, "Sopwith literal");
                        write32(expected + 16 + 0x74, pointer);
                    }
                    compare(expected, state, sizeof state, "state");
                    memcpy(normalized, ours + model->data_rva, model->data_size);
                    normalize(normalized, base, ours, code_targets);
                    compare(retail_data, normalized, end, "model data/header/Linkage");
                }
                /* CRT bookkeeping is private to each implementation, untouched
                 * by any MDL service. Check it against that side's snapshot. */
                compare(image + model->active_end, pristine[i] + end,
                        model->data_size - end, "CRT tail changed");
            }
        }
        printf("%s: %u lifecycle/update scenarios, 0 differences (%s)\n",
               strrchr(model->retail, '/') + 1, count, mode);
        free(pristine[0]); free(pristine[1]); free(retail_data); free(normalized);
        FreeLibrary(retail); FreeLibrary(rebuilt);
        if (reservation) VirtualFree(reservation, 0, MEM_RELEASE);
    }
    return 0;
}
