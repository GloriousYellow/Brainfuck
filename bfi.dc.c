/*
 *  Convert tree to code for dc(1)
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "bfi.tree.h"
#include "bfi.run.h"

static FILE * ofd;

/*
 * DC Registers:
 * p	The bf pointer
 * a	The bf array or tape
 * F	The bf program (pushed before start)
 *
 * b	A stack of currently open while loops
 * i	Input a chracter to the TOS
 * o	Output the TOS as one character (calls reg 'm' if needed)
 * m	Replaces TOS with (TOS & cell_size)
 * M	Used for when % gives a negative in 'm'.
 *
 * C	May be defined as an array of character printers for lox
 *
 * A B Z
 *	Used as temps, restored.
 *
 *
 * Note: traditional dc has no way of entering characters from stdin, a filter
 * number used that converts the characters into numbers. End of file is
 * properly detected though.
 */

static void
fetch_cell(int offset)
{
    if (offset>0)
	fprintf(ofd, "lp%d+;a", offset);
    else if (offset==0)
	fprintf(ofd, "lp;a");
    else
	fprintf(ofd, "lp_%d+;a", -offset);
}

static void
save_cell(int offset)
{
    if (offset>0)
	fprintf(ofd, "lp%d+:a\n", offset);
    else if (offset==0)
	fprintf(ofd, "lp:a\n");
    else
	fprintf(ofd, "lp_%d+:a\n", -offset);
}

static void
prt_value(char * prefix, int count, char * suffix)
{
    if (count>=0)
	fprintf(ofd, "%s%d%s", prefix, count, suffix);
    else
	fprintf(ofd, "%s_%d%s", prefix, -count, suffix);
}

/* This is true for characters that are safe in a [ ] string */
/* '\' if for bsd dc, '|' and '~' are for dc.sed */
#define okay_for_printf(xc) \
		    ( (xc) == '\n' || ( (xc)>= ' ' && (xc) <= '~' && \
		      (xc) != '\\' && /* (xc) != '|' && (xc) != '~' && */ \
		      (xc) != '[' && (xc) != ']' ) \
		    )
