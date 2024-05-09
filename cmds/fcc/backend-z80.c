/*
 *	Z80 Backend
 *
 *	The Z80 essentially has three ways of dealing with locals
 *
 *	1.	8080 style	LD HL,offset ADD HL,SP  { LD E,(HL), INC HL, LD D,(HL) }
 *				7 bytes, 21 + 20 -> 41 clocks.
 *	2.	Call helpers	Akin again to the 8080 port
 *				3 - 5 bytes, but slower (28 cycles minimum extra cost) 66 for word helper
 *	3.	IX frame pointer style
 *				LD E,(ix + n), LD D,(ix + n  + 1)
 *				6 bytes 38 cycles
 *				much higher function entry/exit cost
 *
 *	We use IX, IY and BC as "register" variables. If IX or IY is free and we are not size
 *	optimizing we use IX or IY as a frame pointer. If not helpers.
 *
 *	The situation only improves if we get to Z280 or Rabbit processors
 *
 *	R2000/R3000:	LD	HL,(SP + n)  8bit unsigned (ditto IX IY)
 *			LD	HL,(HL + n) ; IX+ IY+
 * 			LD	(HL + n), HL	; IX+ IY+
 *			LD	(SP + n), HL  ; IX+ IY+
 *
 *	Z80n		As Z80 but can add A and constants to 8/16bit directly which helps in
 *			some stack accessing by avoiding a load and push constant
 *
 *	eZ80		Adds LD BC/DE?HL,(HL) and reverse or HL/IX/IY so now frame pointer
 *			is actually useful
 *
 *	Z280		Can use IX+off for 16bit ops including loads and add so like the ez80
 *			frame pointers look useful, but this extends to other ops like ADDW INCW
 *			etc. Has LDA (ie LEA) so can LDA IX,(SP - n) etc. Can also load and
 *			save IX IY and HL from (SP + n). Can also push constants and relative
 *			addresses
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "backend.h"

/* Until we do more testing */
#define IS_EZ80		0	/* EZ80 has ld de,(hl) and friends but not offset */
#define IS_RABBIT	0	/* Has ld hl,(rr + n) and vice versa but only 16bit */
#define HAS_LDHLSP	0	/* Can ld hl,(sp + n) and vice versa */
#define HAS_LDASP	0	/* Can ld a,(sp + n) and vice versa */
#define HAS_LDHLHL	0	/* Can ld hl,(hl) or hl,(hl + 0) */

#define BYTE(x)		(((unsigned)(x)) & 0xFF)
#define WORD(x)		(((unsigned)(x)) & 0xFFFF)

#define ARGBASE	2	/* Bytes between arguments and locals if no reg saves */

#define LWDIRECT 24	/* Number of __ldword1 __ldword2 etc forms for fastest access */

/*
 *	Upper node flag fields are ours
 */

#define USECC	0x0100

static const char ccnormal[] = "nzz ";
static const char ccinvert[] = "z nz";

/*
 *	State for the current function
 */
static unsigned frame_len;	/* Number of bytes of stack frame */
static unsigned sp;		/* Stack pointer offset tracking */
static unsigned argbase;	/* Argument offset in current function */
static unsigned unreachable;	/* Code after an unconditional jump */
static unsigned func_cleanup;	/* Zero if we can just ret out */
static unsigned use_fp;		/* Using a frame pointer this function */
static const char *ccflags = ccnormal;/* True, False flags */

static const char *regnames[] = {	/* Register variable names */
	NULL,
	"bc",
	"ix",
	"iy"
};

/* Check if a single bit is set or clear */

static int bitcheckb1(uint8_t n)
{
	unsigned m = 1;
	unsigned i;

	for (i = 0; i < 8; i++) {
		if (n == m)
			return i;
		m <<= 1;
	}
	return -1;
}

static int bitcheck1(unsigned n, unsigned s)
{
	unsigned m = 1;
	unsigned i;

	if (s == 1)
		return bitcheckb1(n);
	for (i = 0; i < 16; i++) {
		if (n == m)
			return i;
		m <<= 1;
	}
	return -1;
}

static int bitcheck0(unsigned n, unsigned s)
{
	if (s == 1)
		return bitcheckb1((~n) & 0xFF);
	return bitcheck1((~n) & 0xFFFF, 2);
}

static unsigned get_size(unsigned t)
{
	if (PTR(t))
		return 2;
	if (t == CSHORT || t == USHORT)
		return 2;
	if (t == CCHAR || t == UCHAR)
		return 1;
	if (t == CLONG || t == ULONG || t == FLOAT)
		return 4;
	if (t == CLONGLONG || t == ULONGLONG || t == DOUBLE)
		return 8;
	if (t == VOID)
		return 0;
	fprintf(stderr, "type %x\n", t);
	error("gs");
	return 0;
}

static unsigned get_stack_size(unsigned t)
{
	unsigned n = get_size(t);
	if (n == 1)
		return 2;
	return n;
}

#define T_NREF		(T_USER)		/* Load of C global/static */
#define T_CALLNAME	(T_USER+1)		/* Function call by name */
#define T_NSTORE	(T_USER+2)		/* Store to a C global/static */
#define T_LREF		(T_USER+3)		/* Ditto for local */
#define T_LSTORE	(T_USER+4)
#define T_LBREF		(T_USER+5)		/* Ditto for labelled strings or local static */
#define T_LBSTORE	(T_USER+6)
#define T_RREF		(T_USER+7)
#define T_RSTORE	(T_USER+8)
#define T_RDEREF	(T_USER+9)		/* *regptr */
#define T_REQ		(T_USER+10)		/* *regptr */
#define T_BTST		(T_USER+11)		/* Use bit n, for and bit conditionals */
#define T_BYTEEQ	(T_USER+12)		/* Until we handle 8bit better */
#define T_BYTENE	(T_USER+13)

static void squash_node(struct node *n, struct node *o)
{
	n->value = o->value;
	n->val2 = o->val2;
	n->snum = o->snum;
	free_node(o);
}

static void squash_left(struct node *n, unsigned op)
{
	struct node *l = n->left;
	n->op = op;
	squash_node(n, l);
	n->left = NULL;
}

static void squash_right(struct node *n, unsigned op)
{
	struct node *r = n->right;
	n->op = op;
	squash_node(n, r);
	n->right = NULL;
}

/*
 *	Heuristic for guessing what to put on the right.  This is very
 *	processor dependent.  For Z80 we can fetch most global or static
 *	objects but locals are problematic
 */

static unsigned is_simple(struct node *n)
{
	unsigned op = n->op;

	/* Multi-word objects are never simple */
	if (!PTR(n->type) && (n->type & ~UNSIGNED) > CSHORT)
		return 0;
	/* We can load these directly into a register */
	if (op == T_CONSTANT || op == T_LABEL || op == T_NAME || op == T_REG)
		return 10;
	/* We can load this directly into a register but it may be a byte longer */
	if (op == T_NREF || op == T_LBREF)
		return 9;
	if (op == T_RREF || op == T_RDEREF)
		return 5;
	return 0;
}

/* Operators where we can push CCONLY downwards */
static unsigned is_ccdown(struct node *n)
{
	register unsigned op = n->op;
	if (op == T_ANDAND || op == T_OROR)
		return 1;
	if (op == T_BOOL)
		return 1;
	if (op == T_BANG && !(n->flags & CCFIXED))
		return 1;
	return 0;
}

/* Operators that we known to handle as CCONLY if possible
   TODO: add logic ops as we can BIT many of them */
static unsigned is_cconly(struct node *n)
{
	register unsigned op = n->op;
	if (op == T_EQEQ || op == T_BANGEQ || op == T_BYTEEQ ||
		op == T_BYTENE || op == T_ANDAND || op == T_OROR ||
		op == T_BOOL || op == T_BTST)
		return 1;
	if (op == T_BANG && !(n->flags & CCFIXED))
		return 1;
	return 0;
}

/*
 *	Try and push CCONLY down through the tree
 */
static void propogate_cconly(struct node *n)
{
	register struct node *l, *r;
	unsigned sz = get_size(n->type);
	unsigned val;

	l = n->left;
	r = n->right;


/*	printf("; considering %x %x\n", n->op, n->flags); */
	/* Only do this for nodes that are CCONLY. For example if we hit
	   an EQ (assign) then whilst the result of the assign may be
	   CC only, the subtree of the assignment is most definitely not */
	if (n->op != T_AND && !is_cconly(n) && !(n->flags & CCONLY))
		return;

	/* We have to special case BIT unfortunately, and this is ugly */

	/* A common C idiom is if (a & bit) which we can rewrite into
	   bit n,h or bit n,l */

	if (n->op == T_AND) {
/*		printf(";AND %x %x %x\n", n->op, r->op, n->flags); */
		if (r->op == T_CONSTANT && sz <= 2) {
			val = bitcheck1(r->value, sz);
			if (val != -1) {
				n->op = T_BTST;
				n->value = val;
				free_node(r);
				n->right = l;
				n->left = NULL;
				r = l;
				l = NULL;
			}
		} else
			return;
	}

	n->flags |= CCONLY;
	/* Deal with the CCFIXED limitations for now */
	if (n->flags & CCFIXED) {
		if (l)
			l->flags |= CCFIXED;
		if (r)
			r->flags |= CCFIXED;
	}
/*	printf(";made cconly %x\n", n->op); */
	/* Are we a node that can CCONLY downwards */
	if (is_ccdown(n)) {
/*		printf(";ccdown of %x L\n", n->op); */
		if (l)
			propogate_cconly(l);
/*		printf(";ccdown cont %x R\n", n->op); */
		if (r)
			propogate_cconly(r);
/*		printf(";ccdown done %x\n", n->op); */
	}
}

/*
 *	Allow aribtrary rewriting before the rewrite_node process is called
 *	bottom up. Lets us do things like working out which tree sections
 *	could be 8bit, or downward propagation of properties
 */
struct node *gen_rewrite(struct node *n)
{
	return n;
}

/* Can we stuff this type into a pointer for deref and assignment */

static int type_compatible(struct node *n, unsigned t)
{
	unsigned sz = get_size(t);
	if (sz == 1)
		return 1;
	if (n->value == 1)		/* BC char only */
		return 0;
	return 1;			/* IX and IY can do all sizes */
}
/*
 *	Our chance to do tree rewriting. We don't do much for the Z80
 *	at this point, but we do rewrite name references and function calls
 *	to make them easier to process. We also rewrite dereferences with
 *	offsets so we can use ix and iy nicely.
 */
struct node *gen_rewrite_node(struct node *n)
{
	struct node *l = n->left;
	struct node *r = n->right;
	struct node *c;
	unsigned op = n->op;
	unsigned nt = n->type;
	int val;

