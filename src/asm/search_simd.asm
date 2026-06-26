; search_simd.asm - SIMD-accelerated string search using AVX2/SSE4.2
;
; This module provides high-performance string search using Intel SIMD
; instructions. It processes 32 bytes at a time with AVX2 or 16 bytes
; with SSE4.2, achieving throughput of 10+ GB/sec on modern CPUs.
;
; Build with NASM:
;   nasm -f elf64 search_simd.asm -o search_simd.o  (Linux)
;   nasm -f win64 search_simd.asm -o search_simd.obj (Windows)

%ifdef WIN64
    %define ARG1 rcx
    %define ARG2 rdx
    %define ARG3 r8
    %define ARG3b r8b        ; low byte of the 3rd argument
    %define ARG4 r9
    %define ARG5 [rsp+40]
    %define ARG6 [rsp+48]
%else
    ; System V AMD64 ABI (Linux)
    %define ARG1 rdi
    %define ARG2 rsi
    %define ARG3 rdx
    %define ARG3b dl         ; low byte of the 3rd argument
    %define ARG4 rcx
    %define ARG5 r8
    %define ARG6 r9
%endif

section .text

;------------------------------------------------------------------------------
; size_t simd_search_avx2(const char* text, size_t textLen,
;                         const char* pattern, size_t patternLen,
;                         size_t* results, size_t maxResults)
;
; Search for pattern in text using AVX2 instructions.
; Returns the number of matches found.
;
; Algorithm:
; 1. Broadcast first character of pattern to all 32 lanes of YMM register
; 2. Compare 32 bytes at a time to find potential match positions
; 3. For each potential match, verify full pattern
; 4. Store match positions in results array
;------------------------------------------------------------------------------
global simd_search_avx2
simd_search_avx2:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 16             ; reserve a local slot; maxResults at [rbp-48]
                            ; (must NOT reuse [rbp-8], which holds saved rbx)

    ; Save arguments
    mov r12, ARG1           ; text
    mov r13, ARG2           ; textLen
    mov r14, ARG3           ; pattern
    mov r15, ARG4           ; patternLen

%ifdef WIN64
    mov rbx, ARG5           ; results
    mov rax, ARG6           ; maxResults
    mov [rbp-48], rax
%else
    mov rbx, ARG5           ; results
    mov [rbp-48], ARG6      ; maxResults
%endif
    
    xor rax, rax            ; match count = 0
    
    ; Check for empty pattern or text
    test r15, r15
    jz .done
    test r13, r13
    jz .done
    
    ; Check if text is shorter than pattern
    cmp r13, r15
    jb .done
    
    ; Broadcast first character of pattern to all YMM lanes
    movzx ecx, byte [r14]
    vmovd xmm0, ecx
    vpbroadcastb ymm0, xmm0
    
    ; Calculate end position (textLen - patternLen + 1)
    mov r9, r13
    sub r9, r15
    inc r9                  ; r9 = number of positions to check
    
    xor r8, r8              ; r8 = current position
    
.search_loop:
    ; Check if we have at least 32 bytes left
    mov rcx, r9
    sub rcx, r8
    cmp rcx, 32
    jb .scalar_search
    
    ; Load 32 bytes from text
    vmovdqu ymm1, [r12 + r8]
    
    ; Compare with first char of pattern
    vpcmpeqb ymm2, ymm1, ymm0
    
    ; Get match mask
    vpmovmskb ecx, ymm2
    
    ; Process matches
    test ecx, ecx
    jz .advance_32
    
.process_matches:
    ; Find position of lowest set bit
    bsf edx, ecx
    jz .advance_32
    
    ; Calculate absolute position
    lea r10, [r8 + rdx]
    
    ; Clear this bit
    btr ecx, edx
    
    ; Verify full pattern match
    push rcx
    push r8
    
    ; Compare rest of pattern
    mov rdi, r12
    add rdi, r10            ; text + position
    mov rsi, r14            ; pattern
    mov rcx, r15            ; patternLen
    
    ; Use repe cmpsb for full pattern comparison
    repe cmpsb
    jne .no_match
    
    ; Found a match - store position
    cmp rax, [rbp-48]        ; Check maxResults
    jge .no_match

    mov [rbx + rax*8], r10
    inc rax

    ; Non-overlapping: resume scanning past the end of this match so the
    ; results buffer only needs room for non-overlapping matches.
    pop r8                  ; discard saved window base
    pop rcx                 ; discard saved mask
    lea r8, [r10 + r15]     ; next scan position = match start + patternLen
    cmp r8, r9
    jb .search_loop
    jmp .done

