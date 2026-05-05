PUBLIC kaslr_probe
PUBLIC kaslr_poke

_TEXT SEGMENT

kaslr_probe PROC
        push    rbx
        mov     r10, rcx                    ; preserve probe address

        mfence
        rdtscp                              ; pre  -> edx:eax
        shl     rdx, 32
        or      rdx, rax
        mov     rbx, rdx                    ; rbx = pre
        lfence

        prefetchnta byte ptr [r10]
        prefetcht2  byte ptr [r10]

        lfence
        rdtscp                              ; post -> edx:eax
        shl     rdx, 32
        or      rdx, rax                    ; rdx = post
        mfence

        sub     rdx, rbx
        mov     rax, rdx                    ; return post - pre
        pop     rbx
        ret
kaslr_probe ENDP

kaslr_poke PROC
        mov     rax, 99999
        syscall
        ret
kaslr_poke ENDP

_TEXT ENDS
END
