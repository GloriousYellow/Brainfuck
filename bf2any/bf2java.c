#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"

/*
 * Java translation From BF, runs at about ? instructions per second.
 *
 * Java has a limit of 64k on the size of a compiled function.
 * This is REALLY stupid and makes it completely unsuitable for some
 * applications. (Anything to do with automatically generated code
 * for example.)
 */

/* Guess how many of our instructions we can put in a Java function.
 * Also there is how many instructions we _should_ put in a Java function
 * as a large function won't be JIT optimised properly and so can run MUCH
 * slower.
 */
#define MAXINSTR (12+65536/80)

struct instruction {
    int ch;
    int count;
    int icount;
    struct instruction * next;
    struct instruction * prev;
    struct instruction * loop;
    char * cstr;
} *pgm = 0, *last = 0, *jmpstack = 0;

static int icount = 0;

int ind = 0;
#define I        printf("%*s", ind*4, "")
#define prv(s,v) printf("%*s" s "\n", ind*4, "", (v))

static void reformat(void);
static void loutcmd(int ch, int count, struct instruction *n);
static void add_cstring(char *);
static void print_cstring(char *);

static struct instruction *
node_calloc(void)
{
    struct instruction * n = calloc(1, sizeof*n);
    if (!n) { perror("bf2java"); exit(42); }
    return n;
}

void
outcmd(int ch, int count)
{
    struct instruction * n;

    /* I need to count 'print' commands for the java functions */
    if (ch == '"') { add_cstring(get_string()); return; }

    n = node_calloc();
    n->ch = ch;
    n->count = count;
    n->icount = ++icount;
    n->prev = last;
    if (!last) {
        pgm = n;
    } else {
        last->next = n;
    }
    last = n;

    if (n->ch == '[') {
        n->loop = jmpstack; jmpstack = n;
    } else if (n->ch == ']') {
        n->loop = jmpstack; jmpstack = jmpstack->loop; n->loop->loop = n;
    }

    if (ch != '~') return;

    last->ch = '}';	/* Make the end an end function */

    if (icount > MAXINSTR)
	reformat();

    for(n=pgm; n; n=n->next)
	loutcmd(n->ch, n->count, n);

    loutcmd('}', 0,0);	/* End of the class */

    while(pgm) {
        n = pgm;
        pgm = pgm->next;
        if (n->cstr)
            free(n->cstr);
        memset(n, '\0', sizeof*n);
        free(n);
    }
}

