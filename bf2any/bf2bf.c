#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
 * BF translation to BF. This isn't an identity translation because even
 * without most of the peephole optimisation the loader will still remove
 * sequences that are cancelling or that can never be run because they
 * begin with the '][' comment loop introducer.
 *
 * Then there are all the variants.
 *
 * Some of these also generate a set of C #define lines so the output
 * is compilable as C.
 */

extern int bytecell;

char bf[] = "><+-.,[]";

/* Language "C" */
char * cbyte[] = { "m+=1;", "m-=1;", "++*m;", "--*m;",
		   "write(1,m,1);", "read(0,m,1);", "while(*m){", "}", 0 };
char * cbyte_rle[] = { ";m+=1", ";m-=1", ";*m+=1", ";*m-=1",
		   ";write(1,m,1)", ";read(0,m,1)", ";while(*m){", ";}", "+1"};

char * cint[] = { "m+=1;", "m-=1;", "++*m;", "--*m;",
	"putchar(*m);", "{int _c=getchar();if(_c!=EOF)*m=_c;}",
	"while(*m){", "}", 0 };

char * cint_rle[] = { ";m+=1", ";m-=1", ";*m+=1", ";*m-=1",
	";putchar(*m)", ";{int _c=getchar();if(_c!=EOF)*m=_c;}",
	";while(*m){", ";}", "+1" };

/* Language "ook" */
char * ook[] = {"Ook. Ook?", "Ook? Ook.", "Ook. Ook.", "Ook! Ook!",
		"Ook! Ook.", "Ook. Ook!", "Ook! Ook?", "Ook? Ook!"};

/* Language "blub" */
char *blub[] = {"blub. blub?", "blub? blub.", "blub. blub.", "blub! blub!",
		"blub! blub.", "blub. blub!", "blub! blub?", "blub? blub!"};

/* Language "fuck fuck" */
char *f__k[] = {"folk", "sing", "barb", "teas", "cask", "kerb", "able", "bait"};

/* Language "pogaack" */
char * pogaack[] = {"pogack!", "pogaack!", "pogaaack!", "poock!",
		    "pogaaack?", "poock?", "pogack?", "pogaack?"};

/* Language "triplet" */
char * trip[] = { "OOI", "IOO", "III", "OOO", "OIO", "IOI", "IIO", "OII" };

/* Language "Descriptive BF" */
char * nice[] = { "right", "left", "up", "down", "out", "in", "begin", "end" };
char * bc[] = { "r", "l", "u", "d", "o", "i", "b", "e", "x" };

/* Order should be "there", "once", "was", "a", "fish", "named", "Fred" */
char * fish[] = { "once", "there", "was", "a", "fish", "dead", "named", "Fred" };

/* Silly (er) ones. */
char * dotty[] = { "..", "::", ".:.", ".::", ":.::", ":...", ":.:.", ":..:" };
char * lisp2[] = { "((", "))", "()(", "())", ")())", ")(((", ")()(", ")(()" };

/* Language COW: Not quite as simple as some commands aren't direct replacements. */
char * moo[] = {"moO", "mOo", "MoO", "MOo",
		"MMMMOOMooOOOmooMMM", "OOOMoo", "MOOmoOmOo", "MoOMOomoo"};

/* BF Doubler doubles the cell size. */
char * doubler[] = {">>>>", "<<<<", ">+<+[>-]>[->>+<]<<", ">+<[>-]>[->>-<]<<-",
		    ".", ">>>[-]<<<[-],",
		    ">+<[>-]>[->+>[<-]<[<]>[-<+>]]<-" "[+<",
		    ">+<[>-]>[->+>[<-]<[<]>[-<+>]]<-" "]<"};

char ** lang = 0;
char ** c = 0;
char langver = 0;
int col = 0;
int maxcol = 72;
int state = 0;
int c_style = 0;

void risbf(int ch);
void rlebf(int ch, int count);

