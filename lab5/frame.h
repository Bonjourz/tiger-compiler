
/*Lab5: This header file is not complete. Please finish it with more definition.*/

#ifndef FRAME_H
#define FRAME_H

#include "tree.h"

extern const int F_wordSize;

typedef struct F_frame_ *F_frame;

F_frame F_newFrame(Temp_label label, U_boolList formals);
Temp_label F_name(F_frame f);

typedef struct F_access_ *F_access;
typedef struct F_accessList_ *F_accessList;
Temp_temp getReg(F_access acc);

F_access F_allocLocal(F_frame f, bool escape);
F_accessList F_formals(F_frame f);

struct F_accessList_ {F_access head; F_accessList tail;};

/* declaration for fragments */
typedef struct F_frag_ *F_frag;
struct F_frag_ {enum {F_stringFrag, F_procFrag} kind;
			union {
				struct {Temp_label label; string str;} stringg;
				struct {T_stm body; F_frame frame;} proc;
			} u;
};

F_frag F_StringFrag(Temp_label label, string str);
F_frag F_ProcFrag(T_stm body, F_frame frame);

typedef struct F_fragList_ *F_fragList;
struct F_fragList_ 
{
	F_frag head; 
	F_fragList tail;
};

F_fragList F_FragList(F_frag head, F_fragList tail);

T_exp F_Exp(F_access acc, T_exp framPtr);
T_exp F_externalCall(string s, T_expList args);
T_exp F_FP(void);
T_exp F_RV(void);

#endif
