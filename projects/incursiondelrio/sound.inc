I8253_CONTROL		equ $8
I8253_CH0			equ $b
I8253_CH1			equ $a
CH0_CTR_BEEP		equ 00110110b
CH0_CTR_SILENCE		equ 00110000b

CH1_CTR_BEEP 		equ 01100110b


snd_on:				db 0
snd_bell_count:		db 0
snd_bell_ptr:		dw snd_bell
snd_bell:			
					db $10, $10, $10, $10
					db $10, $9, $10, $9
					db $10, $10, $10, $10
					db $10, $9, $10, $9
					db $10, $10, $10, $10
					db $10, $9, $10, $9

					db $10, $10, $10, $10
					db $10, $10, $10, $10
					db $10, $10, $10, $0
					db $11, $0, $10, $0
snd_bell_end		equ .

SoundInit:
	lxi h, snd_bell
	shld snd_bell_ptr
	mvi a, snd_bell_end - snd_bell-1
	sta snd_bell_count
	mvi a, 0
	sta snd_on

	mvi a, CH1_CTR_BEEP
	out I8253_CONTROL
	ret

SoundSound:
	lda player_collision
	ora a
	jz ss_silence
	mvi a, CH0_CTR_BEEP
	out I8253_CONTROL
	mvi a, $0
	out I8253_CH0
	mvi a, $10
	out I8253_CH0
	ret
ss_silence:
	mvi a, CH0_CTR_SILENCE
	out I8253_CONTROL
	mvi a, $ff
	out I8253_CH0
	out I8253_CH0
	ret

SoundNoise:
	call nextRandomSnd
	lda playerYspeed
	sui YSPEED_DEFAULT
	cma
	adi $80
	mov b, a
	;mvi b, $80
	lda random_sndLo
	ani $3f
	add b
	out I8253_CH1
	ret

SoundFadeOut:
	push d 

	lda snd_bell_count
	ora a
	jm sfo_shutup


	lda snd_on
	mov b, a

	lhld snd_bell_ptr
	mov a, m
	mov d, a
	ora a
	jnz sfo_snd

sfo_silence:
	mov a, b
	ora a
	jz sfo_done

	mvi a, CH0_CTR_SILENCE
	out I8253_CONTROL
	mvi a, $ff
	out I8253_CH0
	out I8253_CH0
	xra a
	sta snd_on
	jmp sfo_done

sfo_snd:
	mov a, b
	cmp d
	jz sfo_done 

	mvi a, CH0_CTR_BEEP
	out I8253_CONTROL
	mvi a, $0
	out I8253_CH0
	mov a, d
	;mvi a, $10
	out I8253_CH0
	mov a, d
	sta snd_on 
sfo_done:
	inx h
	shld snd_bell_ptr
	lxi h, snd_bell_count
	dcr m
	pop d
	ret
sfo_shutup:
	mvi a, CH0_CTR_SILENCE
	out I8253_CONTROL
	mvi a, $ff
	out I8253_CH0
	out I8253_CH0
	pop d
	ret

nextRandomSnd:

	lhld random_snd
	mov a, h 		; randomHi
   	rlc            
   	rlc            
   	rlc    
   	xra h
   	rlc
   	mov h, a
   	mov a, l
   	ral 
   	mov l, a
   	mov a, h
   	ral
	mov h, a
	shld random_snd
	ret

random_snd:
random_sndLo:	db $14
random_sndHi:	db $a8
