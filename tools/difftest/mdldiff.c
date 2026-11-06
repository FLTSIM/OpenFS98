/* Normal MDL callback slice: all eight CD models against portable C.
 * Compares entire .data, whole state with canaries, GLOBAL and copy calls.
 * The first 65536 vectors exhaust each 16-bit control/gear helper input;
 * the remaining vectors use full-width random inputs. No graphics stub is
 * needed: these callbacks produce state and mutate G3D program operands.
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mdl_normal.h"

struct fixture {
    const char *path;
    uint32_t data_rva, data_size, imports_rva, update_rva, state_size;
};
static const struct fixture fixtures[] = {
#include "fixtures.inc"
};

static uint8_t global_blob[0x2000], global_before[0x2000];
static uint8_t host_blob[0x320];
static uint8_t state[0x200], initial[0x200], expected[0x200];
static uint32_t random_state = 0x4d444c31;
static uint32_t calls[2][3], call_count;
static uint32_t vector;
static const char *model;
static enum fs98_mdl_kind current_kind;
static uint8_t *current_image;
static volatile uint32_t saved_registers[5] __attribute__((used));
static volatile uint32_t entry_stack __attribute__((used));

/* One-argument boundary used by WINDOW, including preserved registers and ESP. */
void invoke(void (FS98_STDCALL *fn)(uint8_t *), uint8_t *state);
__asm__(".text\n.globl _invoke\n_invoke:\n\t"
        "pushl %ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
        "movl 20(%esp), %eax\n\tmovl 24(%esp), %edx\n\t"
        "movl %esp, _entry_stack\n\tpushl %edx\n\t"
        "movl $0x31415926, %ebx\n\tmovl $0x27182818, %esi\n\t"
        "movl $0x16180339, %edi\n\tmovl $0x14142135, %ebp\n\t"
        "call *%eax\n\t"
        "movl %ebx, _saved_registers\n\tmovl %esi, _saved_registers+4\n\t"
        "movl %edi, _saved_registers+8\n\tmovl %ebp, _saved_registers+12\n\t"
        "movl %esp, %eax\n\tsubl _entry_stack, %eax\n\t"
        "movl %eax, _saved_registers+16\n\t"
        "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tpopl %ebp\n\tret\n");

static uint32_t rng(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static void fill(void *buffer, uint32_t size)
{
    uint8_t *p = buffer;
    while (size--) *p++ = (uint8_t)rng();
}

static void put32(void *buffer, uint32_t off, uint32_t value)
{ memcpy((uint8_t *)buffer + off, &value, 4); }

static void fail(const char *what, uint32_t offset)
{
    fprintf(stderr, "%s vector %u: %s +0x%x\n", model, vector, what, offset);
    ExitProcess(1);
}

static LONG WINAPI fault(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr, "%s vector %u: exception %08lx at %p\n", model, vector,
            exception->ExceptionRecord->ExceptionCode,
            exception->ExceptionRecord->ExceptionAddress);
    ExitProcess(2);
    return EXCEPTION_EXECUTE_HANDLER;
}

static int32_t FS98_STDCALL copy_stub(void *dst, const void *src, uint32_t size)
{
    if (call_count >= 2) fail("extra copy call", call_count);
    calls[call_count][0] = (uint32_t)((uint8_t *)dst - state - 16);
    calls[call_count][1] = (uint32_t)((const uint8_t *)src - global_blob);
    calls[call_count++][2] = size;
    memmove(dst, src, size);
    return 0;
}

static void check_calls(void)
{
    static const uint32_t wanted[2][3] = {{4, 0x560, 0x24}, {0x28, 0x5b0, 0x18}};
    if (call_count != 2 || memcmp(calls, wanted, sizeof calls)) fail("copy calls", 0);
}

static void check_registers(void)
{
    static const uint32_t wanted[5] = {0x31415926, 0x27182818, 0x16180339, 0x14142135, 0};
    uint32_t i;
    for (i = 0; i < 5; i++)
        if (saved_registers[i] != wanted[i]) fail("preserved register/ESP", i);
}

static void FS98_STDCALL update_c(uint8_t *output)
{
    fs98_mdl_normal_update(current_kind, output, global_blob, current_image, copy_stub);
}

static void compare(const void *a, const void *b, uint32_t size, const char *what)
{
    const uint8_t *p = a, *q = b;
    uint32_t i;
    if (!memcmp(p, q, size)) return;
    for (i = 0; i < size; i++) if (p[i] != q[i]) {
        fprintf(stderr, "  retail=%02x ours=%02x\n", p[i], q[i]);
        fail(what, i);
    }
}