.no_match:
    pop r8
    pop rcx

    ; Continue processing bits in mask
    test ecx, ecx
    jnz .process_matches

.advance_32:
    add r8, 32
    cmp r8, r9
    jb .search_loop
    jmp .done
    
.scalar_search:
    ; Scalar fallback for remaining bytes
    cmp r8, r9
    jge .done
    
    ; Compare first character
    movzx ecx, byte [r12 + r8]
    movzx edx, byte [r14]
    cmp cl, dl
    jne .scalar_next
    
    ; Compare full pattern
    push rax
    mov rdi, r12
    add rdi, r8
    mov rsi, r14
    mov rcx, r15
    repe cmpsb
    pop rax
    jne .scalar_next
    
    ; Store match
    cmp rax, [rbp-48]
    jge .scalar_next
    mov [rbx + rax*8], r8
    inc rax
    add r8, r15            ; non-overlapping: skip past the match
    jmp .scalar_search

.scalar_next:
    inc r8
    jmp .scalar_search
    
.done:
    vzeroupper              ; Clean up AVX state

    add rsp, 16             ; release local slot
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

;------------------------------------------------------------------------------
; size_t simd_search_sse42(const char* text, size_t textLen,
;                          const char* pattern, size_t patternLen,
;                          size_t* results, size_t maxResults)
;
; Search for pattern in text using SSE4.2 instructions.
; Uses PCMPISTRI for efficient string comparison.
;------------------------------------------------------------------------------
global simd_search_sse42
simd_search_sse42:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 16             ; reserve a local slot; maxResults at [rbp-48]
                            ; (must NOT reuse [rbp-8], which holds saved rbx)

    mov r12, ARG1           ; text
    mov r13, ARG2           ; textLen
    mov r14, ARG3           ; pattern
    mov r15, ARG4           ; patternLen
    
%ifdef WIN64
    mov rbx, ARG5           ; results
    mov rax, ARG6
    mov [rbp-48], rax        ; maxResults
%else
    mov rbx, ARG5           ; results
    mov [rbp-48], ARG6       ; maxResults
%endif
    
    xor rax, rax            ; match count
    
    ; Validate inputs
    test r15, r15
    jz .sse_done
    test r13, r13
    jz .sse_done
    cmp r13, r15
    jb .sse_done
    
    ; Load pattern into xmm0 (up to 16 bytes)
    cmp r15, 16
    ja .sse_long_pattern
    
    ; Short pattern - use PCMPISTRI
    movdqu xmm0, [r14]
    
    mov r9, r13
    sub r9, r15
    inc r9
    
    xor r8, r8
    
.sse_search_loop:
    cmp r8, r9
    jge .sse_done
    
    ; Load 16 bytes from text
    movdqu xmm1, [r12 + r8]
    
    ; PCMPISTRI: find first match of pattern in text
    ; imm8 = 0x0C: equal ordered (substring match)
    pcmpistri xmm0, xmm1, 0x0C
    
    jc .sse_found_potential
    
    add r8, 16
    jmp .sse_search_loop
    
.sse_found_potential:
    ; ecx contains position of match
    lea r10, [r8 + rcx]
    
    ; Verify full match if pattern > 16 bytes not needed here
    ; Store match
    cmp rax, [rbp-48]
    jge .sse_continue
    
    mov [rbx + rax*8], r10
    inc rax
    
.sse_continue:
    ; Non-overlapping: resume past the end of the match (r10 = match start).
    lea r8, [r10 + r15]
    jmp .sse_search_loop
    