	/* Squash some byte comparisons down into byte ops */
/*	if (n->op == T_EQEQ && l->op == T_CAST) {
		printf(";EQEQ %x L %x T %x R %x V %lx\n",
			n->flags,
			l->op, l->right->type,
			r->op, r->value);
	} */
	if (n->op == T_EQEQ && l->op == T_CAST && l->right->type == UCHAR &&
		r->op == T_CONSTANT && r->value <= 0xFF) {
		n->op = T_BYTEEQ;
		n->value = r->value;
		n->right = l->right;
		n->left = NULL;
		free_node(l);	/* Dump the cast */
		free_node(r);
		return n;
	}
	/* Squash some byte comparisons down into byte ops */
	if (n->op == T_BANGEQ && l->op == T_CAST && l->right->type == UCHAR &&
		r->op == T_CONSTANT && r->value <= 0xFF) {
		n->op = T_BYTENE;
		n->value = r->value;
		n->right = l->right;
		n->left = NULL;
		free_node(l);	/* Dump the cast */
		free_node(r);
		return n;
	}

	/* spot the following tree
			T_DEREF
			    T_PLUS
		       T_RREF    T_CONSTANT -128-127-size
	  so we can rewrite EQ/RDEREF off base + offset from pointer within range using ix offset */
	if (op == T_DEREF) {
		if (r->op == T_PLUS) {
			c = r->right;
			if (r->left->op == T_RREF && c->op == T_CONSTANT && type_compatible(r->left, nt)) {
				val = c->value;
				/* For now - depends on size */
				/* IX and IY only ranged, BC char * direct */
				if (val == 0 || (val >= -128 && val < 125 && r->left->value != 1)) {
					n->op = T_RDEREF;
					n->val2 = val;
					n->value = r->left->value;
					n->right = NULL;
					free_tree(r);
					return n;
				}
			}
		} else if (r->op == T_RREF && type_compatible(r, nt)) {
			/* Check - are we ok with BC always ? */
			n->op = T_RDEREF;
			n->val2 = 0;
			n->value = r->value;
			n->right = NULL;
			free_node(r);
			return n;
		}
	}
	if (op == T_EQ) {
		if (l->op == T_PLUS) {
			c = l->right;
			if (l->left->op == T_RREF && c->op == T_CONSTANT) {
				val = c->value;
				/* For now - depends on size */
				/* IX and IY only -  BC char * direct only */
				if (val == 0 || (val >= -128 && val < 125 && l->left->value != 1)) {
					n->op = T_REQ;
					n->val2 = val;
					n->value = l->left->value;
					n->left = NULL;
					free_tree(l);
					return n;
				}
			}
		} else if (l->op == T_RREF) {
			val = l->value;
			/* For now - depends on size */
			/* IX and IY only -  BC char * direct only */
			if (val == 0 || (val >= -128 && val < 125 && l->value != 1)) {
				n->op = T_REQ;
				n->val2 = 0;
				n->value = val;
				n->left = NULL;
				free_node(l);
				return n;
			}
		}
	}
	/* Merge offset to object into a  single direct reference */
	if (op == T_PLUS && r->op == T_CONSTANT &&
		(l->op == T_LOCAL || l->op == T_NAME || l->op == T_LABEL || l->op == T_ARGUMENT)) {
		/* We don't care if the right offset is 16bit or 32 as we've
		   got 16bit pointers */
		l->value += r->value;
		free_node(r);
		free_node(n);
		return l;
	}
	/* Rewrite references into a load operation */
	if (nt == CCHAR || nt == UCHAR || nt == CSHORT || nt == USHORT || PTR(nt)) {
		if (op == T_DEREF) {
			if (r->op == T_LOCAL || r->op == T_ARGUMENT) {
				if (r->op == T_ARGUMENT)
					r->value += argbase + frame_len;
				squash_right(n, T_LREF);
				return n;
			}
			if (r->op == T_REG) {
				squash_right(n, T_RREF);
				return n;
			}
			if (r->op == T_NAME) {
				squash_right(n, T_NREF);
				return n;
			}
			if (r->op == T_LABEL) {
				squash_right(n, T_LBREF);
				return n;
			}
			if (r->op == T_RREF) {
				squash_right(n, T_RDEREF);
				n->val2 = 0;
				return n;
			}
		}
		if (op == T_EQ) {
			if (l->op == T_NAME) {
				squash_left(n, T_NSTORE);
				return n;
			}
			if (l->op == T_LABEL) {
				squash_left(n, T_LBSTORE);
				return n;
			}
			if (l->op == T_LOCAL || l->op == T_ARGUMENT) {
				if (l->op == T_ARGUMENT)
					l->value += argbase + frame_len;
				squash_left(n, T_LSTORE);
				return n;
			}
			if (l->op == T_REG) {
				squash_left(n, T_RSTORE);
				return n;
			}
		}
	}
	/* Eliminate casts for sign, pointer conversion or same */
	if (op == T_CAST) {
		if (nt == r->type || (nt ^ r->type) == UNSIGNED ||
		 (PTR(nt) && PTR(r->type))) {
			free_node(n);
			return r;
		}
	}
	/* Rewrite function call of a name into a new node so we can
	   turn it easily into call xyz */
	if (op == T_FUNCCALL && r->op == T_NAME && PTR(r->type) == 1) {
		n->op = T_CALLNAME;
		n->snum = r->snum;
		n->value = r->value;
		free_node(r);
		n->right = NULL;
	}
	/* Commutive operations. We can swap the sides over on these */
	if (op == T_AND || op == T_OR || op == T_HAT || op == T_STAR || op == T_PLUS) {
/*		printf(";left %d right %d\n", is_simple(n->left), is_simple(n->right)); */
		if (is_simple(n->left) > is_simple(n->right)) {
			n->right = l;
			n->left = r;
		}
	}
	return n;
}

/* Export the C symbol */
void gen_export(const char *name)
{
	printf("	.export _%s\n", name);
}

void gen_segment(unsigned segment)
{
	switch(segment) {
	case A_CODE:
		printf("\t.%s\n", codeseg);
		break;
	case A_DATA:
		printf("\t.data\n");
		break;
	case A_BSS:
		printf("\t.bss\n");
		break;
	case A_LITERAL:
		printf("\t.literal\n");
		break;
	default:
		error("gseg");
	}
}

/* Generate the function prologue - may want to defer this until
   gen_frame for the most part */
void gen_prologue(const char *name)
{
	printf("_%s:\n", name);
	unreachable = 0;
}

/* Generate the stack frame */
/* TODO: defer this to statements so we can ld/push initializers */
void gen_frame(unsigned size,  unsigned aframe)
{
	frame_len = size;
	sp = 0;
	use_fp = 0;

	if (size || (func_flags & (F_REG(1)|F_REG(2)|F_REG(3))))
		func_cleanup = 1;
	else
		func_cleanup = 0;

	argbase = ARGBASE;

	/* In banked mode the arguments are two bytes further out */
	if (cpufeat & 1)
		argbase += 2;

	if (func_flags & F_REG(1)) {
		printf("\tpush bc\n");
		argbase += 2;
	}
	if (func_flags & F_REG(2)) {
		printf("\tpush ix\n");
		argbase += 2;
	}
	if (func_flags & F_REG(3)) {
		printf("\tpush iy\n");
		argbase += 2;
	} else {
		/* IY is free use it as a frame pointer ? */
		if (!optsize && size > 4) {
			argbase += 2;
			printf("\tpush iy\n");
			/* Remember we need to restore IY */
			func_flags |= F_REG(3);
			use_fp = 1;
		}
	}
	/* If we are building a frame pointer we need to do this work anyway */
	if (use_fp) {
		printf("\tld iy,0x%x\n", (uint16_t) - size);
		printf("\tadd iy,sp\n");
		printf("\tld sp,iy\n");
		return;
	}
	if (size > 10) {
		printf("\tld hl,0x%x\n", (uint16_t) -size);
		printf("\tadd hl,sp\n");
		printf("\tld sp,hl\n");
		return;
	}
	if (size & 1) {
		printf("\tdec sp\n");
		size--;
	}
	while(size) {
		printf("\tpush hl\n");
		size -= 2;
	}
}

void gen_epilogue(unsigned size, unsigned argsize)
{
	if (sp != 0)
		error("sp");

	/* Return in HL, does need care on stack. TOOD: flag void functions
	   where we can burn the return */
	sp -= size;

	/* This can happen if the function never returns or the only return
	   is a by a ret directly (ie from a function without locals) */
	if (unreachable)
		return;

	if (size > 10) {
		unsigned x = func_flags & F_VOIDRET;
		if (!x)
			printf("\tex de,hl\n");
		printf("\tld hl,0x%x\n", (uint16_t)size);
		printf("\tadd hl,sp\n");
		printf("\tld sp,hl\n");
		if (!x)
			printf("\tex de,hl\n");
	} else {
		if (size & 1) {
			printf("\tinc sp\n");
			size--;
		}
		while (size) {
			printf("\tpop de\n");
			size -= 2;
		}
	}
	if (func_flags & F_REG(3))
		printf("\tpop iy\n");
	if (func_flags & F_REG(2))
		printf("\tpop ix\n");
	if (func_flags & F_REG(1))
		printf("\tpop bc\n");
	printf("\tret\n");
	unreachable = 1;
}

void gen_label(const char *tail, unsigned n)
{
	unreachable = 0;
	printf("L%u%s:\n", n, tail);
}

/* A return statement. We can sometimes shortcut this if we have
   no cleanup to do */
unsigned gen_exit(const char *tail, unsigned n)
{
	if (func_cleanup) {
		gen_jump(tail, n);
		return 0;
	}
	else {
		printf("\tret\n");
		unreachable = 1;
		return 1;
	}
}

void gen_jump(const char *tail, unsigned n)
{
	printf("\tjr L%u%s\n", n, tail);
	unreachable = 1;
}

void gen_jfalse(const char *tail, unsigned n)
{
	printf("\tjr %s,L%u%s\n", ccflags + 2, n, tail);
	ccflags = ccnormal;
}

void gen_jtrue(const char *tail, unsigned n)
{
	printf("\tjr %c%c,L%u%s\n", ccflags[0], ccflags[1], n, tail);
	ccflags = ccnormal;
}

static void gen_cleanup(unsigned v)
{
	/* CLEANUP is special and needs to be handled directly */
	sp -= v;
	if (v > 10) {
		/* This is more expensive, but we don't often pass that many
		   arguments so it seems a win to stay in HL */
		/* TODO: spot void function and skip ex de,hl */
		printf("\tex de,hl\n");
		printf("\tld hl,0x%x\n", v);
		printf("\tadd hl,sp\n");
		printf("\tld sp,hl\n");
		printf("\tex de,hl\n");
	} else {
		while(v >= 2) {
			printf("\tpop de\n");
			v -= 2;
		}
		if (v)
			printf("\tinc sp\n");
	}
}

/*
 *	Helper handlers. We use a tight format for integers but C
 *	style for float as we'll have C coded float support if any
 */

/* True if the helper is to be called C style */
static unsigned c_style(struct node *np)
{
	register struct node *n = np;
	/* Assignment is done asm style */
	if (n->op == T_EQ)
		return 0;
	/* Float ops otherwise are C style */
	if (n->type == FLOAT)
		return 1;
	n = n->right;
	if (n && n->type == FLOAT)
		return 1;
	return 0;
}

