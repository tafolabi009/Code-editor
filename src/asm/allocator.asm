; allocator.asm - High-performance memory allocator
;
; Provides SIMD-accelerated memory operations and arena allocation
; for zero-fragmentation memory management.
;
; Build with NASM:
;   nasm -f elf64 allocator.asm -o allocator.o  (Linux)
;   nasm -f win64 allocator.asm -o allocator.obj (Windows)

%ifdef WIN64
    %define ARG1 rcx
    %define ARG2 rdx
    %define ARG2b dl         ; low byte of the 2nd argument
    %define ARG3 r8
%else
    %define ARG1 rdi
    %define ARG2 rsi
    %define ARG2b sil        ; low byte of the 2nd argument
    %define ARG3 rdx
%endif

section .data
    align 64
    ; Free list head for different block sizes
    free_list_4k:   dq 0
    free_list_64k:  dq 0
    free_list_1m:   dq 0
    
    ; Statistics
    total_allocated: dq 0
    total_freed:     dq 0

section .text

;------------------------------------------------------------------------------
; void simd_memcpy(void* dst, const void* src, size_t size)
;
; High-performance memory copy using AVX2.
; Handles alignment and uses non-temporal stores for large copies.
;------------------------------------------------------------------------------
global simd_memcpy
simd_memcpy:
    push rbp
    mov rbp, rsp
    
    ; Check for trivial cases
    test ARG3, ARG3
    jz .memcpy_done
    
    cmp ARG3, 32
    jb .memcpy_small
    
    cmp ARG3, 256
    jb .memcpy_medium
    
    ; Large copy - use non-temporal stores
    jmp .memcpy_large
    
.memcpy_small:
    ; Copy byte by byte for < 32 bytes
    mov rcx, ARG3
    mov rdi, ARG1
    mov rsi, ARG2
    rep movsb
    jmp .memcpy_done
    
.memcpy_medium:
    ; Use AVX2 for 32-256 bytes
    xor r8, r8
    
.memcpy_medium_loop:
    mov rcx, ARG3
    sub rcx, r8
    cmp rcx, 32
    jb .memcpy_medium_tail
    
    vmovdqu ymm0, [ARG2 + r8]
    vmovdqu [ARG1 + r8], ymm0
    
    add r8, 32
    jmp .memcpy_medium_loop
    
.memcpy_medium_tail:
    test rcx, rcx
    jz .memcpy_cleanup
    
    ; Copy remaining bytes
    lea rdi, [ARG1 + r8]
    lea rsi, [ARG2 + r8]
    rep movsb
    jmp .memcpy_cleanup
    
.memcpy_large:
    ; Align destination to 32 bytes
    mov rax, ARG1
    and rax, 31
    jz .memcpy_large_aligned
    
    ; Copy unaligned prefix
    mov rcx, 32
    sub rcx, rax
    cmp rcx, ARG3
    cmova rcx, ARG3
    
    mov rdi, ARG1
    mov rsi, ARG2
    push rcx
    rep movsb
    pop rcx
    
    add ARG1, rcx
    add ARG2, rcx
    sub ARG3, rcx
    
.memcpy_large_aligned:
    ; Use non-temporal stores for cache efficiency
    xor r8, r8
    
.memcpy_large_loop:
    mov rcx, ARG3
    sub rcx, r8
    cmp rcx, 128
    jb .memcpy_large_tail
    
    ; Prefetch next cache lines
    prefetchnta [ARG2 + r8 + 256]
    
    ; Copy 128 bytes (4 x 32-byte registers)
    vmovdqu ymm0, [ARG2 + r8]
    vmovdqu ymm1, [ARG2 + r8 + 32]
    vmovdqu ymm2, [ARG2 + r8 + 64]
    vmovdqu ymm3, [ARG2 + r8 + 96]
    
    vmovntdq [ARG1 + r8], ymm0
    vmovntdq [ARG1 + r8 + 32], ymm1
    vmovntdq [ARG1 + r8 + 64], ymm2
    vmovntdq [ARG1 + r8 + 96], ymm3
    
    add r8, 128
    jmp .memcpy_large_loop
    
