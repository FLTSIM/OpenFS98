/* 0x22002ac0: body-to-world velocity transform, stdcall(three raw doubles,
 * three output pointers). The stack's argument copies are scratch doubles:
 * their explicit stores round at 64 bits between longer x87 calculations.
 * Keep both branches and their stack order until native FP work can replace
 * this boundary with independently verified arithmetic.
 */
.intel_syntax noprefix
.text
.globl _sim1_velocity
_sim1_velocity:
    cmp DWORD PTR [_sim1_image + 0x7c0], 1
    jne .Lvelocity_other
    fld QWORD PTR [esp + 4]
    fld QWORD PTR [esp + 12]
    fmul QWORD PTR [_sim1_image + 0x1570]
    fld QWORD PTR [esp + 4]
    fxch st(2)
    fmul QWORD PTR [_sim1_image + 0x1578]
    fld QWORD PTR [esp + 12]
    fxch st(1)
    faddp st(2), st
    fld QWORD PTR [esp + 20]
    fxch st(3)
    fmul QWORD PTR [_sim1_image + 0x1570]
    fxch st(2)
    fst QWORD PTR [esp + 12]
    fxch st(1)
    fmul QWORD PTR [_sim1_image + 0x1578]
    fxch st(1)
    fmul QWORD PTR [_sim1_image + 0x1568]
    fxch st(3)
    fmul QWORD PTR [_sim1_image + 0x1560]
    fxch st(1)
    fsubp st(2), st
    mov eax, DWORD PTR [esp + 28]
    mov ecx, DWORD PTR [esp + 32]
    mov edx, DWORD PTR [esp + 36]
    faddp st(2), st
    fst QWORD PTR [esp + 4]
    fld st(1)
    fxch st(1)
    fmul QWORD PTR [_sim1_image + 0x1580]
    fld QWORD PTR [esp + 12]
    fxch st(2)
    fmul QWORD PTR [_sim1_image + 0x1588]
    fld QWORD PTR [esp + 20]
    fxch st(1)
    faddp st(2), st
    fld QWORD PTR [esp + 4]
    fxch st(2)
    fstp QWORD PTR [eax]
    fxch st(2)
    fmul QWORD PTR [_sim1_image + 0x1560]
    fxch st(2)
    fmul QWORD PTR [_sim1_image + 0x1568]
    fsubp st(2), st
    fxch st(1)
    fstp QWORD PTR [ecx]
    fxch st(1)
    fmul QWORD PTR [_sim1_image + 0x1580]
    fxch st(1)
    fmul QWORD PTR [_sim1_image + 0x1588]
    fsubp st(1), st
    fstp QWORD PTR [edx]
    ret 36

.Lvelocity_other:
    fld QWORD PTR [esp + 12]
    fld QWORD PTR [esp + 4]
    fchs
    fxch st(1)
    fst QWORD PTR [esp + 4]
    fxch st(1)
    fst QWORD PTR [esp + 12]
    fxch st(1)
    fmul QWORD PTR [_sim1_image + 0x1570]
    fld QWORD PTR [esp + 12]
    fxch st(2)
    fmul QWORD PTR [_sim1_image + 0x1578]
    fld QWORD PTR [esp + 4]
    fxch st(1)
    fsubp st(2), st
    fld QWORD PTR [esp + 20]
    fxch st(3)
    fmul QWORD PTR [_sim1_image + 0x1570]
    fxch st(2)
    fst QWORD PTR [esp + 12]
    fxch st(1)
    fmul QWORD PTR [_sim1_image + 0x1578]
    fld QWORD PTR [esp + 12]
    fxch st(2)
    fmul QWORD PTR [_sim1_image + 0x1568]
    fld QWORD PTR [esp + 20]
    fxch st(5)
    fmul QWORD PTR [_sim1_image + 0x1560]
    fxch st(3)
    fmul QWORD PTR [_sim1_image + 0x1560]
    fxch st(5)
    fmul QWORD PTR [_sim1_image + 0x1568]
    fxch st(2)
    faddp st(4), st
    fxch st(2)
    faddp st(2), st
    mov eax, DWORD PTR [esp + 28]
    mov ecx, DWORD PTR [esp + 32]
    fsubp st(3), st
    fxch st(1)
    fst QWORD PTR [esp + 4]
    fxch st(1)
    fst QWORD PTR [esp + 12]
    fxch st(2)
    fstp QWORD PTR [eax]
    mov edx, DWORD PTR [esp + 36]
    fmul QWORD PTR [_sim1_image + 0x1580]
    fld QWORD PTR [esp + 12]
    fxch st(2)
    fmul QWORD PTR [_sim1_image + 0x1588]
    fld QWORD PTR [esp + 4]
    fxch st(1)
    fsubp st(2), st
    fxch st(1)
    fstp QWORD PTR [ecx]
    fxch st(1)
    fmul QWORD PTR [_sim1_image + 0x1580]
    fxch st(1)
    fmul QWORD PTR [_sim1_image + 0x1588]
    faddp st(1), st
    fstp QWORD PTR [edx]
    ret 36
.att_syntax prefix