static void
reformat()
{
    struct instruction * n, * t;
    struct instruction ** functions = 0;
    int nfunc = 0, functions_len = 0, i;

    functions = calloc(sizeof(*functions), (functions_len=128));
    if (!functions) { perror("bf2java"); exit(42); }
    nfunc = 1;

    /* Firstly we slice up the program so that the body of (nearly) every
       loop is in it's own function
       A loop that has just one token in it isn't sliced.
     */
    for(n=pgm; n; n=n->next) {
	if (n->ch == ']' && n->loop->next != n && n->loop->next->next != n) {

	    if (nfunc>=functions_len) {
		functions = realloc(functions,
		    sizeof(*functions)*(functions_len=functions_len+128));
		if (!functions) { perror("bf2java"); exit(42); }
	    }

	    /* Start of function */
	    t = node_calloc();
	    t->ch = '{';
	    t->count = nfunc;
	    t->next = n->loop->next;
	    t->next->prev = t;

	    functions[nfunc] = t;

	    /* End of function */
	    t = node_calloc();
	    t->ch = '}';
	    t->prev = n->prev;
	    t->prev->next = t;
	    t->loop = functions[nfunc];	/* Save start of function pointer */
	    t->loop->loop = t;

	    /* Call function. */
	    t = node_calloc();
	    t->ch = '@';
	    t->count = nfunc;

	    t->next = n;
	    t->prev = n->loop;
	    t->prev->next = t;
	    t->next->prev = t;

	    nfunc++;
	}
    }

    /* We need to do the next process to main() too so make a bf0 function
     * to contain those instructions and leave just a call in main().
     */
    t = node_calloc();
    t->ch = '{';
    functions[0] = t;
    t->next = pgm->next;	/* 2nd token as first will be '!' */
    t->next->prev = t;
    t->loop = last;		/* Last is a '}' not a '~' */
    t->loop->loop = t;

    n = pgm; n->next = 0;

    /* Add call bf0 */
    t = node_calloc();
    t->ch = '@';
    t->count = 0;

    t->next = 0; t->prev = n; n->next = t;
    n = t;

    /* And finish up the main() function */
    t = node_calloc();
    t->ch = '}';

    t->next = 0; t->prev = n; n->next = t;
    last = n = t;

    /* Now go down every function and count up it's length.
     * Because we don't have 'break' or 'goto' we can turn the tail of a
     * run that's too long into an extra function.  I try to make sure
     * there are no 'runt' functions by cutting the oversized function
     * in half rather than chopping off the excess.
     */
    for (i=0; i<nfunc; i++)
    {
	int flen = 0;
	n = functions[i];
	while(n) {flen++; n=n->next; }
	if (flen > MAXINSTR) {
	    /* It's too big, chop it in half. */
	    n = functions[i];
	    while(flen>1) { n=n->next; flen-=2; }

	    /* We can't slice a loop */
	    if (n->ch == '[') n=n->prev;
	    if (n->next->ch == ']') n=n->next;

	    if (nfunc>=functions_len) {
		functions = realloc(functions,
		    sizeof(*functions)*(functions_len=functions_len+128));
		if (!functions) { perror("bf2java"); exit(42); }
	    }

	    /* Make new function from tail. */
	    t = node_calloc();
	    t->ch = '{';
	    t->count = nfunc;
	    t->next = n->next;
	    t->next->prev = t;

	    t->loop = functions[i]->loop;
	    t->loop->loop = t;

	    functions[nfunc] = t;
	    functions[i]->loop = 0;
	    n->next = 0;

	    /* Append a call to n */
	    t = node_calloc();
	    t->ch = '@';
	    t->count = nfunc;
	    n->next = t;
	    t->prev = n;

	    /* n is now the call */
	    n = t;

	    /* Append End of function to n */
	    t = node_calloc();
	    t->ch = '}';
	    n->next = t;
	    t->prev = n;

	    t->loop = functions[i];
	    t->loop->loop = t;

	    nfunc++;
	    i--;	/* Recheck this function */
	}
    }

    /* Then paste the functions onto the end of the program */
    for (i=0; i<nfunc; i++)
    {
	last->next = functions[i];
	last->next->prev = last;
	last = last->next->loop;
    }

    free(functions);	/* Cleanup workspace */
}

void
loutcmd(int ch, int count, struct instruction *n)
{
    switch(ch) {
    case '!':

	if (bytecell) {
	    printf("%s%d%s%d%s",
			"import java.io.InputStream;"
		"\n"	"import java.io.OutputStream;"
		"\n"	"import java.io.IOException;"
		"\n"
		"\n"	"public class fuck {"
		"\n"	"    static byte[] m;"
		"\n"	"    static int p;"
		"\n"	"    static byte v;"
		"\n"
		"\n"	"    private static void i() {"
		"\n"	"        try {"
		"\n"	"            System.in.read(m,p,1);"
		"\n"	"        } catch (IOException e) {}"
		"\n"	"    }"
		"\n"
		"\n"	"    private static void o() {"
		"\n"	"//        try {"
		"\n"	"            System.out.write(m[p]);"
		"\n"	"            System.out.flush();"
		"\n"	"//        } catch (IOException e) {}"
		"\n"	"    }"
		"\n"
		"\n"	"    public static void main(String[] args) {"
		"\n"	"        m=new byte[",tapesz,"];"
		"\n"	"        p=",tapeinit,";"
		"\n");

	} else {
	    printf("%s%d%s%d%s",
			"import java.io.InputStream;"
		"\n"	"import java.io.OutputStream;"
		"\n"	"import java.io.IOException;"
		"\n"
		"\n"	"public class fuck {"
		"\n"	"    static int[] m;"
		"\n"	"    static int p;"
		"\n"	"    static int v;"
		"\n"	"    static byte ch;"
		"\n"
		"\n"	"    private static void i() {"
		"\n"	"        try {"
		"\n"	"            v = System.in.read();"
		"\n"	"            if (v>=0) m[p] = v;"
		"\n"	"        } catch (IOException e) {}"
		"\n"	"    }"
		"\n"
		"\n"	"    private static void o() {"
		"\n"	"        ch = (byte) m[p];"
		"\n"	"        System.out.write(ch);"
		"\n"	"        System.out.flush();"
		"\n"	"    }"
		"\n"
		"\n"	"    public static void main(String[] args) {"
		"\n"	"        m=new int[",tapesz,"];"
		"\n"	"        p=",tapeinit,";"
		"\n");
	}

	ind+=2;
	break;

    case '~':
	ind--;
	I; printf("}\n");
	ind--;
	I; printf("}\n");
	break;

    case '@':
	I; printf("bf%d();\n", count);
	break;

    case '{':
	I; printf("private static void bf%d() {\n", count);
	ind++;
	break;

    case '}':
	ind--;
	I; printf("}\n");
	break;

    case '=': I; printf("m[p] = %d;\n", count); break;
    case 'B': I; printf("v = m[p];\n"); break;
    case 'M': I; printf("m[p] += v*%d;\n", count); break;
    case 'N': I; printf("m[p] -= v*%d;\n", count); break;
    case 'S': I; printf("m[p] += v;\n"); break;
    case 'Q': I; printf("if(v!=0) m[p] = %d;\n", count); break;
    case 'm': I; printf("if(v!=0) m[p] += v*%d;\n", count); break;
    case 'n': I; printf("if(v!=0) m[p] -= v*%d;\n", count); break;
    case 's': I; printf("if(v!=0) m[p] += v;\n"); break;

    case 'X': I; printf("throw new IllegalStateException(\"Infinite Loop detected.\");"); break;

    case '+': I; printf("m[p] += %d;\n", count); break;
    case '-': I; printf("m[p] -= %d;\n", count); break;
    case '<': I; printf("p -= %d;\n", count); break;
    case '>': I; printf("p += %d;\n", count); break;

    case '[':
	I; printf("while(m[p] != 0) {\n");
	ind++;
	break;

    case ']':
	ind--;
	I; printf("}\n");
	break;

    case '.': I; printf("o();\n"); break;
    case ',': I; printf("i();\n"); break;

    case '"': print_cstring(n->cstr); break;

    }
}