void gen_helpcall(struct node *n)
{
	/* Check both N and right because we handle casts to/from float in
	   C call format */
	if (c_style(n))
		gen_push(n->right);
	printf("\tcall __");
}

void gen_helpclean(struct node *n)
{
	unsigned s;

	if (c_style(n)) {
		s = 0;
		if (n->left) {
			s += get_size(n->left->type);
			/* gen_node already accounted for removing this thinking
			   the helper did the work, adjust it back as we didn't */
			sp += s;
			}
		s += get_size(n->right->type);
		gen_cleanup(s);
		/* C style ops that are ISBOOL didn't set the bool flags */
		if (n->flags & ISBOOL)
			printf("\txor a\n\tcp l\n");
	}
}

void gen_switch(unsigned n, unsigned type)
{
	printf("\tld de,Sw%u\n", n);
	printf("\tjp __switch");
	helper_type(type, 0);
	printf("\n");
	unreachable = 1;
}

void gen_switchdata(unsigned n, unsigned size)
{
	printf("Sw%u:\n", n);
	printf("\t.word %u\n", size);
}

void gen_case_label(unsigned tag, unsigned entry)
{
	unreachable = 0;
	printf("Sw%u_%u:\n", tag, entry);
}

void gen_case_data(unsigned tag, unsigned entry)
{
	printf("\t.word Sw%u_%u\n", tag, entry);
}

void gen_data_label(const char *name, unsigned align)
{
	printf("_%s:\n", name);
}

void gen_space(unsigned value)
{
	printf("\t.ds %u\n", value);
}

void gen_text_data(unsigned n)
{
	printf("\t.word T%u\n", n);
}

/* The label for a literal (currently only strings)
   TODO: if we add other literals we may need alignment here */

void gen_literal(unsigned n)
{
	if (n)
		printf("T%u:\n", n);
}

void gen_name(struct node *n)
{
	printf("\t.word _%s+%u\n", namestr(n->snum), WORD(n->value));
}

void gen_value(unsigned type, unsigned long value)
{
	unsigned w = WORD(value);
	if (PTR(type)) {
		printf("\t.word %u\n", w);
		return;
	}
	switch (type) {
	case CCHAR:
	case UCHAR:
		printf("\t.byte %u\n", BYTE(w));
		break;
	case CSHORT:
	case USHORT:
		printf("\t.word %u\n", w);
		break;
	case CLONG:
	case ULONG:
	case FLOAT:
		/* We are little endian */
		printf("\t.word %u\n", w);
		printf("\t.word %u\n", (unsigned) ((value >> 16) & 0xFFFF));
		break;
	default:
		error("unsuported type");
	}
}

void gen_start(void)
{
	/* For now.. */
	printf("\t.z80\n");
}

void gen_end(void)
{
}

void gen_tree(struct node *n)
{
	codegen_lr(n);
	printf(";\n");
/*	printf(";SP=%u\n", sp); */
}

/*
 *	Try and generate shorter code for stuff we can directly access
 */

/*
 *	Locals are complicated on the Z80. There is no simply cheap way
 *	to access them so put all the gunk in a helper we can refine
 *	over time for different CPU variants and frame pointers etc
 *
 *	Given a size and offset on the stack, write the code to put
 *	the value into HL without trashing BC or DE
 *
 *	If to_de is set we want the result in de, otherwise HL
 */
unsigned generate_lref(unsigned v, unsigned size, unsigned to_de)
{
	const char *name;
	const char *rp = "hl";

	if (to_de)
		rp = "de";

	if (size == 4)
		return 0;

	/* Rabbit amd Z280 have LD HL,(SP + n) */
	if (HAS_LDHLSP && v + sp <= 255 && !to_de) {
		/* FIXME: We will load an extra byte for 16bit, so hopefully non MMIO TODO */
		printf("\tld hl,(sp+%u)\n", v + sp);
		return 1;
	}
	/* This has to be a local so if it is byte sized we will load the
	   low byte and crap above and all is good */
	if (v + sp == 0 && size <= 2) {
		printf("\tpop %s\n\tpush %s\n", rp, rp);
		return 1;
	}
	/* Frame pointers */
	if (use_fp && v < 128 - size) {
		printf("\tld %c,(iy + %u)\n", rp[1], v);
		if (size == 2)
			printf("\tld %c,(iy + %u)\n", rp[0], v + 1);
		return 1;
	}

	/* Correct for current SP location */
	v += sp;

	/* Byte load is shorter inline for most cases */
	if (size == 1 && (!optsize || v >= LWDIRECT)) {
		if (to_de)
			printf("\tex de,hl\n");
		printf("\tld hl,0x%x\n\tadd hl,sp\n\tld l,(hl)\n", v);
		if (to_de)
			printf("\tex de,hl\n");
		return 1;
	}
	/* Word load is long winded on Z80 */
	if (size == 2 && opt > 2) {
		printf("\tld hl,0x%x\n\tadd hl,sp\n", WORD(v));
		if (IS_RABBIT)
			printf("\tld hl,(hl + %u\n)\n", 0);
		else if (IS_EZ80 && to_de)
			printf("\tld de,(hl)\n");
		else if (to_de)
			printf("\tld e,(hl)\n\tinc hl\n\tld d,(hl)\n");
		else
			printf("\tld a,(hl)\n\tinc hl\n\tld h,(hl)\n\tld l,a\n");
		return 1;
	}
	/* Via helper magic for compactness on Z80 */
	if (size == 1)
		name = "ldbyte";
	else
		name = "ldword";
	/* Check for lref out of range. Our long form helpers currently
	   trash DE so can't be used when we are loading DE */
	if (to_de && v >= 253)
		error("lrr");
	/* We do a call so the stack offset is two bigger */
	if (to_de)
		printf("\tex de,hl\n");

	if (v < LWDIRECT)
		printf("\tcall __%s%u\n", name, v + 2);
	else if (v < 253)
		printf("\tcall __%s\n\t.byte %u\n", name, v + 2);
	else
		printf("\tcall __%sw\n\t.word %u\n", name, v + 2);

	if (to_de)
		printf("\tex de,hl\n");

	return 1;
}

/* Get an lref value into A without destroying HL. DE is fair game. This one
   is allowed to fail */
unsigned generate_lref_a(unsigned v)
{
	/* Correct for current SP location */
	v += sp;

	/* Sadly pop af gets us the wrong byte and sneakily adjusting
	   sp kills us in an interrupt handler. This is is still cheaper
	   than messing around */
	if (v == 0) {
		printf("\tpop de\n\tpush de\n");
		printf("\tld a,e\n");
		return 1;
	}
	/* Offset 1 does work however, although we almost never get that */
	if (v == 1) {
		printf("\tpop af\n\tpush af\n");
		return 1;
	}
	if (use_fp) {
		printf("\tld a,(iy + %d)\n", v - sp);
		return 1;
	}
	/* Byte load and inlien are about the same size so inline for
	   speed */
	printf("\tex de,hl\n");
	printf("\tld hl,0x%x\n\tadd hl,sp\n\tld a,(hl)\n", v);
	printf("\tex de,hl\n");
	return 1;
}

/*
 *	Return 1 if the node can be turned into direct access. The VOID check
 *	is a special case we need to handle stack clean up of void functions.
 *
 *	Note that we think of RDEREF as accessible. There is a specific case
 *	it is not which is loading IX or IY indirect from a pointer. The
 *	register callers must handle the fact this returns 1 and load_r_with
 *	then says 0
 */

static unsigned access_direct(struct node *n)
{
	unsigned op = n->op;

	/* We can direct access integer or smaller types that are constants
	   global/static or string labels */

	 /* For now we only do word sized lrefs FIXME */
	if (op == T_LREF && get_size(n->type) == 2 && n->value + sp < 253)
		return 1;
	if (op != T_CONSTANT && op != T_NAME && op != T_LABEL && op != T_NREF && op != T_LBREF && op != T_REG && op != T_RREF && op != T_RDEREF)
		return 0;
	if (!PTR(n->type) && (n->type & ~UNSIGNED) > CSHORT)
		return 0;
	return 1;
}

/*
 *	Get something that passed the access_direct check into de. Could
 *	we merge this with the similar hl one in the main table ?
 */

static unsigned load_r_with(const char *rp, struct node *n)
{
	unsigned v = WORD(n->value);

	char r = *rp;

	switch(n->op) {
	case T_NAME:
		printf("\tld %s,_%s+%u\n", rp, namestr(n->snum), v);
		return 1;
	case T_LABEL:
		printf("\tld %s,T%u+%u\n", rp, n->val2, v);
		return 1;
	case T_CONSTANT:
		/* We know this is not a long from the checks above */
		printf("\tld %s,0x%x\n", rp, v);
		return 1;
	case T_NREF:
		/* We know it is int or pointer */
		printf("\tld %s,(_%s+%u)\n", rp, namestr(n->snum), v);
		return 1;
		break;
	/* TODO: fold together cleanly with NREF */
	case T_LBREF:
		printf("\tld %s,(T%u+%u)\n", rp, n->val2, v);
		return 1;
	case T_RREF:
		/* Assumes that BC isn't corrupted yet so is already the right value. Use
		   this quirk with care */
		if (n->value == 1 && r != 'i') {
			if (r == 'd')
				printf("\tld d,b\n\tld e,c\n");
			else
				printf("\tld h,b\n\tld l,c\n");
			return 1;
		}
		printf("\tpush %s\n\tpop %s\n", regnames[n->value], rp);
		return 1;
	case T_RDEREF:
		/* One oddity here - we can't load IX or IY from (ix) or (iy) */
		if (*rp == 'i')
			return 0;
		printf("\tld %c,(%s + %u)\n", rp[1], regnames[n->value], n->val2);
		printf("\tld %c,(%s + %u)\n", *rp, regnames[n->value], n->val2 + 1);
		return 1;
	default:
		return 0;
	}
	return 1;
}

static unsigned load_de_with(struct node *n)
{
	/* DE we can lref because our helpers don't touch DE so we can
	   ex de,hl around it */
	if (n->op == T_LREF)
		return generate_lref(n->value, 2, 1);
	return load_r_with("de", n);
}

static unsigned load_hl_with(struct node *n)
{
	/* An HL lref is fine as we can trash HL and A to get HL */
	if (n->op == T_LREF)
		return generate_lref(n->value, 2, 0);
	return load_r_with("hl", n);
}

static unsigned load_a_with(struct node *n)
{
	switch(n->op) {
	case T_CONSTANT:
		/* We know this is not a long from the checks above */
		printf("\tld a,0x%x\n", BYTE(n->value));
		break;
	case T_NREF:
		printf("\tld a,(_%s+%u)\n", namestr(n->snum), WORD(n->value));
		break;
	case T_LBREF:
		printf("\tld a,(T%u+%u)\n", n->val2, WORD(n->value));
		break;
	case T_RREF:
		/* TODO: ix/iy (can they happen ? */
		if (n->value == 1) {
			printf("\tld a,c\n");
			break;
		}
		return 0;
	case T_RDEREF:
		if (n->value == 1) {
			printf("\tld a,(bc)\n");
			break;
		}
		printf("\tld a,(%s+%u)\n", regnames[n->value], n->val2);
		break;
	case T_LREF:
		return generate_lref_a(n->value);
	default:
		return 0;
	}
	return 1;
}

