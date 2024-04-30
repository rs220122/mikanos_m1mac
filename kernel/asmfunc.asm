; asmfunc.asm 
; 
; System V AMD64 Calling Convention 
; Registers: RDI, RSI, RCX, R8, R9

bits 64 
section .text
global IoOut32  ;   void IsOut32(uint16_t addr, uint32_t data);
IoOut32:
    mov dx, di      ; dx = addr(di)
    mov eax, esi    ; eax = data(esi)
    out dx, eax     ; output data(32bit) to output address(dx)
    ret 

global IoIn32   ; uint32_t IoIn32(uint16_t addr);
IoIn32:
    mov dx, di  ; dx = addr 
    in eax, dx 
    ret 
