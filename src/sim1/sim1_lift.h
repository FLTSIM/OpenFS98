/* Explicit-C SIM1 arithmetic shares the already verified integer primitives
 * used by crash models. Its native bridge uses an actual stack so imported
 * stdcall services perform their own argument cleanup, including odd ABIs.
 */
#ifndef FS98_SIM1_LIFT_H
#define FS98_SIM1_LIFT_H
#include "../mdl/mdl_crash_lift.h"

struct sim1_frame {
    struct mdl_frame cpu;
    uint32_t real_sp;
};
typedef char sim1_real_sp_offset[(offsetof(struct sim1_frame, real_sp) == 68) ? 1 : -1];
extern DWORD sim1_lift_tls;
uint32_t sim1_lift_stack(void);
void sim1_lift_native(struct mdl_frame *f, uint32_t target);
void sim1_lift_enter(uint32_t entry, uint32_t *saved);
uint32_t sim1_lift_address(struct mdl_frame *f, uint32_t rva);
#define mdl_address sim1_lift_address
#endif