static void
print_cstring(char * str)
{
    char buf[256];
    int gotnl = 0, gotperc = 0;
    size_t outlen = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || gotnl || outlen > sizeof(buf)-8))
	{
	    buf[outlen] = 0;
	    if (gotnl) {
		buf[outlen-2] = 0;
		prv("System.out.println(\"%s\");", buf);
	    } else
		prv("System.out.print(\"%s\");", buf);
	    gotnl = gotperc = 0; outlen = 0;
	}
	if (!*str) break;

	if (*str == '\n') gotnl = 1;
	if (*str >= ' ' && *str <= '~' && *str != '"' && *str != '\\') {
	    buf[outlen++] = *str;
	} else if (*str == '"' || *str == '\\') {
	    buf[outlen++] = '\\'; buf[outlen++] = *str;
	} else if (*str == '\n') {
	    buf[outlen++] = '\\'; buf[outlen++] = 'n';
	} else if (*str == '\t') {
	    buf[outlen++] = '\\'; buf[outlen++] = 't';
	} else {
	    char buf2[16];
	    int n;
	    sprintf(buf2, "\\%03o", *str & 0xFF);
	    for(n=0; buf2[n]; n++)
		buf[outlen++] = buf2[n];
	}
    }
}

/*
 * This function is like the print_cstring() function and must calculate the
 * string limits the same as that function. It is needed so that i can count
 * the number of Java statments that a single print will become.
 */
static void
add_cstring(char * str)
{
    char buf[256];
    int gotnl = 0, gotperc = 0;
    size_t outlen = 0;
    int bcount = 0;

    if (!str) return;

    for(;; str++) {
	if (outlen && (*str == 0 || gotnl || outlen > sizeof(buf)-8))
	{
	    struct instruction * n;

	    n = node_calloc();
	    n->ch = '"';
	    n->count = 0;
	    n->icount = ++icount;
	    n->prev = last;
	    if (!last) {
		pgm = n;
	    } else {
		last->next = n;
	    }
	    last = n;

	    buf[bcount] = 0;
	    n->cstr = strdup(buf);
	    gotnl = gotperc = 0; outlen = 0; bcount = 0;
	}
	if (!*str) break;

	if (*str == '\n') gotnl = 1;
	buf[bcount++] = *str;
	if (*str >= ' ' && *str <= '~' && *str != '"' && *str != '\\') {
	    outlen++;
	} else if (*str == '"' || *str == '\\' || *str == '\n' || *str == '\t') {
	    outlen+=2;
	} else {
	    outlen+=4;
	}
    }
}
