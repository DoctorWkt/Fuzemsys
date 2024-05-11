#define BYTE(x)		(((unsigned)(x)) & 0xFF)
#define WORD(x)		(((unsigned)(x)) & 0xFFFF)

#define ARGBASE	2	/* Bytes between arguments and locals if no reg saves */

#define T_NREF		(T_USER)		/* Load of C global/static */
#define T_CALLNAME	(T_USER+1)		/* Function call by name */
#define T_NSTORE	(T_USER+2)		/* Store to a C global/static */
#define T_LREF		(T_USER+3)		/* Ditto for local */
#define T_LSTORE	(T_USER+4)
#define T_LBREF		(T_USER+5)		/* Ditto for labelled strings or local static */
#define T_LBSTORE	(T_USER+6)
#define T_DEREFPLUS	(T_USER+7)
#define T_EQPLUS	(T_USER+8)
#define T_LDEREF	(T_USER+9)	/* *local + offset */
#define T_LEQ		(T_USER+10)	/* *local + offset = n */
#define T_NDEREF	(T_USER+11)	/* *(name + offset) */
#define T_LBDEREF	(T_USER+12)	/* *(name + offset) */
#define T_NEQ		(T_USER+13)	/* *(name + offset) = n */
#define T_LBEQ		(T_USER+14)	/* *(label + offset) = n */
#define T_RREF		(T_USER+15)
#define T_RSTORE	(T_USER+16)
#define T_RDEREF	(T_USER+17)		/* *regptr */
#define T_REQ		(T_USER+18)		/* *regptr = */
#define T_RDEREFPLUS	(T_USER+19)		/* *regptr++ */
#define T_REQPLUS	(T_USER+20)		/* *regptr++ =  */

extern unsigned frame_len;	/* Number of bytes of stack frame */
extern unsigned sp;		/* Stack pointer offset tracking */
extern unsigned argbase;	/* Argument offset in current function */

extern unsigned cpu_has_d;	/* 16bit ops and 'D' are present */
extern unsigned cpu_has_xgdx;	/* XGDX is present */
extern unsigned cpu_has_abx;	/* ABX is present */
extern unsigned cpu_has_pshx;	/* Has PSHX PULX */
extern unsigned cpu_has_y;	/* Has Y register */
extern unsigned cpu_has_lea;	/* Has LEA. For now 6809 but if we get to HC12... */
extern unsigned cpu_is_09;	/* Bulding for 6x09 so a bit different */
extern unsigned cpu_pic;	/* Position independent output (6809 only) */
extern const char *ld8_op;
extern const char *st8_op;
extern const char *pic_op;

/* Tracking */
extern uint8_t a_val;
extern uint8_t b_val;
extern unsigned a_valid;
extern unsigned b_valid;
extern unsigned d_valid;
extern uint16_t x_fpoff;
extern unsigned x_fprel;