void
print_dc(void)
{
    struct bfi * n = bfprog;
    int stackdepth = 0;
    int hello_world;
    int used_lix = 0;
    int used_lox = 0;
    ofd = stdout;

    calculate_stats();
    hello_world = (total_nodes == node_type_counts[T_PRT] && !noheader);

    if (hello_world) {
	struct bfi * v = n;
	while(v && v->type == T_PRT && okay_for_printf(v->count))
	    v=v->next;
	if (v == 0) {
	    printf("[");
	    for(v=n; v; v=v->next) putchar(v->count);
	    printf("]P\n");
	    return;
	}
    }

    if (most_neg_maad_loop < 0 && node_type_counts[T_MOV] != 0)
	fprintf(ofd, "[%dsp\n", -most_neg_maad_loop);
    else
	fprintf(ofd, "[0sp\n");

    while(n)
    {
	switch(n->type)
	{
	case T_MOV:
	    prt_value("lp", n->count, "+sp\n");
	    break;

	case T_ADD:
	    fetch_cell(n->offset);
	    prt_value("", n->count, "+");
	    save_cell(n->offset);
	    break;

	case T_SET:
	    prt_value("", n->count, "");
	    save_cell(n->offset);
	    break;

	case T_CALC:
	    /* p[offset] = count + count2 * p[offset2] + count3 * p[offset3] */
	    if (n->offset == n->offset2 && n->count2 == 1) {
		fetch_cell(n->offset2);
		if (n->count)
		    prt_value("", n->count, "+");
	    } else if (n->count2 != 0) {
		fetch_cell(n->offset2);
		if (n->count2 != 1)
		    prt_value("", n->count2, "*");
		if (n->count)
		    prt_value("", n->count, "+");
	    } else
		prt_value("", n->count, " ");

	    if (n->count3 != 0) {
		fetch_cell(n->offset3);

		if (n->count3 != 1)
		    prt_value("", n->count3, "*+");
		else
		    fprintf(ofd, "+");
	    }

	    save_cell(n->offset);
	    break;

	case T_IF: case T_MULT: case T_CMULT: case T_FOR:
	case T_WHL:
	    stackdepth++;
	    fprintf(ofd, "[\n");
	    break;

	case T_END:
	    stackdepth--;
	    fetch_cell(n->offset);
	    if (cell_mask > 0)	fprintf(ofd, "lmx ");

	    fprintf(ofd, "0!=b]Sb ");

	    fetch_cell(n->offset);
	    if (cell_mask > 0)	fprintf(ofd, "lmx ");

	    fprintf(ofd, "0!=bLbc\n");
	    break;

	case T_ENDIF:
	    stackdepth--;
	    fprintf(ofd, "]Sb ");
	    fetch_cell(n->offset);
	    if (cell_mask > 0)	fprintf(ofd, "lmx ");

	    fprintf(ofd, "0!=bLbc\n");
	    break;

	case T_PRT:
	    if (n->count == -1) {
		fetch_cell(n->offset);
		fprintf(ofd, "lox\n");
		used_lox = 1;
		break;
	    }

	    if (!okay_for_printf(n->count) ||
		    !n->next || n->next->type != T_PRT) {

		prt_value("", n->count, "lox\n");
		used_lox = 1;
	    } else {
		int i = 0, j;
		struct bfi * v = n;
		char *s, *p;
		while(v->next && v->next->type == T_PRT &&
			    okay_for_printf(v->next->count)) {
		    v = v->next;
		    i++;
		}
		p = s = malloc(i+2);

		for(j=0; j<i; j++) {
		    *p++ = n->count;
		    n = n->next;
		}
		*p++ = n->count;
		*p++ = 0;

		fprintf(ofd, "[%s]P\n", s);
		free(s);
	    }
	    break;

	case T_INP:
	    fprintf(ofd, "lix");
	    used_lix = 1;

	    fprintf(ofd, "[");
	    save_cell(n->offset);
	    fprintf(ofd, "]SA d _1!=A ");
	    fprintf(ofd, "0sALAc");
	    break;

	case T_STOP:
	    fprintf(ofd, "[STOP command executed\n]P\n");
	    if (stackdepth>1)
		fprintf(ofd, "%dQ\n", stackdepth);
	    else
		fprintf(ofd, "q\n");
	    break;

	default:
	    fprintf(stderr, "Code gen error: "
		    "%s\t"
		    "%d,%d,%d,%d,%d,%d\n",
		    tokennames[n->type],
		    n->offset, n->count,
		    n->offset2, n->count2,
		    n->offset3, n->count3);
	    exit(1);
	    break;
	}
	n=n->next;
    }

    fprintf(ofd, "q]SF\n");

#ifndef DISABLE_DC_V7
    if (used_lox) {
	int i;
	fprintf(ofd, "[");
	for (i = 1; i < 127; i++) {
	    if (i == '\n') {
		fprintf(ofd, "[[\n]P]%d:C\n", i);
	    } else if (i >= ' ' && i <= '~' &&
		       i != '[' && i != ']' && i != '\\' && i != '|') {
		fprintf(ofd, "[[%c]P]%d:C", i, i);
	    } else if (i < 100) {
		fprintf(ofd, "[%dP]%d:C", i, i);
	    }
	    if (i % 8 == 7)
		fprintf(ofd, "\n");
	}
	fprintf(ofd, "]SZ\n");

	/* Use 'Z' command to detect v7 dc. Detects dc.sed as not v7. */
	fprintf(ofd, "[1   [aP]so ]SB\n");
	fprintf(ofd, "[lZx [256%%256+256%%;Cx]so ]SA\n");
	fprintf(ofd, "0 0 [o] Z 1=B 0=A 0sALAsBLBsZLZ c\n");

	/*  Note: dc.sed works in traditional (v7) mode, but as it takes
	 *  80 seconds just to print "Hello World!" there's not much point
	 *  doing an auto detection. As all other dc implementations I
	 *  know of have the 'a' command ( "make TOS a one char string" )
	 *  I'm assuming all future ones will too.
	 *
	 *  Note: if it's required the 'r' command is the most likely
	 *  candidate for an auto detection but the 'Z' will still be needed
	 *  to detect v7 without an error and dc.sed will output an error. */
    }
#else
    if (used_lox) fprintf(ofd, "[aP]so\n");
#endif
    if (cell_size > 0)
	fprintf(ofd, "[%d+]sM [%d %% d0>M]sm\n", cell_mask+1, cell_mask+1);

    if (used_lix) {
	if (input_string) {
	    char * p = input_string;
	    fprintf(ofd, "0si\n");
	    for(;*p; p++)
		fprintf(ofd, "%dlid1+si:I\n", (unsigned char)*p);
	    fprintf(ofd, "_1lid1+si:I\n");
	    fprintf(ofd, "0li:I0sn\n");
	    fprintf(ofd, "[lnd1+sn;I]si\n");
	} else {

	    fprintf(ofd, "[1G [sB_1]SA [bAla]SB 0=A Bx 0sALAaBLB+ ]si\n");
	    fprintf(ofd, "[? z [_1]SA 0=A 0sALA+ ]si\n");
	}
    }

    fprintf(ofd, "LFx\n");
}