int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 0) : 70000;
    uint32_t k, i, value, image_size;
    int relocated = argc > 2 && !strcmp(argv[2], "relocate");
    SetUnhandledExceptionFilter(fault);
    put32(host_blob, 0x21c, (uint32_t)(uintptr_t)copy_stub);
    for (k = 0; k < sizeof fixtures / sizeof fixtures[0]; k++) {
        const struct fixture *f = &fixtures[k];
        HMODULE dll;
        uint8_t *base, *clone, *pristine, *retail_data;
        uint8_t *reservation = NULL;
        IMAGE_NT_HEADERS *nt;
        void (FS98_STDCALL *update)(uint8_t *);
        model = f->path;
        if (relocated) {
            /* Occupy every normal-model preferred address, including overlap. */
            reservation = VirtualAlloc((void *)0x22100000, 0x60000,
                                      MEM_RESERVE, PAGE_NOACCESS);
            if (!reservation) fail("reserve preferred bases", GetLastError());
        }
        dll = LoadLibraryA(f->path);
        if (!dll) fail("LoadLibrary", GetLastError());
        base = (uint8_t *)dll;
        nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
        if (relocated && (uintptr_t)base == nt->OptionalHeader.ImageBase)
            fail("forced relocation did not move module", 0);
        image_size = nt->OptionalHeader.SizeOfImage;
        clone = calloc(1, image_size);
        pristine = malloc(f->data_size);
        retail_data = malloc(f->data_size);
        if (!clone || !pristine || !retail_data) fail("allocation", 0);
        /* Keep only data and the Sopwith literal; C never executes this image. */
        memcpy(clone + f->data_rva, base + f->data_rva, f->data_size);
        memcpy(clone + 0x1037, base + 0x1037, 14);
        put32(base, f->imports_rva + 4, (uint32_t)(uintptr_t)host_blob);
        put32(base, f->imports_rva + 12, (uint32_t)(uintptr_t)global_blob);
        memcpy(pristine, base + f->data_rva, f->data_size);
        update = (void (FS98_STDCALL *)(uint8_t *))(base + f->update_rva);
        for (vector = 0; vector < count; vector++) {
            fill(global_blob, sizeof global_blob);
            fill(initial, sizeof initial);
            /* Alternate persistent state with fresh random state. */
            if ((vector & 3) && vector) memcpy(initial, expected, sizeof initial);
            global_blob[0xb6a] = (vector & 1) ? 0 : (uint8_t)(1 + vector % 255);
            global_blob[0x280] = (uint8_t)(vector >> 1);
            global_blob[0x28c] = (uint8_t)(vector >> 2);
            global_blob[0x282] = (uint8_t)vector;
            value = vector < 65536 ? vector : rng();
            for (i = 0; i < 3; i++) {
                uint16_t word = (uint16_t)value;
                put32(global_blob, 0xbec + i * 4, value);
                memcpy(global_blob + 0x11c0 + i * 2, &word, 2);
            }
            memcpy(global_before, global_blob, sizeof global_blob);
            memcpy(state, initial, sizeof state);
            memcpy(base + f->data_rva, pristine, f->data_size);
            call_count = 0;
            invoke(update, state + 16);
            check_registers();
            check_calls();
            compare(global_blob, global_before, sizeof global_blob, "GLOBAL retail");
            memcpy(expected, state, sizeof state);
            memcpy(retail_data, base + f->data_rva, f->data_size);
            memcpy(state, initial, sizeof state);
            memcpy(clone + f->data_rva, pristine, f->data_size);
            call_count = 0;
            current_kind = (enum fs98_mdl_kind)k;
            current_image = clone;
            invoke(update_c, state + 16);
            check_registers();
            check_calls();
            compare(global_blob, global_before, sizeof global_blob, "GLOBAL C");
            /* Sopwith stores an image-relative string pointer into state. */
            if (k == FS98_MDL_SOPWITH)
                put32(expected, 16 + 0x74, (uint32_t)(uintptr_t)(clone + 0x1037));
            compare(state, expected, sizeof state, "state/canaries");
            compare(clone + f->data_rva, retail_data, f->data_size, ".data");
            compare(state, initial, 16, "leading canary");
            compare(state + 16 + f->state_size, initial + 16 + f->state_size,
                    sizeof state - 16 - f->state_size, "trailing canary");
        }
        printf("%s: %u updates, 0 differences (%s)\n", strrchr(f->path, '/') + 1,
               count, relocated ? "relocated" : "preferred base");
        free(clone); free(pristine); free(retail_data);
        FreeLibrary(dll);
        if (reservation) VirtualFree(reservation, 0, MEM_RELEASE);
    }
    return 0;
}