static void repeated_op(const char *o, unsigned n)
{
	while(n--)
		printf("\t%s\n", o);
}

static void get_regvar(unsigned reg, struct node *n, unsigned s)
{
	if (n && (n->flags & NORETURN))
		return;
	if (reg == 1) {
		printf("\tld l,c\n");
		if (s == 2)
			printf("\tld h,b\n");
		return;
	}
	printf("\tpush %s\n\tpop hl\n", regnames[reg]);
}

static void load_regvar(unsigned r, unsigned s)
{
	if (r == 1) {
		printf("\tld c,l\n");
		if (s == 2)
			printf("\tld b,h\n");
		return;
	}
	printf("\tpush hl\n\tpop %s\n", regnames[r]);
}

/* We use "DE" as a name but A as register for 8bit ops... probably ought to rework one day */
static unsigned gen_deop(const char *op, struct node *n, struct node *r, unsigned sign)
{
	unsigned s = get_size(n->type);
	if (s > 2)
		return 0;
	/* Generate ld e, forms of de helpers if the value is below 256 as
	   is often the case */
	if (optsize && s == 2 && r->op == T_CONSTANT) {
		unsigned v= r->value;
		if (v < 256) {
			printf("\tld e,%u\n", v);
			printf("\tcall __%s", op);
			if (sign)
				helper_type(n->type, sign);
			printf("0d\n");
			return 1;
		}
	}
	if (s == 2) {
		if (load_de_with(r) == 0)
			return 0;
	} else {
		if (load_a_with(r) == 0)
			return 0;
	}
	if (sign)
		helper_s(n, op);
	else
		helper(n, op);
	return 1;
}

static unsigned gen_compc(const char *op, struct node *n, struct node *r, unsigned sign)
{
	unsigned s = get_size(n->type);
	/* TODO: Z280 has CPW HL,DE CPW HL, const */
	if (r->op == T_CONSTANT) {
		/* Some minimal cases to work out how best to use CCONLY */
		if (n->op == T_BANGEQ && (n->flags & CCONLY)) {
			if (r->value == 0) {
				if (s == 1) {
					printf("\txor a\n\tcp l\n");
					n->flags |= USECC;
					return 1;
				}
				if (s == 2) {
					printf("\tld a,h\n\tor l\n");
					n->flags |= USECC;
					return 1;
				}
			}
			if (s == 1 && r->value == 255) {
				printf("\tinc l\n");
				n->flags |= USECC;
				return 1;
			}
			if (s == 2 && r->value == 0xFFFF) {
				printf("\tld a,h\n\tand l\n\tinc a\n");
				n->flags |= USECC;
				return 1;
			}
			if (s == 2 && r->value <= 0xFF) {
				/* The xor is a compare resulting in 0 if equal,
				   and the or h then checks the high byte matches */
				printf("\tld a,0x%x\n", (unsigned)r->value);
				printf("\txor l\n");
				printf("\tor h\n");
				n->flags |= USECC;
				return 1;
			}
			if (s == 2 && (r->value & 0xFF) == 0) {
				printf("\tld a,0x%x\n", (unsigned)r->value >> 8);
				printf("\txor h\n");
				printf("\tor l\n");
				n->flags |= USECC;
				return 1;
			}
			if (s == 1) {
				printf("\tld a,0x%x\n", (unsigned)r->value & 0xFF);
				printf("\tcp l\n");
				return 1;
			}
		}
		if (n->op == T_EQEQ && (n->flags & CCONLY) && !(n->flags & CCFIXED)) {
			if (r->value == 0) {
				if (s == 1) {
					printf("\txor a\n\tcp l\n");
					ccflags = ccinvert;
					n->flags |= USECC;
					return 1;
				}
				if (s == 2) {
					printf("\tld a,h\n\tor l\n");
					ccflags = ccinvert;
					n->flags |= USECC;
					return 1;
				}
			}
			if (r->value == 255 && s == 1) {
				printf("\tinc l\n");
				ccflags = ccinvert;
				n->flags |= USECC;
				return 1;
			}
			if (r->value == 0xFFFF && s == 2) {
				printf("\tld a,h\n\tand l\n\tinc a\n");
				ccflags = ccinvert;
				n->flags |= USECC;
				return 1;
			}
			if (s == 2 && r->value <= 0xFF) {
				/* The xor is a compare resulting in 0 if equal,
				   and the or h then checks the high byte matches */
				printf("\tld a,0x%x\n", (unsigned)r->value);
				printf("\txor l\n");
				printf("\tor h\n");
				ccflags = ccinvert;
				n->flags |= USECC;
				return 1;
			}
			if (s == 2 && (r->value & 0xFF) == 0) {
				printf("\tld a,0x%x\n", (unsigned)r->value >> 8);
				printf("\txor h\n");
				printf("\tor l\n");
				ccflags = ccinvert;
				printf(";EQEQ z true z nz\n");
				n->flags |= USECC;
				return 1;
			}
			if (s == 1) {
				printf("\tld a,0x%x\n", (unsigned)r->value & 0xFF);
				printf("\tcp l\n");
				ccflags = ccinvert;
				n->flags |= USECC;
				return 1;
			}
		}
		/* TODO : float helper */
		if (r->value == 0 && r->type != FLOAT) {
			char buf[10];
			strcpy(buf, op);
			strcat(buf, "0");
			if (sign)
				helper_s(n, buf);
			else
				helper(n, buf);
			n->flags |= ISBOOL;
			return 1;
		}
	}
	/* Generate short cases with BC */
	if (r->op == T_RREF && s == 2 && r->value == 1 && (n->flags & CCONLY)) {
		if (n->op == T_BANGEQ) {
			printf("\tor a\n\tsbc hl,bc\n");
			n->flags |= USECC;
			return 1;
		}
		if (n->op == T_EQEQ && !(n->flags & CCFIXED)) {
			printf("\tor a\n\tsbc hl,bc\n");
			ccflags = ccinvert;
			n->flags |= USECC;
			return 1;
		}
	}
	/* We need to set a variable to the type of branch to use for the condition and then pick it up
	   (once) in the following branch instruction to do more than the basics */
	if (n->op == T_BANGEQ && (n->flags & CCONLY)) {
		if (s == 1 && load_a_with(r) == 1) {
			printf("\tcp l\n");
			n->flags |= USECC;
			return 1;
		}
		if (s == 2 && load_de_with(r) == 1) {
			printf("\tor a\n\tsbc hl,de\n");
			n->flags |= USECC;
			return 1;
		}
	}
	if (n->op == T_EQEQ && (n->flags & CCONLY) && !(n->flags & CCFIXED)) {
		if (s == 1 && load_a_with(r) == 1) {
			printf("\tcp l\n");
			ccflags = ccinvert;
			n->flags |= USECC;
			return 1;
		}
		if (s == 2 && load_de_with(r) == 1) {
			printf("\tor a\n\tsbc hl,de\n");
			ccflags = ccinvert;
			n->flags |= USECC;
			return 1;
		}
	}
	/* TODO: unsigned gt and lt etc including moving const by one to
	   get easy to inline form */
	if (gen_deop(op, n, r, sign)) {
		n->flags |= ISBOOL;
		return 1;
	}
	return 0;
}

static int count_mul_cost(unsigned n)
{
	int cost = 0;
	if ((n & 0xFF) == 0) {
		n >>= 8;
		cost += 3;		/* mov mvi */
	}
	while(n > 1) {
		if (n & 1)
			cost += 3;	/* push pop add hl,de */
		n >>= 1;
		cost++;			/* dad h */
	}
	return cost;
}

/* Write the multiply for any value > 0 */
static void write_mul(unsigned n)
{
	unsigned pops = 0;
	if ((n & 0xFF) == 0) {
		printf("\tld h,l\n\tld l,0\n");
		n >>= 8;
	}
	while(n > 1) {
		if (n & 1) {
			pops++;
			printf("\tpush hl\n");
		}
		printf("\tadd hl,hl\n");
		n >>= 1;
	}
	while(pops--) {
		printf("\tpop de\n\tadd hl,de\n");
	}
}

static unsigned can_fast_mul(unsigned s, unsigned n)
{
	/* Pulled out of my hat 8) */
	unsigned cost = 15 + 3 * opt;
	if (s > 2)
		return 0;

	/* The base cost of the helper is 6 lxi de, n; call, but this may be too aggressive
	   given the cost of mulde TODO */
	if (optsize)
		cost = 10;
	if (n == 0 || count_mul_cost(n) <= cost)
		return 1;
	return 0;
}

static void gen_fast_mul(unsigned s, unsigned n)
{

	if (n == 0)
		printf("\tld hl,0x0\n");
	else
		write_mul(n);
}

static unsigned gen_fast_div(unsigned n, unsigned s, unsigned u)
{
	u &= UNSIGNED;
	if (s != 2)
		return 0;
	if (n == 1)
		return 1;
	if (n == 256 && u) {
		printf("\tld l,h\n\tld h,0x0\n");
		return 1;
	}
	if (n & (n - 1))
		return 0;


	if (u) {
		while(n > 1) {
			printf("\tsrl h\n\trr l\n");
			n >>= 1;
		}
	} else {
		while(n > 1) {
			printf("\tsra h\n\trr l\n");
			n >>= 1;
		}
	}
	return 1;
}


/* TODO : we could in theory optimize xor 255 with cpl ? */
static unsigned gen_logicc(struct node *n, unsigned s, const char *op, unsigned v, unsigned code)
{
	unsigned h = (v >> 8) & 0xFF;
	unsigned l = v & 0xFF;

	/* Rabbit has 16bit logic operators */
	if (s == 2 && IS_RABBIT) {
		if (load_de_with(n) == 0)
			return 0;
		printf("\t%s hl,de\n", op);
		return 1;
	}

	if (s > 2 || (n && n->op != T_CONSTANT))
		return 0;

	/* If we are trying to be compact only inline the short ones */
	if (optsize && ((h != 0 && h != 255) || (l != 0 && l != 255)))
		return 0;

	if (s == 2) {
		if (h == 0) {
			if (code == 1)
				printf("\tld h,0x0\n");
		}
		else if (h == 255 && code != 3) {
			if (code == 2)
				printf("\tld h,0xff\n");
		} else {
			printf("\tld a,h\n\t%s 0x%x\n\tld h,a\n", op, h);
		}
	}
	if (l == 0) {
		if (code == 1)
			printf("\tld l,0x0\n");
	} else if (l == 255 && code != 3) {
		if (code == 2)
			printf("\tld l,0xff\n");
	} else {
		printf("\tld a,l\n\t%s 0x%x\n\tld l,a\n", op, l);
	}
	return 1;
}

static unsigned gen_fast_remainder(unsigned n, unsigned s)
{
	unsigned mask;
	if (s != 2)
		return 0;
	if (n == 1) {
		printf("\tld hl,0x00\n");
		return 1;
	}
	if (n == 256) {
		printf("\tld h,0x0\n");
		return 1;
	}
	if (n & (n - 1))
		return 0;
	if (!optsize) {
		mask = n - 1;
		gen_logicc(NULL, s, "and", mask, 1);
		return 1;
	}
	return 0;
}

