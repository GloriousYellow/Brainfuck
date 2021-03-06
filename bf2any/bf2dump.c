#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf2any.h"
#include "move_opt.h"

static char *
cell(int mov)
{
    static char buf[3+3+sizeof(mov)*3];
    if (mov == 0) return "*m";
    sprintf(buf, "m[%d]", mov);
    return buf;
}

void
outcmd(int ch, int count)
{
static int ind = 0;
    int mov = 0;

#ifdef CHEADER
    if (ch == '!')
	printf("%s\n",  "#include<stdio.h>"
		"\n"	"#define I(a,b)"
		"\n"	"#define putch(x) putchar(x)"
		"\n"	"#define getch(x) v=getchar();if(v!=EOF) x=v;"
		"\n"	"unsigned char mem[30000], *m=mem, v;"
		"\n"	"int"
		"\n"	"main()");
#endif

    printf("I('%c', %d) \t", ch, count);

    move_opt(&ch, &count, &mov);
    if (ch == 0) { putchar('\n'); return; }

    if (ch == ']' || ch == '~') ind--;
    printf("%*s", ind*2, "");

    switch(ch) {
    case '=': printf("%s = %d;\n", cell(mov), count); break;
    case 'B': printf("v = %s;\n", cell(mov)); break;
    case 'M': printf("%s += v*%d;\n", cell(mov), count); break;
    case 'N': printf("%s -= v*%d;\n", cell(mov), count); break;
    case 'S': printf("%s += v;\n", cell(mov)); break;
    case 'Q': printf("if(v!=0) %s = %d;\n", cell(mov), count); break;
    case 'm': printf("if(v!=0) %s += v*%d;\n", cell(mov), count); break;
    case 'n': printf("if(v!=0) %s -= v*%d;\n", cell(mov), count); break;
    case 's': printf("if(v!=0) %s += v;\n", cell(mov)); break;

    case '>': printf("m += %d;\n", count); break;
    case '<': printf("m -= %d;\n", count); break;
    case '+': printf("%s += %d;\n", cell(mov), count); break;
    case '-': printf("%s -= %d;\n", cell(mov), count); break;
    case '.': printf("putch(%s);\n", cell(mov)); break;
    case ',': printf("getch(%s);\n", cell(mov)); break;

    case 'X':
	printf("fprintf(stderr, \"Abort: Infinite Loop.\\n\"); exit(1);\n");
	break;

    case '[':
	printf("while (%s!=0) {\n", cell(mov));
	ind++;
	break;

    case ']':
	if (count > 0)
	    printf("  m += %d;\n%10s\t%*s", count, "", ind*2, "");
	else if (count < 0)
	    printf("  m -= %d;\n%10s\t%*s", -count, "", ind*2, "");
	printf("} /* %s */\n", cell(mov));
	break;

    case '"':
	{
	    char * s = get_string(), *p=s;
	    int percent = 0;
	    for(p=s;*p && !percent;p++) percent = (*p == '%');
	    printf("printf(%s\"", percent?"\"%s\",":"");
	    for(;*s;s++)
		if (*s>=' ' && *s<='~' && *s!='\\' && *s!='"')
		    putchar(*s);
		else if (*s == '\n')
		    printf("\\n");
		else if (*s == '\\' || *s == '"')
		    printf("\\%c", *s);
		else
		    printf("\\%03o", *s);
	    printf("\");\n");
	}
	break;

    case '!': puts("{"); ind++; break;
    case '~': puts("}"); break;

    default:
	printf("/* ? */\n");
	break;
    }
}
