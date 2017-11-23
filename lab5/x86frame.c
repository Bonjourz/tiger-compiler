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
	Temp_label l;
	F_accessList a;
};

F_frame F_newFrame(Temp_label l, U_boolList formals) {
	F_frame f = (F_frame)checked_malloc(sizeof(*f));
	f->l = l;
	F_accessList a = NULL;
	int offset = 0;
	for (; formals; formals = formals->tail) {
		/* escape */
		if (formals->head)
			a = F_AccessList(InReg(Temp_newtemp()), a);

		else {
			a = F_AccessList(InFrame(offset * F_wordSize), a);
			offset++;
		}
	}

	f->a = a;
	return f;
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

