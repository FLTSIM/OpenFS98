/* The generated host runtime uses the integer primitives already shared by
 * the SIM1 and model transliterations. Native calls use the real thread stack
 * and transfer all registers, including MFC's this pointer in ECX.
 */
#ifndef FS98_HOST_RUNTIME_H
#define FS98_HOST_RUNTIME_H
#include "../mdl/mdl_crash_lift.h"
#include <string.h>
#include "fs98/host_runtime.h"

struct host_runtime_frame {
    struct mdl_frame cpu;
    uint32_t real_sp;
};
typedef char host_real_sp_offset[(offsetof(struct host_runtime_frame, real_sp) == 68) ? 1 : -1];
extern DWORD host_runtime_tls;
uint32_t host_runtime_stack(void);
void host_runtime_native(struct mdl_frame *frame, uint32_t target);
uint32_t host_runtime_address(struct mdl_frame *frame, uint32_t rva);
uint32_t *host_runtime_enter(uint32_t entry, uint32_t *saved);
int host_runtime_init(void);
void host_runtime_fail(uint32_t address);
void host_runtime_import_error(const char *library, const char *name);
static uint32_t host_fs_address(uint32_t offset)
{
    uint32_t teb;
    __asm__ volatile("mov %0, fs:0x18" : "=r"(teb));
    return teb + offset;
}
static uint32_t host_runtime_read(struct mdl_frame *frame, uint32_t address, unsigned width)
{
    static struct mdl_frame empty;
    return mdl_read(frame ? frame : &empty,address,width);
}
static void host_runtime_write(struct mdl_frame *frame, uint32_t address, uint32_t value, unsigned width)
{
    static struct mdl_frame empty;
    mdl_write(frame ? frame : &empty,address,value,width);
}
#define mdl_read host_runtime_read
#define mdl_write host_runtime_write
#define mdl_address host_runtime_address
#endif
