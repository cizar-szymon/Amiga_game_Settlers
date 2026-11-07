/*
 * draw_runs_68k.c - the rows of a sprite from its runs into the card's
 * memory, in 68k assembly (0.129): the same as draw_runs<true> in
 * video-amiga.cc, which stays the reference (VERIFY, the fast RAM path).
 *
 * A run code: 0x00-0x3f skip n+1, 0x40-0x7f copy n+1 (the pixels follow),
 * 0x80-0xbf shade n+1, 0xc0 end of the row. A copy is written as a byte
 * and a word up to a longword boundary, longwords, a word and a byte; a
 * shade run is read whole by longwords, darkened through the table and
 * written back (one turn from writing to reading the card per run).
 *
 * void draw_runs_vram68k(uint8_t *d, int stride, const uint8_t *r, int h,
 *                        const uint8_t *shade);
 */

__asm__(
"	.text\n"
"	.even\n"
"	.globl	_draw_runs_vram68k\n"
"_draw_runs_vram68k:\n"
"	movem.l	d2-d7/a2-a5,-(sp)\n"
"	lea	-80(sp),sp\n"
"	move.l	124(sp),a3\n"           /* d */
"	move.l	128(sp),d6\n"           /* stride */
"	move.l	132(sp),a1\n"           /* r */
"	move.l	136(sp),d7\n"           /* h */
"	move.l	140(sp),a2\n"           /* shade */
"	tst.l	d7\n"
"	ble	.Lr_done\n"
".Lr_row:\n"
"	move.l	a3,a0\n"
".Lr_next:\n"
"	moveq	#0,d0\n"
"	move.b	(a1)+,d0\n"
"	bmi	.Lr_shade\n"
"	cmp.b	#0x40,d0\n"
"	bcc	.Lr_copy\n"
"	lea	1(a0,d0.l),a0\n"        /* skip */
"	bra	.Lr_next\n"
".Lr_copy:\n"
"	sub.b	#0x3f,d0\n"             /* n 1..64 */
"	cmp.b	#4,d0\n"
"	bcs	.Lr_tail\n"
"	move.l	a0,d1\n"
"	btst	#0,d1\n"
"	beq	1f\n"
"	move.b	(a1)+,(a0)+\n"
"	subq.l	#1,d0\n"
"	move.l	a0,d1\n"
"1:	btst	#1,d1\n"
"	beq	2f\n"
"	move.w	(a1)+,(a0)+\n"
"	subq.l	#2,d0\n"
"2:	move.l	d0,d1\n"
"	lsr.l	#2,d1\n"
"	beq	.Lr_tail\n"
"	subq.l	#1,d1\n"
"3:	move.l	(a1)+,(a0)+\n"
"	dbf	d1,3b\n"
".Lr_tail:\n"
"	btst	#1,d0\n"
"	beq	4f\n"
"	move.w	(a1)+,(a0)+\n"
"4:	btst	#0,d0\n"
"	beq	.Lr_next\n"
"	move.b	(a1)+,(a0)+\n"
"	bra	.Lr_next\n"
".Lr_shade:\n"
"	cmp.b	#0xc0,d0\n"
"	bcc	.Lr_endrow\n"
"	sub.b	#0x7f,d0\n"             /* n 1..64 */
"	moveq	#3,d2\n"
"	move.l	a0,d1\n"
"	and.l	d1,d2\n"                /* lead */
"	move.l	a0,a4\n"
"	sub.l	d2,a4\n"                /* the longword the run starts in */
"	move.l	d2,d3\n"
"	add.l	d0,d3\n"
"	addq.l	#3,d3\n"
"	lsr.l	#2,d3\n"                /* longwords 1..17 */
"	move.l	sp,a5\n"
"	move.l	d3,d4\n"
"	subq.l	#1,d4\n"
"5:	move.l	(a4)+,(a5)+\n"
"	dbf	d4,5b\n"
"	lea	0(sp,d2.l),a5\n"
"	move.l	d0,d4\n"
"	subq.l	#1,d4\n"
"	moveq	#0,d1\n"
"6:	move.b	(a5),d1\n"
"	move.b	0(a2,d1.l),(a5)+\n"
"	dbf	d4,6b\n"
"	move.l	a0,a4\n"
"	sub.l	d2,a4\n"
"	move.l	sp,a5\n"
"	move.l	d3,d4\n"
"	subq.l	#1,d4\n"
"7:	move.l	(a5)+,(a4)+\n"
"	dbf	d4,7b\n"
"	add.l	d0,a0\n"
"	bra	.Lr_next\n"
".Lr_endrow:\n"
"	add.l	d6,a3\n"
"	subq.l	#1,d7\n"
"	bgt	.Lr_row\n"
".Lr_done:\n"
"	lea	80(sp),sp\n"
"	movem.l	(sp)+,d2-d7/a2-a5\n"
"	rts\n"
);