.memcpy_large_tail:
    ; Copy remaining with regular stores
    cmp rcx, 32
    jb .memcpy_large_byte_tail
    
    vmovdqu ymm0, [ARG2 + r8]
    vmovdqu [ARG1 + r8], ymm0
    add r8, 32
    sub rcx, 32
    jmp .memcpy_large_tail
    
.memcpy_large_byte_tail:
    test rcx, rcx
    jz .memcpy_large_fence
    
    lea rdi, [ARG1 + r8]
    lea rsi, [ARG2 + r8]
    rep movsb
    
.memcpy_large_fence:
    sfence                  ; Ensure non-temporal stores are visible
    
.memcpy_cleanup:
    vzeroupper
    
.memcpy_done:
    pop rbp
    ret

;------------------------------------------------------------------------------
; void simd_memset(void* dst, int value, size_t size)
;
; High-performance memory set using AVX2.
;------------------------------------------------------------------------------
global simd_memset
simd_memset:
    push rbp
    mov rbp, rsp
    
    test ARG3, ARG3
    jz .memset_done
    
    ; Broadcast value to all bytes of YMM register
    movzx eax, ARG2b
    vmovd xmm0, eax
    vpbroadcastb ymm0, xmm0
    
    cmp ARG3, 32
    jb .memset_small
    
    cmp ARG3, 256
    jb .memset_medium
    
    jmp .memset_large
    
.memset_small:
    mov rcx, ARG3
    mov rdi, ARG1
    mov al, ARG2b
    rep stosb
    jmp .memset_done
    
.memset_medium:
    xor r8, r8
    
.memset_medium_loop:
    mov rcx, ARG3
    sub rcx, r8
    cmp rcx, 32
    jb .memset_medium_tail
    
    vmovdqu [ARG1 + r8], ymm0
    add r8, 32
    jmp .memset_medium_loop
    
.memset_medium_tail:
    test rcx, rcx
    jz .memset_cleanup
    
    lea rdi, [ARG1 + r8]
    mov al, ARG2b
    rep stosb
    jmp .memset_cleanup
    
.memset_large:
    ; Align to 32 bytes
    mov rax, ARG1
    and rax, 31
    jz .memset_large_aligned
    
    mov rcx, 32
    sub rcx, rax
    mov rdi, ARG1
    mov al, ARG2b
    push rcx
    rep stosb
    pop rcx
    
    add ARG1, rcx
    sub ARG3, rcx
    
.memset_large_aligned:
    xor r8, r8
    
.memset_large_loop:
    mov rcx, ARG3
    sub rcx, r8
    cmp rcx, 128
    jb .memset_large_tail
    
    vmovntdq [ARG1 + r8], ymm0
    vmovntdq [ARG1 + r8 + 32], ymm0
    vmovntdq [ARG1 + r8 + 64], ymm0
    vmovntdq [ARG1 + r8 + 96], ymm0
    
    add r8, 128
    jmp .memset_large_loop
    
.memset_large_tail:
    cmp rcx, 32
    jb .memset_large_byte_tail
    
    vmovdqu [ARG1 + r8], ymm0
    add r8, 32
    sub rcx, 32
    jmp .memset_large_tail
    
.memset_large_byte_tail:
    test rcx, rcx
    jz .memset_large_fence
    
    lea rdi, [ARG1 + r8]
    mov al, ARG2b
    rep stosb
    
.memset_large_fence:
    sfence
    
.memset_cleanup:
    vzeroupper
    
.memset_done:
    pop rbp
    ret

;------------------------------------------------------------------------------
; int simd_memcmp(const void* a, const void* b, size_t size)
;
; High-performance memory compare using AVX2.
; Returns 0 if equal, non-zero if different.
;------------------------------------------------------------------------------
global simd_memcmp
simd_memcmp:
    push rbp
    mov rbp, rsp
    
    xor eax, eax
    
    test ARG3, ARG3
    jz .memcmp_done
    
    cmp ARG3, 32
    jb .memcmp_small
    
    xor r8, r8
    