/*
 *	If possible turn this node into a direct access. We've already checked
 *	that the right hand side is suitable. If this returns 0 it will instead
 *	fall back to doing it stack based.
 *
 *	The 8080 is pretty basic so there isn't a lot we turn around here. As
 *	proof of concept we deal with the add case. Other processors may be
 *	able to handle a lot more.
 *
 *	If your processor is good at subtracts you may also want to rewrite
 *	constant on the left subtracts in the rewrite rules into some kind of
 *	rsub operator.
 */
unsigned gen_direct(struct node *n)
{
	unsigned s = get_size(n->type);
	struct node *r = n->right;
	unsigned v;
	unsigned nr = n->flags & NORETURN;

	/* We only deal with simple cases for now */
	if (r) {
		if (!access_direct(n->right))
			return 0;
		v = r->value;
	}

	switch (n->op) {
	case T_CLEANUP:
		gen_cleanup(v);
		return 1;
	case T_NSTORE:
		if (s > 2)
			return 0;
		if (s == 1)
			printf("\tld a,l\n");
		printf("\tld (_%s+%u),", namestr(n->snum), WORD(n->value));
			return 1;
		if (s == 1)
			printf("a\n");
		else
			printf("hl\n");
		/* TODO 4/8 for long etc */
		return 0;
	case T_LBSTORE:
		if (s > 2)
			return 0;
		if (s == 1)
			printf("\tld a,l\n");
		printf("\tld (T%u+%u), ", n->val2, v);
		if (s == 1)
			printf("a\n");
		else
			printf("hl\n");
		return 1;
	case T_RSTORE:
		load_regvar(n->value, s);
		return 1;
	case T_EQ:
		/* The address is in HL at this point */
		if (IS_EZ80 && s == 2) {
			if (load_de_with(r) == 0)
				return 0;
			printf("\tld (hl),de\n");
			if (!nr)
				printf("\tex de,hl\n");
			return 1;
		}
		if (s == 1) {
			/* We need to end up with the value in l if this is not NORETURN, also
			   we can optimize constant a step more */
			if (r->op == T_CONSTANT && nr)
				printf("\tld (hl),0x%x\n", v & 0xFF);
			else {
				if (load_a_with(r) == 0)
					return 0;
				printf("\tld (hl),a\n");
				if (!nr)
					printf("\tld l,a\n");
			}
			return 1;
		}
		if (s == 2 && r->op == T_CONSTANT) {
			if (nr)	{ /* usual case */
				printf("\tld (hl),0x%x\n", v & 0xFF);
				printf("\tinc hl\n");
				printf("\tld (hl),0x%x\n", v >> 8);
			} else {
				printf("\tld de,0x%x\n", v);
				printf("\tld (hl),e\n");
				printf("\tinc hl\n");
				printf("\tld (hl),d\n");
				printf("\tex de,hl\n");
			}
			return 1;
		}
		if (s == 4 && r->op == T_CONSTANT) {
			if (v < 0x02) {
				printf("\tcall __assign%ul\n", v);
				return 1;
			}
			if (v <= 0xFF) {
				printf("\tld a,0x%x\n", v);
				helper(n, "assign0la");
				return 1;
			}
			if (v <= 0xFFFF) {
				printf("\tld de,%u\n", v);
				helper(n, "assignl0de");
				return 1;
			}
		}
		return 0;
	case T_PLUS:
		/* Zero should be eliminated in cc1 FIXME */
		if (r->op == T_CONSTANT) {
			if (v == 0)
				return 1;
			if (v < 4 && s <= 2) {
				if (s == 1)
					repeated_op("inc l", v);
				else
					repeated_op("inc hl", v);
				return 1;
			}
		}
		/* Faster case for foo + bc to avoid transfer to de */
		if (r->op == T_RREF && v == 1) {
			printf("\tadd hl,bc\n");
			return 1;
		}
		if (s <= 2) {
			/* LHS is in HL at the moment, end up with the result in HL */
			if (s == 1) {
				if (load_a_with(r) == 0)
					return 0;
				printf("\tld e,a\n");
			}
			if (s > 2 || load_de_with(r) == 0)
				return 0;
			printf("\tadd hl,de\n");
			return 1;
		}
		return 0;
	case T_MINUS:
		if (r->op == T_CONSTANT) {
			if (v == 0)
				return 1;
			if (v < 6 && s <= 2) {
				if (s == 1)
					repeated_op("dec l", v);
				else
					repeated_op("dec hl", v);
				return 1;
			}
			printf("\tld de,0x%x\n", 65536 - v);
			printf("\tadd hl,de\n");
			return 1;
		}
		/* Avoid loading BC into DE unnecessarily. No shortcut for IXY though */
		if (r->op == T_REG && v == 1) {
			printf("\tor a\n\tsbc hl,bc\n");
			return 1;
		}
		/* TODO: Can we ex de,hl, load into hl and go? - check direction too */
		if (load_de_with(r) == 0)
			return 0;
		printf("\tor a\n\tsbc hl,de\n");
		return 1;
	case T_STAR:
		if (r->op == T_CONSTANT) {
			if (s <= 2 && can_fast_mul(s, v)) {
				gen_fast_mul(s, v);
				return 1;
			}
		}
		return gen_deop("mulde", n, r, 0);
	case T_SLASH:
		if (r->op == T_CONSTANT) {
			if (s <= 2 && gen_fast_div(s, v, n->type))
				return 1;
		}
		return gen_deop("divde", n, r, 1);
	case T_PERCENT:
		if (r->op == T_CONSTANT && (n->type & UNSIGNED)) {
			if (s <= 2 && gen_fast_remainder(s, v))
				return 1;
		}
		return gen_deop("remde", n, r, 1);
	case T_AND:
		if (gen_logicc(r, s, "and", v, 1))
			return 1;
		if (r->op == T_CONSTANT && s <= 2) {
			int b = bitcheck0(v, s);
			if (b >= 0) {
				/* Single clear bit */
				if (b < 8)
					printf("\tres %u,l\n", b);
				else
					printf("\tres %u,h\n", b - 8);
				return 1;
			}
		}
		return gen_deop("bandde", n, r, 0);
	case T_OR:
		if (r->op == T_CONSTANT && v == 0)
			return 1;
		if (r->op == T_CONSTANT && s <= 2) {
			int b = bitcheck1(v, s);
			if (b >= 0) {
				/* Single set bit */
				if (b < 8)
					printf("\tset %u,l\n", b);
				else
					printf("\tset %u,h\n", b - 8);
				return 1;
			}
			/* For small values it's more efficient to do this inline even
			   in -Os */
			if (!(v & 0xFF00)) {
				printf("\tld a,0x%x\n\tor l\n\tld l,a\n", v);
				return 1;
			}
			if (!(v & 0x00FF)) {
				printf("\tld a,0x%x\n\tor h\n\tld h,a\n", v >> 8);
				return 1;
			}
		}
		if (gen_logicc(r, s, "or", r->value, 2))
			return 1;
		return gen_deop("borde", n, r, 0);
	case T_HAT:
		/* For small values it's more efficient to do this inline even
		   in -Os */
		if (r->op == T_CONSTANT && s <= 2) {
			if (v == 0)
				return 1;
			if (!(v & 0xFF00)) {
				printf("\tld a,0x%x\n\txor l\n\tld l,a\n", v);
				return 1;
			}
			if (!(v & 0x00FF)) {
				printf("\tld a,0x%x\n\txor h\n\tld h,a\n", v >> 8);
				return 1;
			}
		}
		if (gen_logicc(r, s, "xor", r->value, 3))
			return 1;
		return gen_deop("bxorde", n, r, 0);
	/* TODO: add sbc hl,de etc versions of these when we can - or in optimizer ? */
	case T_EQEQ:
		return gen_compc("cmpeq", n, r, 0);
	case T_GTEQ:
		return gen_compc("cmpgteq", n, r, 1);
	case T_GT:
		return gen_compc("cmpgt", n, r, 1);
	case T_LTEQ:
		return gen_compc("cmplteq", n, r, 1);
	case T_LT:
		return gen_compc("cmplt", n, r, 1);
	case T_BANGEQ:
		return gen_compc("cmpne", n, r, 0);
	case T_LTLT:
		/* TODO: Z80N has some shift helpers */
		if (s <= 2 && r->op == T_CONSTANT && v <= 8) {
			if (r->value < 8)
				repeated_op("add hl,hl", v);
			else
				printf("\tld h,l\n\tld l,0x0\n");
			return 1;
		}
		return gen_deop("shlde", n, r, 0);
	case T_GTGT:
		/* >> by 8 unsigned */
		if (s == 2 && (n->type & UNSIGNED) && r->op == T_CONSTANT && r->value == 8) {
			printf("\tld l,h\n\tld h,0x0\n");
			return 1;
		}
		return gen_deop("shrde", n, r, 1);
	/* Shorten post inc/dec if result not needed - in which case it's the same as
	   pre inc/dec */
	case T_PLUSPLUS:
		/* This occurs a lot with pointers */
		if (s == 2 && r->op == T_CONSTANT) {
			if (v <= 4) {
				printf("\tcall __postinc%u\n", v);
				return 1;
			}
			if (v <= 255) {
				printf("\tld e,0x%x\n", v);
				printf("\tcall __postince\n");
				return 1;
			}
			printf("\tld de,0x%x\n", v);
			printf("\tcall __postincde\n");
			return 1;
		}
		if (!(n->flags & NORETURN))
			return 0;
	case T_PLUSEQ:
		if (s == 1) {
			if (r->op == T_CONSTANT && v < 4 && (n->flags & NORETURN)) {
				repeated_op("inc (hl)", v);
			} else {
				if (load_a_with(r) == 0)
					return 0;
				printf("\tadd a,(hl)\n\tld (hl),a\n");
				if (!(n->flags & NORETURN))
					printf("\tld l,a\n");
			}
			return 1;
		}
		/* These are very common */
		if (s == 2 && r->op == T_CONSTANT) {
			if (v <= 4) {
				printf("\tcall __pluseq%u\n", v);
				return 1;
			}
			if (v < 256) {
				printf("\tld e,%u\n", v);
				printf("\tcall __pluseqe\n");
				return 1;
			}
			/* High only */
			if ((v & 0x00FF) == 0) {
				printf("\tinc hl\n");
				if (v < (4 << 8))
					repeated_op("inc (hl)", v >> 8);
				else {
					printf("\tld a,0x%x\n", v >> 8);
					printf("\tadd a,(hl)\n");
					printf("\tld (hl),a\n");
				}
				return 1;
			}
		}
		return gen_deop("pluseqde", n, r, 0);
	case T_MINUSMINUS:
		/* This occurs a lot with pointers */
		if (s == 2 && r->op == T_CONSTANT) {
			if (v <= 4) {
				printf("\tcall __postdec%u\n", v);
				return 1;
			}
			if (v <= 255) {
				printf("\tld e,0x%x\n", v);
				printf("\tcall __postdece\n");
				return 1;
			}
			printf("\tld de,0x%x\n", v);
			printf("\tcall __postdecde\n");
			return 1;
		}
		if (!(n->flags & NORETURN))
			return 0;
	case T_MINUSEQ:
		if (s == 1) {
			/* Shortcut for small 8bit values */
			if (r->op == T_CONSTANT && r->value < 4 && (n->flags & NORETURN)) {
				repeated_op("dec (hl)", r->value);
			} else {
				/* Subtraction is not transitive so this is
				   messier */
				if (r->op == T_CONSTANT) {
					if (r->value == 1)
						printf("\tld a,(hl)\n\tdec a\n\tld (hl),a\n");
					else
						printf("\tld a,(hl)\n\tsub 0x%x\n\tld (hl),a",
							(int)r->value);
				} else {
					if (load_a_with(r) == 0)
						return 0;
					printf("\tcpl\n\tinc a\n\n");
					printf("\tsub (hl)\n\tld (hl),a\n");
				}
				if (!(n->flags & NORETURN))
					printf("\tld l,a\n");
			}
			return 1;
		}
		/* These are very common */
		if (s == 2 && r->op == T_CONSTANT) {
			if (v <= 4) {
				printf("\tcall __minuseq%u\n", v);
				return 1;
			}
			if (v < 256) {
				printf("\tld e,%u\n", v);
				printf("\tcall __minuseqe\n");
				return 1;
			}
		}
		return gen_deop("minuseqde", n, r, 0);
	case T_ANDEQ:
		if (s == 1) {
			if (load_a_with(r) == 0)
				return 0;
			printf("\tand (hl)\n\tld (hl),a\n");
			if (!(n->flags & NORETURN))
				printf("\tld l,a\n");
			return 1;
		}
		return gen_deop("andeqde", n, r, 0);
	case T_OREQ:
		if (s == 1) {
			if (load_a_with(r) == 0)
				return 0;
			printf("\tor (hl)\n\tld (hl),a\n");
			if (!(n->flags & NORETURN))
				printf("\tld l,a\n");
			return 1;
		}
		return gen_deop("oreqde", n, r, 0);
	case T_HATEQ:
		if (s == 1) {
			if (load_a_with(r) == 0)
				return 0;
			printf("\txor (hl)\n\tld (hl),a\n");
			if (!(n->flags & NORETURN))
				printf("\tld l,a\n");
			return 1;
		}
		return gen_deop("xoreqde", n, r, 0);
	}
	return 0;
}

