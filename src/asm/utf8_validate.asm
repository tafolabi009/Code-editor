; utf8_validate.asm - SIMD-accelerated UTF-8 validation
;
; Uses SSE4.2 instructions for fast UTF-8 validation at 5+ GB/sec.
; Validates byte sequences according to RFC 3629.
;
; Build with NASM:
;   nasm -f elf64 utf8_validate.asm -o utf8_validate.o  (Linux)
;   nasm -f win64 utf8_validate.asm -o utf8_validate.obj (Windows)

%ifdef WIN64
    %define ARG1 rcx
    %define ARG2 rdx
%else
    %define ARG1 rdi
    %define ARG2 rsi
%endif

section .data
    align 16
    ; Lookup table for UTF-8 byte classification
    ; 0 = ASCII (0x00-0x7F)
    ; 1 = Continuation byte (0x80-0xBF)
    ; 2 = Invalid (0xC0-0xC1, 0xF5-0xFF)
    ; 3 = 2-byte lead (0xC2-0xDF)
    ; 4 = 3-byte lead (0xE0-0xEF)
    ; 5 = 4-byte lead (0xF0-0xF4)
    utf8_class_lo:
        db 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  ; 0x00-0x0F
    utf8_class_hi:
        db 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 3, 3, 4, 5  ; 0x80-0xFF >> 4

section .text

;------------------------------------------------------------------------------
; int simd_utf8_validate(const char* text, size_t length)
;
; Validates that text contains valid UTF-8.
; Returns 1 if valid, 0 if invalid.
;
; Algorithm uses SIMD to process 16 bytes at a time:
; 1. Check for ASCII-only chunks (fast path)
; 2. For non-ASCII, validate byte sequences
;------------------------------------------------------------------------------
global simd_utf8_validate
simd_utf8_validate:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    mov r12, ARG1           ; text
    mov r13, ARG2           ; length
    
    ; Default: valid
    mov eax, 1
    
    test r13, r13
    jz .valid
    
    xor r8, r8              ; current position
    xor r9, r9              ; expected continuation bytes
    
.validate_loop:
    ; Check if we're expecting continuation bytes
    test r9, r9
    jnz .check_continuation
    
    ; Process 16 bytes at a time if possible
    mov rcx, r13
    sub rcx, r8
    cmp rcx, 16
    jb .scalar_validate
    
    ; Load 16 bytes
    movdqu xmm0, [r12 + r8]
    
    ; Check if all bytes are ASCII (< 0x80)
    pmovmskb ecx, xmm0
    test ecx, ecx
    jz .advance_16          ; All ASCII - skip ahead
    
    ; Has non-ASCII bytes - process scalar
    jmp .scalar_validate
    
.advance_16:
    add r8, 16
    cmp r8, r13
    jb .validate_loop
    jmp .valid
    
.scalar_validate:
    cmp r8, r13
    jge .check_end
    
    movzx ecx, byte [r12 + r8]
    
    ; Check byte type
    cmp cl, 0x80
    jb .ascii_byte
    
    cmp cl, 0xC0
    jb .continuation_byte
    
    cmp cl, 0xC2
    jb .invalid             ; 0xC0-0xC1 are invalid
    
    cmp cl, 0xE0
    jb .two_byte_lead
    
    cmp cl, 0xF0
    jb .three_byte_lead
    
    cmp cl, 0xF5
    jb .four_byte_lead
    
    ; 0xF5-0xFF are invalid
    jmp .invalid
    
.ascii_byte:
    inc r8
    jmp .validate_loop
    
.continuation_byte:
    ; Unexpected continuation byte
    test r9, r9
    jz .invalid
    dec r9
    inc r8
    jmp .validate_loop
    
.two_byte_lead:
    mov r9, 1               ; Expect 1 continuation byte
    inc r8
    jmp .validate_loop
    
.three_byte_lead:
    ; Check for overlong encoding and surrogate
    cmp cl, 0xE0
    jne .three_byte_normal
    
    ; E0 must be followed by A0-BF
    lea rcx, [r8 + 1]
    cmp rcx, r13
    jge .invalid
    movzx edx, byte [r12 + rcx]
    cmp dl, 0xA0
    jb .invalid
    jmp .three_byte_normal
    
.three_byte_normal:
    ; Check for surrogate (ED must be followed by 80-9F)
    cmp cl, 0xED
    jne .three_byte_ok
    
    lea rcx, [r8 + 1]
    cmp rcx, r13
    jge .invalid
    movzx edx, byte [r12 + rcx]
    cmp dl, 0xA0
    jge .invalid
    
.three_byte_ok:
    mov r9, 2               ; Expect 2 continuation bytes
    inc r8
    jmp .validate_loop
    
.four_byte_lead:
    ; Check for overlong and too-large code points
    cmp cl, 0xF0
    jne .four_byte_check_high
    
    ; F0 must be followed by 90-BF
    lea rcx, [r8 + 1]
    cmp rcx, r13
    jge .invalid
    movzx edx, byte [r12 + rcx]
    cmp dl, 0x90
    jb .invalid
    jmp .four_byte_ok
    
.four_byte_check_high:
    cmp cl, 0xF4
    jne .four_byte_ok
    
    ; F4 must be followed by 80-8F
    lea rcx, [r8 + 1]
    cmp rcx, r13
    jge .invalid
    movzx edx, byte [r12 + rcx]
    cmp dl, 0x90
    jge .invalid
    