int
check_arg(char * arg)
{
    if (strcmp(arg, "-c") == 0) {
	lang = cbyte; langver = 0; c_style = 2; return 1;
    } else
    if (strcmp(arg, "-n") == 0 || strcmp(arg, "-nice") == 0) {
	lang = nice; langver = 0; c_style = 1; return 1;
    } else
    if (strcmp(arg, "-mini") == 0) {
	lang = bc; langver = 0; c_style = 1; return 1;
    } else
    if (strcmp(arg, "-f") == 0 || strcmp(arg, "-fish") == 0) {
	lang = fish; langver = 0; c_style = 1; return 1;
    } else
    if (strcmp(arg, "-trip") == 0 || strcmp(arg, "-triplet") == 0) {
	lang = trip; langver = 0; c_style = 1; return 1;
    } else
    if (strcmp(arg, "-ook") == 0) {
	lang = ook; langver = 0; c_style = 0; return 1;
    } else
    if (strcmp(arg, "-blub") == 0) {
	lang = blub; langver = 0; c_style = 0; return 1;
    } else
    if (strcmp(arg, "-moo") == 0) {
	lang = moo; langver = 0; c_style = 0; return 1;
    } else
    if (strcmp(arg, "-fk") == 0) {
	lang = f__k; langver = 0; c_style = 1; return 1;
    } else
    if (strcmp(arg, "-pogaack") == 0) {
	lang = pogaack; langver = 0; c_style = 0; return 1;
    } else
    if (strcmp(arg, "-:") == 0) {
	lang = dotty; langver = 3; c_style = 0; return 1;
    } else
    if (strcmp(arg, "-lisp") == 0) {
	lang = lisp2; langver = 3; c_style = 0; return 1;
    } else
    if (strcmp(arg, "-risbf") == 0) {
	lang = 0; langver = 1; c_style = 0; return 1;
    } else
    if (strcmp(arg, "-rle") == 0) {
	lang = bc; langver = 2; c_style = 1; return 1;
    } else
    if (strcmp(arg, "-dump") == 0) {
	lang = 0; langver = 4; c_style = 0; return 1;
    } else
    if (strcmp(arg, "-O") == 0 && langver == 4) {
	return 1;
    } else
    if (strncmp(arg, "-w", 2) == 0 && arg[2] >= '0' && arg[2] <= '9') {
	maxcol = atol(arg+2);
	return 1;
    } else
    if (strcmp(arg, "-double") == 0) {
	lang = doubler; langver = 3; c_style = 0; return 1;
    } else
    if (strcmp("-h", arg) ==0) {
	fprintf(stderr, "%s\n",
	"\t"    "-c      Plain C"
	"\n\t"  "-rle    Odd RLE C translation"
	"\n\t"  "-nice   Nice memorable C translation."
	"\n\t"  "-mini   Compact C translation."
	"\n\t"  "-fish   There once was a (dead) fish named Fred"
	"\n\t"  "-trip   Triplet like translation"
	"\n\t"  "-ook    Ook!"
	"\n\t"  "-blub   Blub!"
	"\n\t"  "-moo    Cow -- http://www.frank-buss.de/cow.html"
	"\n\t"  "-fk     fuck fuck"
	"\n\t"  "-pogaack Pogaack."
	"\n\t"  "-:      Dotty"
	"\n\t"  "-lisp   Lisp Zero"
	"\n\t"  "-risbf  RISBF"
	"\n\t"  "-dump   Token dump"
	"\n\t"  "-double BF to BF translation, cell size doubler."
	"\n\t"  "-w99    Width to line wrap after, default 72"
	);
	return 1;
    } else
	return 0;
}

static void
pc(int ch)
{
    if (col>=maxcol && maxcol) {
	putchar('\n');
	col = 0;
	if (ch == ' ') ch = 0;
    }
    if (ch) {
	putchar(ch);
	col++;
    }
}

void
outcmd(int ch, int count)
{
    if (ch == '!') {
	if (langver != 2) {
	    if (bytecell) c = cbyte; else c = cint;
	} else {
	    if (bytecell) c = cbyte_rle; else c = cint_rle;
	}
	if (lang == cbyte) lang = c;
    }

    if (langver == 0 || langver == 2) {
	while(count-->0){
	    if (lang) {
		pc(0);
		col += printf("%s%s", col?" ":"", lang[strchr(bf,ch)-bf]);
		if (langver == 2)
		    while(count-->0){
			pc(0);
			col += printf("%s%s", col?" ":"", lang[8]);
		    }
	    } else
		pc(ch);
	}
    } else if (langver == 1) {
	while (count-->0)
	    risbf(ch);
    } else if (langver == 3) {
	while(count-->0){
	    char * p = lang[strchr(bf,ch)-bf];
	    while (*p)
		pc(*p++);
	}
    } else if (langver == 4) {
	printf("%c %d\n", ch, count);
	col = 0;
    }

    if (ch == '~') {
	pc(0);
	if (c_style == 1)
	    col += printf("%s%s", col?" ":"", "_");
	if (c_style == 2)
	    col += printf("%s%s", col?" ":"", "return 0;}");
	if(col)
	    putchar('\n');
    }

    if (ch == '!' && c_style) {
	int i;
	if (bytecell)
	    printf("#include<unistd.h>\n");
	else
	    printf("#include<stdio.h>\n");
	if (c_style == 1) {
	    for (i=0; i<8 + (langver == 2); i++)
		printf("#define %s %s\n", lang[i], c[i]);
	    printf("#define _ %sreturn 0;}\n", langver==2?";":"");
	}
	if (bytecell)
	    printf("char mem[30000];int main(){register char*m=mem;\n");
	else
	    printf("int mem[30000];int main(){register int*m=mem;\n");
    }
}

void
risbf(int ch)
{
    switch(ch) {
    case '>':
	if (state!=1) pc('*'); state=1;
	pc('+');
	break;
    case '<':
	if (state!=1) pc('*'); state=1;
	pc('-');
	break;
    case '+':
	if (state!=0) pc('*'); state=0;
	pc('+');
	break;
    case '-':
	if (state!=0) pc('*'); state=0;
	pc('-');
	break;
    case '.': pc('/'); pc('/'); break;
    case ',': pc('/'); pc('*'); break;
    case '[': pc('/'); pc('+'); break;
    case ']': pc('/'); pc('-'); break;
    }
}

#if 0 /* OLDCODE */
void
rlebf(int ch, int count)
{
    if (ch == '!') {
	printf(	"#include<unistd.h>"
	"\n"	"#define r ;m+=1"
	"\n"	"#define l ;m-=1"
	"\n"	"#define u ;*m+=1"
	"\n"	"#define d ;*m-=1"
	"\n"	"#define b ;while(*m){"
	"\n"	"#define e ;}"
	"\n"	"#define o ;write(1,m,1)"
	"\n"	"#define i ;read(0,m,1)"
	"\n"	"#define _ ;return 0;}"
	"\n"	"char mem[30000];int main(){register char*m=mem\n");
    }
    if (count > 0) {
	pc(0);
	col += printf("%s%s", col?" ":"", lang[strchr(bf,ch)-bf]);
	if (count > 1) col += printf("*%d", count);
    }
    if (ch == '~') {
	pc(0);
	col += printf("%s%s", col?" ":"", "_");
    }
}
#endif