.sse_long_pattern:
    ; For patterns > 16 bytes, use first char comparison
    movzx ecx, byte [r14]
    movd xmm0, ecx
    pxor xmm2, xmm2
    pshufb xmm0, xmm2       ; Broadcast to all bytes
    
    mov r9, r13
    sub r9, r15
    inc r9
    xor r8, r8
    
.sse_long_loop:
    mov rcx, r9
    sub rcx, r8
    cmp rcx, 16
    jb .sse_scalar
    
    movdqu xmm1, [r12 + r8]
    pcmpeqb xmm1, xmm0
    pmovmskb ecx, xmm1
    
    test ecx, ecx
    jz .sse_advance_16
    
.sse_process_long:
    bsf edx, ecx
    jz .sse_advance_16
    
    lea r10, [r8 + rdx]
    btr ecx, edx
    
    ; Verify full pattern
    push rcx
    push r8
    mov rdi, r12
    add rdi, r10
    mov rsi, r14
    mov rcx, r15
    repe cmpsb
    jne .sse_no_match
    
    cmp rax, [rbp-48]
    jge .sse_no_match
    mov [rbx + rax*8], r10
    inc rax

    ; Non-overlapping: resume past the end of this match.
    pop r8                  ; discard saved window base
    pop rcx                 ; discard saved mask
    lea r8, [r10 + r15]
    cmp r8, r9
    jb .sse_long_loop
    jmp .sse_done

.sse_no_match:
    pop r8
    pop rcx
    test ecx, ecx
    jnz .sse_process_long
    
.sse_advance_16:
    add r8, 16
    cmp r8, r9
    jb .sse_long_loop
    jmp .sse_done
    
.sse_scalar:
    cmp r8, r9
    jge .sse_done
    
    movzx ecx, byte [r12 + r8]
    movzx edx, byte [r14]
    cmp cl, dl
    jne .sse_scalar_next
    
    push rax
    mov rdi, r12
    add rdi, r8
    mov rsi, r14
    mov rcx, r15
    repe cmpsb
    pop rax
    jne .sse_scalar_next
    
    cmp rax, [rbp-48]
    jge .sse_scalar_next
    mov [rbx + rax*8], r8
    inc rax
    add r8, r15            ; non-overlapping: skip past the match
    jmp .sse_scalar

.sse_scalar_next:
    inc r8
    jmp .sse_scalar
    
.sse_done:
    add rsp, 16             ; release local slot
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

;------------------------------------------------------------------------------
; size_t simd_memchr(const char* text, size_t textLen, char c)
;
; Find first occurrence of character c in text using SIMD.
; Returns offset of first occurrence, or textLen if not found.
;------------------------------------------------------------------------------
global simd_memchr
simd_memchr:
    push rbp
    mov rbp, rsp
    
    mov rax, ARG2           ; Return textLen if not found
    
    test ARG2, ARG2
    jz .memchr_done
    
    ; Broadcast search character
    movzx ecx, ARG3b
    vmovd xmm0, ecx
    vpbroadcastb ymm0, xmm0
    
    xor r8, r8
    
.memchr_loop:
    mov rcx, ARG2
    sub rcx, r8
    cmp rcx, 32
    jb .memchr_scalar
    
    vmovdqu ymm1, [ARG1 + r8]
    vpcmpeqb ymm2, ymm1, ymm0
    vpmovmskb ecx, ymm2
    
    test ecx, ecx
    jnz .memchr_found
    
    add r8, 32
    jmp .memchr_loop
    
.memchr_found:
    bsf ecx, ecx
    lea rax, [r8 + rcx]
    jmp .memchr_cleanup
    
.memchr_scalar:
    cmp r8, ARG2
    jge .memchr_done
    
    movzx ecx, byte [ARG1 + r8]
    cmp cl, ARG3b
    je .memchr_found_scalar
    
    inc r8
    jmp .memchr_scalar
    
.memchr_found_scalar:
    mov rax, r8
    
.memchr_cleanup:
    vzeroupper
    
.memchr_done:
    pop rbp
    ret

; Mark the stack as non-executable (ELF). Silences the linker
; "missing .note.GNU-stack section implies executable stack" warning.
%ifidn __OUTPUT_FORMAT__, elf64
section .note.GNU-stack noalloc noexec nowrite progbits
%endif