/*
 *	Allow the code generator to shortcut the generation of the argument
 *	of a single argument operator (for example to shortcut constant cases
 *	or simple name loads that can be done better directly)
 */
unsigned gen_uni_direct(struct node *n)
{
	struct node *r = n->right;
	unsigned v;
	/* There are some uni operations on registers we can do
	   non destructively but directly on BC */
	if (r && r->op == T_RREF && r->value == 1) {
		if (n->op == T_BTST) {
			/* Bit test on BC - always CCONLY */
			v = n->value;
			if (v < 8)
				printf("\tbit %u,c\n", v);
			else
				printf("\tbit %u,b\n", v - 8);
		}
	}
	return 0;
}

static unsigned reg_canincdec(unsigned reg, struct node *n, unsigned s, int v)
{
	/* We only deal with short and char for register */
	if (reg > 1 && s != 2)
		return 0;
	/* Is the shortcut worth it ? */
	if (n->op != T_CONSTANT || v > 8 || v < -8)
		return 0;
	return 1;
}

static unsigned reg_incdec(unsigned reg, unsigned s, int v)
{
	char buf[16];
	char *op = "inc";
	if (v < 0) {
		op = "dec";
		v = -v;
	}
	if (s == 1)
		sprintf(buf, "%s %c", op, regnames[reg][1]);
	else
		sprintf(buf, "%s %s", op, regnames[reg]);

	repeated_op(buf, v);
	return 1;
}

static void reghelper_s(struct node *n, const char *p)
{
	static char x[16];
	strcpy(x, p);
	switch(n->left->value) {
	case 2:
		*x = 'i';
		x[1] = 'x';
		break;
	case 3:
		*x = 'i';
		x[1] = 'y';
		break;
	}
	helper_s(n, p);
}

static void reghelper(struct node *n, const char *p)
{
	static char x[16];
	strcpy(x, p);
	switch(n->left->value) {
	case 2:
		*x = 'i';
		x[1] = 'x';
		break;
	case 3:
		*x = 'i';
		x[1] = 'y';
		break;
	}
	helper(n, p);
}

static void reg_logic(struct node *n, unsigned s, unsigned op, const char *i)
{
	/* TODO : constant optimization */
	codegen_lr(n->right);
	/* HL is now the value to combine with BC */
	if (opt > 1 && n->left->value == 1) {
		/* TODO - can avoid the reload into HL if NORETURN */
		/* TODO: check we never end up with IX or IY here */
		if (s == 2)
			printf("\tld a,b\n\t%s h\n\tld b,a\nld h,a\n", i + 2);
		printf("\tld a,c\n\t%s c\n\tld c,a\nld l,a\n", i + 2);
	} else {
		reghelper(n, i);
		get_regvar(n->left->value, NULL, s);
	}
}

/*
 *	Allow the code generator to short cut any subtrees it can directly
 *	generate. Also our point to do any private tree mods downwards
 */
