/* Test-only adapter for the native boundary. The harness supplies callees
 * with explicit machine-register/flags results and independent cleanup sizes.
 */
#include "sim1_lift.h"
#include <string.h>

void sim1_native_probe(uint32_t *registers, uint32_t target, uint32_t argc, const uint32_t *args)
{
    struct sim1_frame context;
    struct mdl_frame *f = &context.cpu;
    uint32_t *stack_top = (uint32_t *)(uintptr_t)sim1_lift_stack();
    uint32_t initial_sp, i;
    void *previous;
    memcpy(f, registers, 36);
    f->stack_low = f->stack_high = 0;
    f->esp = initial_sp = (uintptr_t)stack_top;
    for (i = 0; i < argc; i++) stack_top[i] = args[i];
    previous = TlsGetValue(sim1_lift_tls);
    TlsSetValue(sim1_lift_tls, f);
    sim1_lift_native(f, target);
    f->esp -= initial_sp;
    memcpy(registers, f, 36);
    TlsSetValue(sim1_lift_tls, previous);
}
