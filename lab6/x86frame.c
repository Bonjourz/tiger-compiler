#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "table.h"
#include "tree.h"
#include "frame.h"


static F_access InFrame(int offset);
static F_access InReg(Temp_temp reg);
F_accessList F_AccessList(F_access head, F_accessList tail);
/*Lab5: Your implementation here.*/
const int F_wordSize = 4;
/* Frames */
struct F_frame_{
	Temp_label name;
	F_accessList formals;
	F_accessList locals;

	//the number of arguments
	int argSize;

	//the number of local variables
	int length;
	
	//num of local var in the frame	
	int f_cnt;		

	//register lists for the frame
	Temp_tempList calleesaves;
	Temp_tempList callersaves;
	Temp_tempList specialregs;
};

F_frame F_newFrame(Temp_label l, U_boolList formals) {
	F_frame f = (F_frame)checked_malloc(sizeof(*f));
	f->name = l;
	f->locals = NULL;
	f->argSize = 0;
	f->length = 0;
	f->f_cnt = 0;
	f->callersaves = NULL;
	f->calleesaves = NULL;
	f->specialregs = NULL;

	F_accessList a = NULL;
	int offset = 0;
	for (; formals; formals = formals->tail) {
		/* escape */
		f->argSize++;
		if (formals->head)
			a = F_AccessList(InReg(Temp_newtemp()), a);

		else {
			a = F_AccessList(InFrame(offset * F_wordSize), a);
			offset++;
		}
	}

	f->formals = a;
	return f;
}

Temp_label F_name(F_frame f) {
	return f->name;
}

F_accessList F_formals(F_frame f) {
	return f->formals;
}

F_access F_allocLocal(F_frame f, bool escape) {
	F_access a = NULL;
	if (escape) {
		Temp_temp r = Temp_newtemp();
		a = InReg(r);
	}
	else {
		f->f_cnt++;
		a = InFrame(-(f->f_cnt) * F_wordSize);
	}
	f->locals = F_AccessList(a, f->locals);
	f->length++;
	return a;
}


//varibales
struct F_access_ {
	enum {inFrame, inReg} kind;
	union {
		int offset; //inFrame
		Temp_temp reg; //inReg
	} u;
};

static F_access InFrame(int offset) {
	F_access a = (F_access)checked_malloc(sizeof(*a));
	a->kind = inFrame;
	a->u.offset = offset;
	return a;
}

static F_access InReg(Temp_temp reg) {
	F_access a = (F_access)checked_malloc(sizeof(*a));
	a->kind = inReg;
	a->u.reg = reg;
	return a;
}

Temp_temp getReg(F_access acc) {
	return acc->u.reg;
}

F_accessList F_AccessList(F_access head, F_accessList tail) {
	F_accessList l = (F_accessList)checked_malloc(sizeof(*l));
	l->head = head;
	l->tail = tail;
	return l;
}

F_frag F_StringFrag(Temp_label label, string str) {
	F_frag f = (F_frag)checked_malloc(sizeof(*f));
	f->kind = F_stringFrag;
	f->u.stringg.label = label;
	f->u.stringg.str = str;
	return f;
}

F_frag F_ProcFrag(T_stm body, F_frame frame) {
	F_frag f = (F_frag)checked_malloc(sizeof(*f));
	f->kind = F_procFrag;
	f->u.proc.body = body;
	f->u.proc.frame = frame;
	return f;                                     
}

F_fragList F_FragList(F_frag head, F_fragList tail) {
	F_fragList f_l = (F_fragList)checked_malloc(sizeof(*f_l));
	f_l->head = head;
	f_l->tail = tail;
	return f_l;                                     
}

T_exp F_Exp(F_access acc, T_exp framePtr) {
	if (acc->kind == inFrame)
		return T_Mem(T_Binop(T_plus, framePtr, T_Const(acc->u.offset)));
	else
		return T_Temp(acc->u.reg);
}

T_exp F_externalCall(string s, T_expList args) {
	return T_Call(T_Name(Temp_namedlabel(s)), args);
}

static Temp_temp rv = NULL;
T_exp F_RV() {
	if (!rv)
		rv = Temp_newtemp();
	return T_Temp(rv);
}

static Temp_temp fp = NULL;
T_exp F_FP() {
	if (!fp)
		fp = Temp_newtemp();
	return T_Temp(fp);
}