unsigned gen_shortcut(struct node *n)
{
	unsigned s = get_size(n->type);
	unsigned v;
 	struct node *l = n->left;
 	struct node *r = n->right;
	unsigned nr = n->flags & NORETURN;

	/* Don't generate unreachable blocks */
	if (unreachable)
		return 1;

	/* Try and rewrite this node subtree for CC only */
	if ((opt || optsize) && (n->flags & CCONLY))
		propogate_cconly(n);

	/* The comma operator discards the result of the left side, then
	   evaluates the right. Avoid pushing/popping and generating stuff
	   that is surplus */
	if (n->op == T_COMMA) {
		l->flags |= NORETURN;
		codegen_lr(l);
 		/* Parent determines child node requirements */
		codegen_lr(r);
		r->flags |= nr;
		return 1;
	}
	if (n->op == T_BOOL) {
		/* Logic via jumping always uses CC in our case */
		if (r->op == T_ANDAND || r->op == T_OROR)
			r->flags |= USECC;
		codegen_lr(r);
		/* Condition code only doesn't need us to do anything if
		   our child is already CCONLY */
/*		printf("; N %x R %x NF %x RF %x\n",
			n->op, r->op, n->flags, r->flags); */
		/* If we want CC flags and the subtree is CC flags do nothing */
		if ((n->flags & CCONLY) && (r->flags & USECC)) {
			n->flags |= USECC;
			return 1;
		}
		if (r->flags & USECC) {
			/* Should never happen */
			printf("\t;BOTCH %04X:%04X\n", r->op, r->flags);
			printf("\tcall __cctobool\n");
		}
		/* If the result is bool do nothing */
		if (r->flags & ISBOOL)
			return 1;
		s = get_size(r->type);
		if (s <= 2 && (n->flags & CCONLY)) {
			if (s == 2)
				printf("\tld a,h\n\tor l\n");
			else
				printf("\tld a,l\n\tor a\n");
			return 1;
		}
		if (IS_RABBIT && s <= 2) {
			/* quick way to bool byte */
			if (s == 1)
				printf("\tld h,l\n");
			printf("\tbool hl\n");
		} else
			/* Too big or value needed */
			helper(n, "bool");
		n->flags |= ISBOOL;
		return 1;
	}
	/* Re-order assignments we can do the simple way */
	/* TODO: LBSTORE */
	if (n->op == T_NSTORE && s <= 2) {
		/* Handle const nr specially */
		if (s == 1 && r->op == T_CONSTANT && (n->flags & NORETURN)) {
			printf("\tld a,0x%x\n", (uint8_t)r->value);
		} else {
			codegen_lr(n->right);
			if (s == 1)
				printf("\tld a,l\n");
		}
		/* Expression result is now in HL or A or both as needed */
		if (s == 1)
			printf("\tld (_%s+%u), a\n", namestr(n->snum), WORD(n->value));
		else
			printf("\tld (_%s+%u), hl\n", namestr(n->snum), WORD(n->value));
		return 1;
	}
	/* Locals we can do on some later processors, Z80 is doable but messy - so not worth it */
	if (n->op == T_LSTORE && s <= 2) {
		v = n->value;
		if (use_fp && v < 128 - s && r->op == T_CONSTANT && (n->flags & NORETURN)) {
			/* Constant assign odd case */
			printf("\tld (iy + %u), %d\n",v ,
				((unsigned)r->value) & 0xFF);
			if (s == 2)
				printf("\tld (iy + %u), %d\n", v + 1,
					((unsigned)r->value) >> 8);
			return 1;
		}
		/* Rabbit and Z280 */
		if (HAS_LDHLSP && v + sp < 255 && s == 2) {
			codegen_lr(n->right);
			printf("\tld (sp + %u),hl\n", v + sp);
			return 1;
		}
		if (use_fp && v <= 128 - s) {
			codegen_lr(n->right);
			printf("\tld (iy + %u), l\n", v);
			if (s == 2)
				printf("\tld (iy + %u), h\n", v + 1);
			return 1;
		}
		/* General case */
		if (v + sp == 0 && s == 2) {
			/* The one case Z80 is worth doing */
			codegen_lr(n->right);
			if (nr)
				printf("\tex (sp),hl\n");
			else
				printf("\tpop af\n\tpush hl\n");
			return 1;
		}
	}
	/* Shortcut any initialization of BC/IX/IY we can do directly */
	if (n->op == T_RSTORE) {
		if (s == 2 && nr)
			return load_r_with(regnames[n->value], r);
		/* Can in theory do byte sized shortcuts but need a suitable
		   helper adding for the few we can - notably c,(ix+n) */
	}

	/* Assignment to *BC is byte pointer always */
	if (n->op == T_REQ) {
		const char *rp = regnames[n->value];
		switch(n->value) {
		case 1:
			/* Try and get the value into A */
			if (!load_a_with(r)) {
				codegen_lr(r);		/* If not then into HL */
				printf("\tld a,l\n");
			}
			printf("\tld (bc),a\n");	/* Do in case volatile */
			if (!nr)
				printf("\tld l,a\n");
			return 1;
		case 2:
		case 3:
			if (s == 1) {
				if (!load_a_with(r)) {
					codegen_lr(r);
					printf("\tld (%s + %u),l\n", rp, n->val2);
					return 1;
				}
				if (*rp == 'i')
					printf("\tld (%s + %u),a\n", rp, n->val2);
				else
					printf("\tld (%s),a\n", rp);
				return 1;
			}
			if (s == 2) {
				if (!load_hl_with(r))
					codegen_lr(r);
				printf("\tld (%s + %u),l\n", rp, n->val2);
				printf("\tld (%s + %u),h\n", rp, n->val2 + 1);
				return 1;
			}
			if (s == 4) {
				/* No fast HL load with a 32bit value */
				codegen_lr(r);
				printf("\tld (%s + %u),l\n", rp, n->val2);
				printf("\tld (%s + %u),h\n", rp, n->val2 + 1);
				printf("\tld de, (__hireg)\n");
				printf("\tld (%s + %u),e\n", rp, n->val2 + 2);
				printf("\tld (%s + %u),d\n", rp, n->val2 + 3);
				return 1;
			}
		}
		return 0;
	}
	/* Special case - we can compare BC with zero efficiently for flags */
	if (l && r && r->op == T_CONSTANT && r->value == 0 &&
		l->op == T_RREF && l->value == 1 && (n->flags & CCONLY)) {
		if (n->op == T_BANGEQ) {
			if (s == 1) {
				printf("\txor a\n\tcp c\n");
				n->flags |= USECC;
				return 1;
			}
			if (s == 2) {
				printf("\tld a,c\n\tor b\n");
				n->flags |= USECC;
				return 1;
			}
		}
		if (n->op == T_EQEQ && !(n->flags & CCFIXED)) {
			if (s == 1) {
				printf("\txor a\n\tcp c\n");
				ccflags = ccinvert;
				n->flags |= USECC;
				return 1;
			}
			if (s == 2) {
				printf("\tld a,c\n\tor b\n");
				ccflags = ccinvert;
				n->flags |= USECC;
				return 1;
			}
		}
		/* TODO: we can also do >= 0 and < 0 (bit 7,b) */
	}
	/* Register targetted ops. These are totally different to the normal EQ ops because
	   we don't have an address we can push and then do a memory op */
	if (l && l->op == T_REG) {
		unsigned reg = l->value;
		v = r->value;
		switch(n->op) {
		case T_PLUSPLUS:
			/* x++ with result discarded can be treated as ++x */
			if (nr) {
				if (reg_canincdec(reg, r, s, v)) {
					reg_incdec(reg, s, v);
					return 1;
				}
			}
			if (!nr) {
				printf("\tpush %s\n", regnames[reg]);
				sp += 2;
			}
			/* Fall through */
		case T_PLUSEQ:
			if (reg_canincdec(reg, r, s, v)) {
				reg_incdec(reg, s, v);
				if (nr)
					return 1;
				if (n->op == T_PLUSEQ) {
					get_regvar(reg, n, s);
				}
			} else {
				/* Amount to add into HL */
				if (reg == 1) {
					codegen_lr(r);
					printf("\tadd hl,bc\n");
					printf("\tld c,l\n");
					if (s == 2)
						printf("\tld b,h\n");
				} else {
					codegen_lr(r);
					printf("\tex de,hl\n\tadd %s,de\n", regnames[reg]);
					if (!(n->flags & NORETURN))
						get_regvar(reg, n, s);
				}
			}
			if (n->op == T_PLUSPLUS && !(n->flags & NORETURN)) {
				printf("\tpop hl\n");
				sp -= 2;
			}
			return 1;
		case T_MINUSMINUS:
			if (!(n->flags & NORETURN)) {
				if (reg_canincdec(reg, r, s, -v)) {
					get_regvar(reg, n, s);
					reg_incdec(reg, s, -v);
					return 1;
				}
				/* Z80 has no 16bit subtract for IX or IT */
				if (reg != 1) {
					if (r->op == T_CONSTANT) {
						printf("\tld de,%u\n", -v);
						printf("\tadd %s,de\n", regnames[reg]);
						return 1;
					}
					codegen_lr(r);
					helper(n, "negate");
					printf("\tex de,hl\n");
					printf("\tadd %s,de\n", regnames[reg]);
					return 1;
				}
				codegen_lr(r);
				if (s == 1) {
					printf("\tld a,c\n\tsub l\n\tld l,c\n\tld c,a\n");
					return 1;
				}
				reghelper(n, "bcsub");
				return 1;
			}
			/* If we don't care about the return they look the same so fall
			   through */
		case T_MINUSEQ:
			if (reg_canincdec(reg, r, s, -v)) {
				reg_incdec(reg, s, -v);
				get_regvar(reg, n, s);
				return 1;
			}
			/* Get the subtraction value into HL */
			codegen_lr(r);
			if (reg != 1) {
				if (r->op == T_CONSTANT) {
					printf("\tlxi d,%u\n", -v);
					printf("\tadd %s,de\n", regnames[reg]);
				} else {
					codegen_lr(r);
					helper(n, "negate");
					printf("\tex de,hl\n");
					printf("\tadd %s,de\n", regnames[reg]);
				}
			} else
				reghelper(n, "bcsub");
			/* Result is only left in reg var reload if needed */
			get_regvar(reg, n, s);
			return 1;
		/* For now - we can do better - maybe just rewrite them into load,
		   op, store ? */
		case T_STAREQ:
			/* TODO: constant multiply */
			if (r->op == T_CONSTANT) {
				if (can_fast_mul(s, v)) {
					get_regvar(reg, NULL, s);
					gen_fast_mul(s, v);
					load_regvar(reg, s);
					return 1;
				}
			}
			codegen_lr(r);
			reghelper(n, "bcmul");
			return 1;
		case T_SLASHEQ:
			/* TODO: power of 2 constant divide maybe ? */
			codegen_lr(r);
			reghelper_s(n, "bcdiv");
			return 1;
		case T_PERCENTEQ:
			/* TODO: spot % 256 case */
			codegen_lr(r);
			reghelper(n, "bcrem");
			return 1;
		case T_SHLEQ:
			/* TODO IX and IY for the simple cases */
			if (r->op == T_CONSTANT && reg == 1) {
				if (s == 1 && v >= 8) {
					printf("\tld c,0x0\n");
					get_regvar(reg, n, s);
					return 1;
				}
				if (s == 1) {
					printf("\tld a,c\n");
					repeated_op("add a,a", v);
					printf("\tld c,a\n");
					get_regvar(reg, n, s);
					return 1;
				}
				/* 16 bit */
				if (v >= 16) {
					printf("\tld bc,0x0\n");
					get_regvar(reg, n, s);
					return 1;
				}
				if (v == 8) {
					printf("\tld b,c\n\tld c,0x0\n");
					get_regvar(reg, n, s);
					return 1;
				}
				if (v > 8) {
					printf("\tld a,c\n");
					repeated_op("add a", v - 8);
					printf("\tld b,a\nld c,0x0\n");
					get_regvar(reg, n, s);
					return 1;
				}
				/* 16bit full shifting */
				get_regvar(reg, NULL, s);
				repeated_op("add hl,hl", v);
				load_regvar(reg, s);
				return 1;
			}
			codegen_lr(r);
			reghelper(n, "bcshl");
			return 1;
		case T_SHREQ:
			if (r->op == T_CONSTANT && reg == 1) {
				if (v >= 8 && s == 1) {
					printf("\tld c,0x0\n");
					get_regvar(reg, n, s);
					return 1;
				}
				if (v >= 16) {
					printf("\tld bc,0x0\n");
					get_regvar(reg, n, s);
					return 1;
				}
				if (v == 8 && (n->type & UNSIGNED)) {
					printf("\tld c,b\nld b,0x0\n");
					get_regvar(reg, n, s);
					return 1;
				}
			}
			codegen_lr(r);
			reghelper_s(n, "bcshr");
			return 1;
		case T_ANDEQ:
			reg_logic(n, s, 0, "bcana");
			return 1;
		case T_OREQ:
			reg_logic(n, s, 1, "bcora");
			return 1;
		case T_HATEQ:
			reg_logic(n, s, 2, "bcxra");
			return 1;
		}
		fprintf(stderr, "unfixed regleft on %04X\n", n->op);
	}
	/* ?? LBSTORE TODO */
	return 0;
}

/* Stack the node which is currently in the working register */
unsigned gen_push(struct node *n)
{
	unsigned size = get_stack_size(n->type);

	/* Our push will put the object on the stack, so account for it */
	sp += size;

	/* Drop a trailing ; in to tell the optimizer HL is now dead. This is safe
	   as we currently operate but that may need review if we get clever */
	switch(size) {
	case 2:
		printf("\tpush hl\n;\n");
		return 1;
	case 4:
		if (optsize)
			printf("\tcall __pushl\n;\n");
		else
			printf("\tex de,hl\n\tld hl,(__hireg)\n\tpush hl\n\tpush de\n;\n");
		return 1;
	default:
		return 0;
	}
}

static unsigned gen_cast(struct node *n)
{
	unsigned lt = n->type;
	unsigned rt = n->right->type;
	unsigned ls;

	if (PTR(rt))
		rt = USHORT;
	if (PTR(lt))
		lt = USHORT;

	/* Floats and stuff handled by helper */
	if (!IS_INTARITH(lt) || !IS_INTARITH(rt))
		return 0;

	ls = get_size(lt);

	/* Size shrink is free */
	if ((lt & ~UNSIGNED) <= (rt & ~UNSIGNED))
		return 1;
	/* Don't do the harder ones */
	if (!(rt & UNSIGNED) || ls > 2)
		return 0;
	printf("\tld h,0x0\n");
	return 1;
}

