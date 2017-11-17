%{
/* Lab2 Attention: You are only allowed to add code in this file and start at Line 26.*/
#include <string.h>
#include "util.h"
#include "symbol.h"
#include "errormsg.h"
#include "absyn.h"
#include "y.tab.h"

int charPos=1;

int yywrap(void)
{
 charPos=1;
 return 1;
}

void adjust(void)
{
 EM_tokPos=charPos;
 charPos+=yyleng;
}

/*
* Please don't modify the lines above.
* You can add C declarations of your own below.
*/

/* @function: getstr
 * @input: a string literal
 * @output: the string value for the input which has all the escape sequences 
 * translated into their meaning.
 */

char str[1024];
int str_len;    /* The yyleng for the whole str(including '"') */
int pos;        /* The position of the pointer of the string*/
int num_com;        /* The num of nested comment */
char *getSRING(const char *str)
{
	//optional: implement this function if you need it
	return NULL;
}



%}
 /* You can add lex defINITIALions here. */

digits [0-9]+

letters [a-zA-Z]+

int {digits}

id {letters}({letters}|{digits}|_)*

%Start COMMENT STR QUOTE

%%
  /* 
  * Below is an example, which you can wipe out
  * and write reguler expressions and actions of your own.
  */ 

<INITIAL>"," {adjust(); return COMMA;}
<INITIAL>":" {adjust(); return COLON;}
<INITIAL>";" {adjust(); return SEMICOLON;}
<INITIAL>"(" {adjust(); return LPAREN;}
<INITIAL>")" {adjust(); return RPAREN;}
<INITIAL>"[" {adjust(); return LBRACK;}
<INITIAL>"]" {adjust(); return RBRACK;}
<INITIAL>"{" {adjust(); return LBRACE;}
<INITIAL>"}" {adjust(); return RBRACE;}
<INITIAL>"." {adjust(); return DOT;}
<INITIAL>"+" {adjust(); return PLUS;}
<INITIAL>"-" {adjust(); return MINUS;}
<INITIAL>"*" {adjust(); return TIMES;}
<INITIAL>"/" {adjust(); return DIVIDE;}
<INITIAL>"=" {adjust(); return EQ;}
<INITIAL>"<>" {adjust(); return NEQ;}
<INITIAL>"<" {adjust(); return LT;}
<INITIAL>"<=" {adjust(); return LE;}
<INITIAL>">" {adjust(); return GT;}
<INITIAL>">=" {adjust(); return GE;}
<INITIAL>"&" {adjust(); return AND;}
<INITIAL>"|" {adjust(); return OR;}
<INITIAL>":=" {adjust(); return ASSIGN;}
<INITIAL>"array" {adjust(); return ARRAY;}
<INITIAL>"if" {adjust(); return IF;}
<INITIAL>"then" {adjust(); return THEN;}
<INITIAL>"else" {adjust(); return ELSE;}
<INITIAL>"while" {adjust(); return WHILE;}
<INITIAL>"for" {adjust(); return FOR;}
<INITIAL>"to" {adjust(); return TO;}
<INITIAL>"do" {adjust(); return DO;}
<INITIAL>"let" {adjust(); return LET;}
<INITIAL>"in" {adjust(); return IN;}
<INITIAL>"end" {adjust(); return END;}
<INITIAL>"of" {adjust(); return OF;}
<INITIAL>"break" {adjust(); return BREAK;}
<INITIAL>"nil" {adjust(); return NIL;}
<INITIAL>"function" {adjust(); return FUNCTION;}
<INITIAL>"var" {adjust(); return VAR;}
<INITIAL>"type" {adjust(); return TYPE;}

<INITIAL>"\n" {adjust(); EM_newline();}
<INITIAL>" " {adjust();}
<INITIAL>\t {adjust();}

<INITIAL>"/*" {adjust(); num_com = 1; BEGIN COMMENT;}

<INITIAL>\" {str_len = 1; pos = 0; BEGIN STR;}

<INITIAL>"//" {adjust(); BEGIN QUOTE;}

<INITIAL>{int} {adjust(); yylval.ival = atoi(yytext); return INT;}
<INITIAL>{id} {adjust(); yylval.sval = String(yytext); return ID;}

<INITIAL>. {adjust(); EM_error(EM_tokPos, "illegal character");} 

<COMMENT>"*/" {adjust(); num_com--; if (num_com == 0) BEGIN INITIAL;}
<COMMENT>"/*" {adjust(); num_com++;}
<COMMENT>"\n" {adjust(); EM_newline();}
<COMMENT>. {adjust();}

<STR>\" {adjust(); str_len++; charPos = str_len + EM_tokPos;str[pos] = '\0'; yylval.sval = String(str); BEGIN INITIAL; return STRING;}

<STR>\\n {str[pos] = '\n'; pos++; str_len += yyleng;}
<STR>\\t {str[pos] = '\t'; pos++; str_len += yyleng;}
<STR>\\ {str_len += yyleng; EM_newline();}
<STR><<EOF>> {adjust(); EM_error(EM_tokPos, "Illegal eof");}
<STR>. {strcpy(str+pos, yytext); str_len += yyleng; pos += yyleng;}

<QUOTE>. {adjust();}
<QUOTE><<EOF>> {adjust(); EM_error(EM_tokPos, "Illegal eof");}
<QUOTE>\n {adjust(); EM_newline(); BEGIN INITIAL;}

. {yyless(0); BEGIN INITIAL;}

