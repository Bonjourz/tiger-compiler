/*
 * main.c
 */

#include <stdio.h>
#include <string.h>
#include "util.h"
#include "symbol.h"
#include "types.h"
#include "absyn.h"
#include "errormsg.h"
#include "temp.h" /* needed by translate.h */
#include "tree.h" /* needed by frame.h */
#include "assem.h"
#include "frame.h" /* needed by translate.h and printfrags prototype */
#include "translate.h"
#include "env.h"
#include "semant.h" /* function prototype for transProg */
#include "canon.h"
#include "prabsyn.h"
#include "printtree.h"
#include "escape.h" 
#include "parse.h"
#include "codegen.h"
#include "graph.h"
#include "regalloc.h"

extern bool anyErrors;

/*Lab6: complete the function doProc
 * 1. initialize the F_tempMap
 * 2. initialize the register lists (for register allocation)
 * 3. do register allocation
 * 4. output (print) the assembly code of each function
 
 * Uncommenting the following printf can help you debugging.*/

/* print the assembly language instructions to filename.s */
static void doProc(FILE *out, F_frame frame, T_stm body)
{
 AS_proc proc;
 struct RA_result allocation;
 T_stmList stmList;
 AS_instrList iList;
 struct C_block blo;

 Temp_map F_tempMap = Temp_empty();
 initTempMap(F_tempMap);
 /*
 printf("machin reg: eax:%d, ebx:%d, ecx%d, edx:%d, edi:%d, esi:%d, esp:%d, ebp:%d\n",
  Temp_int(F_EAX()), Temp_int(F_EBX()), Temp_int(F_ECX()), Temp_int(F_EDX()), 
  Temp_int(F_EDI()), Temp_int(F_ESI()),Temp_int(F_ESP()), Temp_int(F_EBP()));*/
 //printf("doProc for function %s:\n", S_name(F_name(frame)));
  //printStmList(stdout, T_StmList(body, NULL));
 //printf("-------====IR tree=====-----\n");

 stmList = C_linearize(body);
 /*printStmList(stdout, stmList);
 printf("-------====Linearlized=====-----\n");*/

 blo = C_basicBlocks(stmList);
 C_stmListList stmLists = blo.stmLists;
 /*for (; stmLists; stmLists = stmLists->tail) {
 	printStmList(stdout, stmLists->head);
	printf("------====Basic block=====-------\n");
 }*/

 stmList = C_traceSchedule(blo);
 //if (!strncmp(S_name(F_name(frame)), "quicksort", 5)) {
 //printStmList(stdout, stmList);
 //printf("-------====trace=====-----\n");}
 iList  = F_codegen(frame, stmList); /* 9 */
  //printf("fuckframe: %s\n", S_name(F_name(frame)));
 //printf("divide===============\n");
 //if (!strncmp(S_name(F_name(frame)), "quicksort", 5)) {
 //AS_printInstrList(stdout, iList, Temp_layerMap(F_tempMap, Temp_name()));
 //printf("\n\n----======before RA=======-----\n\n");}

 struct RA_result ra = RA_regAlloc(frame, iList); 
 
 proc =	F_procEntryExit3(frame, ra.il);
 //if (!strncmp(S_name(F_name(frame)), "quicksort", 5)) {
 string procName = S_name(F_name(frame));
 fprintf(out, "\n.text\n");
 fprintf(out, ".globl %s\n", procName);
 fprintf(out, ".type %s, @function\n", procName);
 fprintf(out, "%s:\n", procName);
 fprintf(out, "%s\n", proc->prolog);
 AS_printInstrList (out, proc->body,
                       Temp_layerMap(F_tempMap, ra.coloring));
 fprintf(out, "\n%s", proc->epilog);//}
}

void doStr(FILE *out, Temp_label label, string str) {
	fprintf(out, "\n.section .rodata\n");
	fprintf(out, ".%s:\n", S_name(label));
	//it may contains zeros in the middle of string. To keep this work, we need to print all the charactors instead of using fprintf(str)
	fprintf(out, ".int %d\n", (int)strlen(str));
  fprintf(out, ".string \"");
  for (; *str != '\0'; str++) {
    if (*str == '\n')
      fprintf(out, "\\n");
    else if (*str == '\t')
      fprintf(out, "\\t");
    else
      fprintf(out, "%c", *str);
  }
  fprintf(out, "\"\n");
}

int main(int argc, string *argv)
{
 A_exp absyn_root;
 S_table base_env, base_tenv;
 F_fragList frags;
 char outfile[100];
 FILE *out;

 if (argc==2) {
   absyn_root = parse(argv[1]);
   if (!absyn_root)
     return 1;
     
#if 0
   pr_exp(out, absyn_root, 0); /* print absyn data structure */
   fprintf(out, "\n");
#endif

   Esc_findEscape(absyn_root);
   frags = SEM_transProg(absyn_root);
   if (anyErrors) return 1; /* don't continue */

   /* convert the filename */
   sprintf(outfile, "%s.s", argv[1]);
   out = fopen(outfile, "w");
   out = NULL;
   /* Chapter 8, 9, 10, 11 & 12 */
   for (;frags;frags=frags->tail)
     if (frags->head->kind == F_procFrag) {
       doProc(out, frags->head->u.proc.frame, frags->head->u.proc.body);
	 }
     else if (frags->head->kind == F_stringFrag) 
	   doStr(out, frags->head->u.stringg.label, frags->head->u.stringg.str);

   fclose(out);
   return 0;
 }
 EM_error(0,"usage: tiger file.tig");
 return 1;
}