unsigned gen_node(struct node *n)
{
	unsigned size = get_size(n->type);
	unsigned v;
	char *name;
	unsigned nr = n->flags & NORETURN;
	register struct node *r = n->right;

	/* We adjust sp so track the pre-adjustment one too when we need it */

	v = n->value;

	/* An operation with a left hand node will have the left stacked
	   and the operation will consume it so adjust the stack.

	   The exception to this is comma and the function call nodes
	   as we leave the arguments pushed for the function call */

	if (n->left && n->op != T_ARGCOMMA && n->op != T_CALLNAME && n->op != T_FUNCCALL)
		sp -= get_stack_size(n->left->type);

	switch (n->op) {
		/* Load from a name */
	case T_NREF:
		if (size == 1) {
			printf("\tld a,(_%s+%u)\n", namestr(n->snum), v);
			printf("\tld l,a\n");
			return 1;
		} else if (size == 2) {
			printf("\tld hl,(_%s+%u)\n", namestr(n->snum), v);
			return 1;
		} else if (size == 4) {
			printf("\tld hl,(_%s+%u)\n", namestr(n->snum), v + 2);
			printf("\tld (__hireg),hl\n");
			printf("\tld hl,(_%s+%u)\n", namestr(n->snum), v);
			return 1;
		}
		break;
	case T_LBREF:
		if (size == 1) {
			printf("\tld a,(T%u+%u)\n", n->val2, v);
			printf("\tld l,a\n");
		} else if (size == 2) {
			printf("\tld hl,(T%u+%u)\n", n->val2, v);
			return 1;
		} else if (size == 4) {
			printf("\tld hl, (T%u+%u)\n", n->val2, v + 2);
			printf("\tld (__hireg),hl\n");
			printf("\tld hl,(T%u+%u)\n", n->val2, v);
		} else
			error("lbrb");
		return 1;
	case T_LREF:
		/* We are loading something then not using it, and it's local
		   so can go away */
		if (nr)
			return 1;
/*		printf(";L sp %u spval %u %s(%ld)\n", sp, spval, namestr(n->snum), n->value); */
		if (generate_lref(v, size, 0))
			return 1;
		error("lrb");
		return 0;
	case T_RREF:
		if (nr)
			return 1;
		if (n->value == 1) {
			printf("\tld l,c\n");
			if (size == 2)
				printf("\tld h,b\n");
		} else {
			printf("\tpush %s\n\tpop hl\n", regnames[n->value]);
		}
 		return 1;
	case T_NSTORE:
		if (size == 4) {
			printf("\tld (%s+%u), hl\n", namestr(n->snum), v);
			printf("\tld de,(__hireg)\nld (%s+%u),de\n",
				namestr(n->snum), v + 2);
			return 1;
		}
		if (size > 2)
			return 0;
		if (size == 1)
			printf("\tld a,l\n");
		printf("\tld (_%s+%u),", namestr(n->snum), v);
		if (size == 1)
			printf("a\n");
		else
			printf("hl\n");
		return 1;
	case T_LBSTORE:
		if (size == 4) {
			printf("\tld (T%u+%u),hl\n", n->val2, v);
			printf("\tld de,(__hireg)\n\tld (T%u+%u),de\n",
				n->val2, v + 2);
			return 1;
		}
		if (size > 2)
			return 0;
		if (size == 1)
			printf("\tld a,l\n\tld (T%u+%u),a\n", n->val2, v);
		else
			printf("\tld (T%u+%u),hl\n", n->val2, v);
		return 1;
	case T_LSTORE:
/*		printf(";L sp %u spval %u %s(%ld)\n", sp, spval, namestr(n->snum), n->value); */
		v += sp;
		if (HAS_LDHLSP && v <= 255) {
			if (size == 2) {
				printf("\tld (sp+%u),hl\n", v);
				return 1;
			}
			if (HAS_LDASP && size == 1) {
				printf("\tld a,l\n\tld (sp + %u),a d\n", v);
				return 1;
			}
		}
		if (v == 0 && size == 2 ) {
			if (n->flags & NORETURN)
				printf("\tex (sp).hl\n");
			else
				printf("\tpop af\n\tpush hl\n");
			return 1;
		}
		/* Rabbit can LD HL,(HL + 0) so we can construct a load fairly ok */
		if (IS_RABBIT && size == 2) {
			printf("\tld hl,0x%x\n\tadd hl,sp\n\tld hl,(hl + 0)\n", v);
			return 1;
		}
		if (size == 1 && (!optsize || v >= LWDIRECT)) {
			printf("\tld a,l\n\tld hl,%u\n\tadd hl,sp\n\tld (hl),a\n", WORD(v));
			if (!(n->flags & NORETURN))
				printf("\tld l,a\n");
			return 1;
		}
		/* For -O3 they asked for it so inline the lot */
		/* We dealt with size one above */
		if (opt > 2 && size == 2) {
			printf("\tex de,hl\n\tld hl,0x%x\n\tadd hl,sp\n\tld (hl),e\n\tinc hl\n", WORD(v));
			printf("\tld (hl),d\n");
			if (!(n->flags & NORETURN))
				printf("\tex de,hl\n");
			return 1;
		}
		/* Via helper magic for compactness on Z80 */
		/* Can rewrite some of them into rst if need be */
		if (size == 1)
			name = "stbyte";
		else
			name = "stword";
		/* Like load the helper is offset by two because of the
		   stack */
		if (v < 24)
			printf("\tcall __%s%u\n", name, v + 2);
		else if (v < 253)
			printf("\tcall __%s\n\t.byte %u\n", name, v + 2);
		else
			printf("\tcall __%sw\n\t.word %u\n", name, v + 2);
		return 1;
	case T_RSTORE:
		load_regvar(n->value, size);
		return 1;
		/* Call a function by name */
	case T_CALLNAME:
		if (cpufeat & 1)
			printf("\tpush af\n");
		printf("\tcall _%s+%u\n", namestr(n->snum), v);
		if (cpufeat & 1)
			printf("\tpop af\n");
		return 1;
	case T_EQ:
		if (size == 2) {
			if (IS_EZ80)
				printf("\tex de,hl\npop hl\nld (hl),de\n");
			else
				printf("\tex de,hl\n\tpop hl\n\tld (hl),e\n\tinc hl\n\tld (hl),d\n");
			if (!nr)
				printf("\tex de,hl\n");
			return 1;
		}
		if (size == 1) {
			printf("\tpop de\n\tex de,hl\n\tld (hl),e\n");
			if (!nr)
				printf("\tex de,hl\n");
			return 1;
		}
		break;
	case T_RDEREF:
		switch(n->value) {
		case 1:
			/* BC is never anything but a byte sized ptr */
			printf("\tld a,(bc)\n");	/* NORETURN but generated imples volatile so keep the ldax */
			if (!nr)
				printf("\tld l,a\n");
			return 1;
		case 2:
			if (size == 4) {
				printf("\tld h,(ix + %u)\n", n->val2 + 3);
				printf("\tld l,(ix + %u)\n", n->val2 + 2);
				printf("\tld (__hireg),hl\n");
			}
			if (size > 1)
				printf("\tld h,(ix + %u)\n", n->val2 + 1);
			printf("\tld l,(ix + %u)\n", n->val2);
			return 1;
		case 3:
			if (size == 4) {
				printf("\tld h,(iy + %u)\n", n->val2 + 3);
				printf("\tld l,(iy + %u)\n", n->val2 + 2);
				printf("\tld (__hireg),hl\n");
			}
			if (size > 1)
				printf("\tld h,(iy + %u)\n", n->val2 + 1);
			printf("\tld l,(iy + %u)\n", n->val2);
			return 1;
		}
		return 0;
	case T_DEREF:
		if (size == 4 && IS_EZ80) {
			printf("\tld de,(hl)\n\tinc hl\n\tinc hl\n\tld hl,(hl)\n\tld (_hireg),hl\n\tex de.hl\n");
			return 1;
		}
		if (size == 2) {
			if (HAS_LDHLHL)
				printf("\tld hl,(hl)\n");
			else
				printf("\tld e,(hl)\n\tinc hl\n\tld d,(hl)\n\tex de,hl\n");
			return 1;
		}
		if (size == 1) {
			printf("\tld l,(hl)\n");
			return 1;
		}
		break;
	case T_FUNCCALL:
		/* Banking has no other effect as indirectly referenced calls go via the stub
		   table so the function has a valid 16bit "address" */
		if (cpufeat & 1)
			printf("\tpush af\n");
		printf("\tcall __callhl\n");
		if (cpufeat & 1)
			printf("\tpop af\n");
		/* The bank linker will rewrite this for banked functions and use the extra
		   stack slot to effectively stack the bank switch info */
		return 1;
	case T_LABEL:
		/* Used for const strings and local static */
		printf("\tld hl,T%u+%u\n", n->val2, v);
		return 1;
	case T_CONSTANT:
		switch(size) {
		case 4:
			printf("\tld hl,0x%x\n", ((v >> 16) & 0xFFFF));
			printf("\tld (__hireg),hl\n");
		case 2:
			printf("\tld hl,0x%x\n", (v & 0xFFFF));
			return 1;
		case 1:
			printf("\tld l,0x%x\n", (v & 0xFF));
			return 1;
		}
		break;
	case T_NAME:
		printf("\tld hl,");
		printf("_%s+%u\n", namestr(n->snum), v);
		return 1;
	case T_LOCAL:
		v += sp;
/*		printf(";LO sp %u spval %u %s(%ld)\n", sp, spval, namestr(n->snum), n->value); */
		printf("\tld hl,0x%x\n", v);
		printf("\tadd hl,sp\n");
		return 1;
	case T_ARGUMENT:
		v += frame_len + argbase + sp;
/*		printf(";AR sp %u spval %u %s(%ld)\n", sp, spval, namestr(n->snum), n->value); */
		printf("\tld hl,0x%x\n", v);
		printf("\tadd hl,sp\n");
		return 1;
	case T_REG:
		if (nr)
			return 1;
		/* A register has no address.. we need to sort this out */
		error("rega");
		return 1;
	case T_CAST:
		/* TODO:Z280 has EXTS */
		return gen_cast(n);
	case T_BOOL:
		/* Rabbit has an boolify operator */
		if (IS_RABBIT) {
			printf("\tbool hl\n");
			return 1;
		}
		break;
	case T_PLUS:
		if (size <= 2) {
			printf("\tpop de\n\tadd hl,de\n");
			return 1;
		}
		break;
	case T_MINUS:
		if (size <= 2) {
			printf("\tpop de\n\tor a\n\tex de,hl\n\tsbc hl,de\n");
			return 1;
		}
		break;
	case T_BANG:
		/* Logic via jumping always uses CC in our case */
		if (r->op == T_ANDAND || r->op == T_OROR)
			r->flags |= USECC;
		/* Always use the helper if we need the actual value */
		if (!(n->flags & CCONLY)) {
			if (r->flags & USECC) {
				/* Should never happen */
				printf(";BOTCH %04x:%04X\n", r->op, r->flags);
				printf("\tcall __cctonot\n");
				return 1;
			}
			return 0;
		}
		/* Already CC format */
		if (r->flags & USECC) {
			if (!(n->flags & CCFIXED)) {
				/* Will need work when we add > >= and friends TODO */
				if (ccflags == ccinvert)
					ccflags = ccnormal;
				else if (ccflags == ccnormal)
					ccflags = ccinvert;
				else
					error("ccf");
				n->flags |= USECC;
				return 1;
			}
			/*  We have a CC we want to invert it but we are not
			    allowed to */
			printf("\tcall __cctonot\n");
			n->flags |= ISBOOL;
			return 1;
		}
		if (size == 4)
			return 0;
		if (!(n->flags & CCFIXED)) {
			/* Turn into CC format */
			if (size == 1)
				printf("\tld a,l\n\tor a\n");
			else if (size == 2)
				printf("\tld a,h\n\tor l\n");
			ccflags = ccinvert;
			n->flags |= USECC;
			return 1;
		}
		return 0;
	case T_BTST:
		/* Always CCONLY */
		if (v < 8)
			printf("\tbit %u, l\n", v);
		else
			printf("\tbit %u, h\n", v - 8);
		n->flags |= USECC;
		return 1;
	case T_BYTEEQ:
		v = n->value;
		/* Value is in L, just compare byte if we can */
		printf("\tld a,0x%x\n", v & 0xFF);
		if ((n->flags & CCONLY) && !(n->flags & CCFIXED)) {
			printf("\tcp l\n");
			ccflags = ccinvert;
			n->flags |= USECC;
			return 1;
		}
		/* Couldn't short cut so do it direct */
		printf("\tcall __cmpeqla\n");
		n->flags |= ISBOOL;
		return 1;
	case T_BYTENE:
		v = n->value;
		/* Value is in L, just compare byte if we can */
		printf("\tld a,0x%x\n", v & 0xFF);
		if (n->flags & CCONLY) {
			printf("\tcp l\n");
			n->flags |= USECC;
			return 1;
		}
		printf("\tcall __cmpnela\n");
		n->flags |= ISBOOL;
		return 1;
	}
	return 0;
}