.memcmp_loop:
    mov rcx, ARG3
    sub rcx, r8
    cmp rcx, 32
    jb .memcmp_tail
    
    vmovdqu ymm0, [ARG1 + r8]
    vmovdqu ymm1, [ARG2 + r8]
    vpcmpeqb ymm2, ymm0, ymm1
    vpmovmskb eax, ymm2
    
    cmp eax, 0xFFFFFFFF
    jne .memcmp_diff
    
    add r8, 32
    jmp .memcmp_loop
    
.memcmp_tail:
    test rcx, rcx
    jz .memcmp_equal
    
.memcmp_small:
    mov rdi, ARG1
    add rdi, r8
    mov rsi, ARG2
    add rsi, r8
    mov rcx, ARG3
    sub rcx, r8
    repe cmpsb
    jne .memcmp_diff_scalar
    
.memcmp_equal:
    xor eax, eax
    jmp .memcmp_cleanup
    
.memcmp_diff:
    ; Find differing byte
    not eax
    bsf eax, eax
    jmp .memcmp_cleanup
    
.memcmp_diff_scalar:
    mov eax, 1
    
.memcmp_cleanup:
    vzeroupper
    
.memcmp_done:
    pop rbp
    ret

;------------------------------------------------------------------------------
; void* asm_aligned_alloc(size_t size, size_t alignment)
;
; Allocate memory with specified alignment.
; Uses system allocator with alignment padding.
;------------------------------------------------------------------------------
global asm_aligned_alloc
asm_aligned_alloc:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    
    mov r12, ARG1           ; size
    mov rbx, ARG2           ; alignment
    
    ; Calculate total size needed
    ; total = size + alignment + sizeof(void*)
    lea rdi, [r12 + rbx + 8]
    
    ; Call malloc (or mmap for large allocations)
%ifdef WIN64
    sub rsp, 32
    mov rcx, rdi
    call [rel __imp_malloc]
    add rsp, 32
%else
    call malloc wrt ..plt
%endif
    
    test rax, rax
    jz .alloc_fail
    
    ; Calculate aligned address
    lea rcx, [rax + 8]      ; Leave room for original pointer
    add rcx, rbx
    dec rcx
    ; Align down to the alignment boundary (assumes power of 2). The mask
    ; ~(alignment - 1) must be built at runtime since alignment is in a
    ; register, not a constant.
    mov r8, rbx
    dec r8
    not r8                  ; r8 = ~(alignment - 1)
    and rcx, r8
    
    ; Store original pointer before aligned address
    mov [rcx - 8], rax
    
    mov rax, rcx
    
.alloc_fail:
    pop r12
    pop rbx
    pop rbp
    ret

;------------------------------------------------------------------------------
; void asm_aligned_free(void* ptr)
;
; Free aligned memory allocated with asm_aligned_alloc.
;------------------------------------------------------------------------------
global asm_aligned_free
asm_aligned_free:
    push rbp
    mov rbp, rsp
    
    test ARG1, ARG1
    jz .free_done
    
    ; Retrieve original pointer
    mov rdi, [ARG1 - 8]
    
%ifdef WIN64
    sub rsp, 32
    mov rcx, rdi
    call [rel __imp_free]
    add rsp, 32
%else
    call free wrt ..plt
%endif
    
.free_done:
    pop rbp
    ret

; External function declarations
%ifdef WIN64
    extern __imp_malloc
    extern __imp_free
%else
    extern malloc
    extern free
%endif

; Mark the stack as non-executable (ELF). Silences the linker
; "missing .note.GNU-stack section implies executable stack" warning.
%ifidn __OUTPUT_FORMAT__, elf64
section .note.GNU-stack noalloc noexec nowrite progbits
%endif
