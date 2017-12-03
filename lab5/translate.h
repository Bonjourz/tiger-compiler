#ifndef TRANSLATE_H
#define TRANSLATE_H

#include "util.h"
#include "absyn.h"
#include "temp.h"
#include "frame.h"

/* Lab5: your code below */

typedef struct Tr_exp_ *Tr_exp;
typedef struct Tr_expList_ *Tr_expList;
struct Tr_expList_ {Tr_exp head; Tr_expList tail;};
Tr_expList Tr_ExpList(Tr_exp head, Tr_expList tail);

typedef struct Tr_access_ *Tr_access;

typedef struct Tr_accessList_ *Tr_accessList;
struct Tr_accessList_ {Tr_access head; Tr_accessList tail;};


typedef struct Tr_level_ *Tr_level;

Tr_accessList Tr_AccessList(Tr_access head, Tr_accessList tail);
Tr_accessList Tr_TrLevelVar(Tr_level level);
Tr_level Tr_outermost(void);

Tr_level Tr_newLevel(Tr_level parent, Temp_label name, U_boolList formals);

Tr_accessList Tr_formals(Tr_level level);

Tr_access Tr_allocLocal(Tr_level level, bool escape);

Tr_level Tr_newLevel(Tr_level parent, Temp_label name, U_boolList formals);

F_fragList Tr_getResult(void);

void Tr_procEntryExit(Tr_level, Tr_exp, Tr_accessList);

Tr_exp Tr_NullExp();
Tr_exp Tr_NilExp();
Tr_exp Tr_IntExp(int i);
Tr_exp Tr_StringExp(string str);
Tr_exp Tr_CallExp(Temp_label label, Tr_expList expList, Tr_level cur, Tr_level dst);
Tr_exp Tr_ArithExp(A_oper oper, Tr_exp left, Tr_exp right);
Tr_exp Tr_CmpOp(A_oper oper, Tr_exp left, Tr_exp right, int isstr);
Tr_exp Tr_RecordExp(Tr_expList expList, int size);
Tr_exp Tr_SeqExp();
Tr_exp Tr_AssignExp(Tr_exp l, Tr_exp r);
Tr_exp Tr_IfExp(Tr_exp test, Tr_exp then, Tr_exp elsee);
Tr_exp Tr_WhileExp(Tr_exp test, Tr_exp body, Temp_label done) ;
Tr_exp Tr_ForExp(Tr_exp lo, Tr_exp hi, Tr_exp body, Temp_label done, Tr_access acc);
Tr_exp Tr_BreakExp(Temp_label breakl);
Tr_exp Tr_LetExp(Tr_exp head, Tr_exp tail);
Tr_exp Tr_ArrayExp(Tr_exp size, Tr_exp init);

Tr_exp Tr_SimpleVar(Tr_access acc, Tr_level cur_l);
Tr_exp Tr_FieldVar(Tr_exp var, int index);
Tr_exp Tr_SubscriptVar(Tr_exp var, Tr_exp index);

Tr_exp Tr_VarDec(Tr_access acc, Tr_exp init);
#endif