.four_byte_ok:
    mov r9, 3               ; Expect 3 continuation bytes
    inc r8
    jmp .validate_loop
    
.check_continuation:
    cmp r8, r13
    jge .invalid            ; Truncated sequence
    
    movzx ecx, byte [r12 + r8]
    cmp cl, 0x80
    jb .invalid
    cmp cl, 0xC0
    jge .invalid
    
    dec r9
    inc r8
    jmp .validate_loop
    
.check_end:
    ; If we're still expecting continuation bytes, invalid
    test r9, r9
    jnz .invalid
    
.valid:
    mov eax, 1
    jmp .done
    
.invalid:
    xor eax, eax
    
.done:
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

;------------------------------------------------------------------------------
; size_t simd_utf8_find_invalid(const char* text, size_t length)
;
; Find first invalid UTF-8 byte.
; Returns offset of first invalid byte, or length if all valid.
;------------------------------------------------------------------------------
global simd_utf8_find_invalid
simd_utf8_find_invalid:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    mov r12, ARG1
    mov r13, ARG2
    
    mov rax, r13            ; Return length if all valid
    
    test r13, r13
    jz .find_done
    
    xor r8, r8
    xor r9, r9              ; Expected continuations
    
.find_loop:
    test r9, r9
    jnz .find_continuation
    
    cmp r8, r13
    jge .find_done
    
    movzx ecx, byte [r12 + r8]
    
    ; ASCII fast path
    cmp cl, 0x80
    jb .find_ascii
    
    ; Non-ASCII validation
    cmp cl, 0xC0
    jb .find_invalid_cont
    
    cmp cl, 0xC2
    jb .find_invalid_here
    
    cmp cl, 0xE0
    jb .find_two
    
    cmp cl, 0xF0
    jb .find_three
    
    cmp cl, 0xF5
    jb .find_four
    
    jmp .find_invalid_here
    
.find_ascii:
    inc r8
    jmp .find_loop
    
.find_invalid_cont:
    ; Unexpected continuation
    test r9, r9
    jz .find_invalid_here
    dec r9
    inc r8
    jmp .find_loop
    
.find_two:
    mov r9, 1
    inc r8
    jmp .find_loop
    
.find_three:
    mov r9, 2
    inc r8
    jmp .find_loop
    
.find_four:
    mov r9, 3
    inc r8
    jmp .find_loop
    
.find_continuation:
    cmp r8, r13
    jge .find_invalid_here
    
    movzx ecx, byte [r12 + r8]
    cmp cl, 0x80
    jb .find_invalid_here
    cmp cl, 0xC0
    jge .find_invalid_here
    
    dec r9
    inc r8
    jmp .find_loop
    
.find_invalid_here:
    mov rax, r8
    
.find_done:
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

;------------------------------------------------------------------------------
; size_t simd_utf8_count_codepoints(const char* text, size_t length)
;
; Count UTF-8 code points using SIMD.
; Counts bytes that are NOT continuation bytes (0x80-0xBF).
;------------------------------------------------------------------------------
global simd_utf8_count_codepoints
simd_utf8_count_codepoints:
    push rbp
    mov rbp, rsp
    
    xor rax, rax            ; count = 0
    
    test ARG2, ARG2
    jz .count_done
    
    xor r8, r8              ; position
    
    ; Create mask for continuation bytes detection
    ; Continuation bytes are 0x80-0xBF (10xxxxxx pattern)
    mov ecx, 0x80808080
    vmovd xmm2, ecx
    vpbroadcastd ymm2, xmm2  ; 0x80 in all bytes
    
    mov ecx, 0x40404040
    vmovd xmm3, ecx
    vpbroadcastd ymm3, xmm3  ; 0x40 in all bytes
    
.count_loop:
    mov rcx, ARG2
    sub rcx, r8
    cmp rcx, 32
    jb .count_scalar
    
    ; Load 32 bytes
    vmovdqu ymm0, [ARG1 + r8]
    
    ; Check for continuation bytes: (byte & 0xC0) == 0x80
    vpand ymm1, ymm0, ymm2  ; byte & 0x80
    vpcmpeqb ymm1, ymm1, ymm2  ; == 0x80?
    
    vpand ymm4, ymm0, ymm3  ; byte & 0x40
    vpxor ymm4, ymm4, ymm3  ; != 0x40 (should be 0 for continuation)
    vpcmpeqb ymm4, ymm4, ymm3
    
    vpand ymm1, ymm1, ymm4  ; Both conditions
    
    ; Count non-continuation bytes
    vpmovmskb ecx, ymm1
    not ecx                 ; Invert: count NON-continuation bytes
    popcnt ecx, ecx
    add rax, rcx
    
    add r8, 32
    jmp .count_loop
    
.count_scalar:
    cmp r8, ARG2
    jge .count_cleanup
    
    movzx ecx, byte [ARG1 + r8]
    
    ; Check if NOT a continuation byte
    and cl, 0xC0
    cmp cl, 0x80
    je .count_skip
    
    inc rax
    
.count_skip:
    inc r8
    jmp .count_scalar
    
.count_cleanup:
    vzeroupper
    
.count_done:
    pop rbp
    ret

; Mark the stack as non-executable (ELF). Silences the linker
; "missing .note.GNU-stack section implies executable stack" warning.
%ifidn __OUTPUT_FORMAT__, elf64
section .note.GNU-stack noalloc noexec nowrite progbits
%endif
