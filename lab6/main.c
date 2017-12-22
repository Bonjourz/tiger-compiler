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

 stmList = C_linearize(body);

 blo = C_basicBlocks(stmList);

 C_stmListList stmLists = blo.stmLists;

 stmList = C_traceSchedule(blo);

 iList  = F_codegen(frame, stmList);

 iList = F_procEntryExit2(iList);

 struct RA_result ra = RA_regAlloc(frame, iList); 
 
 proc =	F_procEntryExit3(frame, ra.il);

 string procName = S_name(F_name(frame));
 fprintf(out, "\n.text\n");
 fprintf(out, ".globl %s\n", procName);
 fprintf(out, ".type %s, @function\n", procName);
 fprintf(out, "%s:\n", procName);
 fprintf(out, "%s\n", proc->prolog);
 AS_printInstrList (out, proc->body,
                       Temp_layerMap(F_tempMap, ra.coloring));
 fprintf(out, "\n%s", proc->epilog);
}

void doStr(FILE *out, Temp_label label, string str) {
	fprintf(out, "\n.section .rodata\n");
	fprintf(out, ".%s:\n", S_name(label));
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
