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
	
	//num of local var in the frame	
	int f_cnt;		

	Temp_map F_tempMap;
};

F_frame F_newFrame(Temp_label l, U_boolList formals) {
	F_frame f = (F_frame)checked_malloc(sizeof(*f));
	f->name = l;
	f->locals = NULL;
	f->f_cnt = 0;
	f->F_tempMap = Temp_name();

	F_accessList aHead = NULL, aTail = NULL;
	F_access a = NULL;
	int offset = 3;
	for (; formals; formals = formals->tail) {
		/* escape */
		if (!formals->head)
			a = InReg(Temp_newtemp());

		else {
			a = InFrame(offset * F_wordSize);
			offset++;
		}

		if (aHead == NULL) {
			aHead = F_AccessList(a, NULL);
			aTail = aHead;
		}

		else {
			aTail->tail = F_AccessList(a, NULL);
			aTail = aTail->tail;
		}
	}

	f->formals = aHead;
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
	if (!escape) {
		Temp_temp r = Temp_newtemp();
		a = InReg(r);
	}
	else {
		f->f_cnt++;
		a = InFrame(-(f->f_cnt) * F_wordSize);
	}
	f->locals = F_AccessList(a, f->locals);
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

int F_makeSpill(F_frame f) {
	f->f_cnt++;
	return f->f_cnt;
}

T_exp F_externalCall(string s, T_expList args) {
	return T_Call(T_Name(Temp_namedlabel(s)), args);
}

static Temp_temp eax = NULL;
static Temp_temp ebx = NULL;
static Temp_temp ecx = NULL;
static Temp_temp edx = NULL;
static Temp_temp esi = NULL;
static Temp_temp edi = NULL;
static Temp_temp ebp = NULL;
static Temp_temp esp = NULL;

static void initMachineReg() {
	eax = Temp_newtemp();
	ebx = Temp_newtemp();
	ecx = Temp_newtemp();
	edx = Temp_newtemp();
	esi = Temp_newtemp();
	edi = Temp_newtemp();
	ebp = Temp_newtemp();
	esp = Temp_newtemp();
}

Temp_temp F_RV() {
	if (!eax)
		initMachineReg();
	return eax;
}

Temp_temp F_FP() {
	if (!ebp)
		initMachineReg();
	return ebp;
}

Temp_temp F_EAX() {
	if (!eax)
		initMachineReg();
	return eax;
}

Temp_temp F_EBX() {
	if (!ebx)
		initMachineReg();
	return ebx;
}

Temp_temp F_ECX() {
	if (!ecx)
		initMachineReg();
	return ecx;
}

Temp_temp F_EDX() {
	if (!edx)
		initMachineReg();
	return edx;
}

Temp_temp F_EDI() {
	if (!edi)
		initMachineReg();
	return edi;
}

Temp_temp F_ESI() {
	if (!esi)
		initMachineReg();
	return esi;
}

Temp_temp F_EBP() {
	if (!ebp)
		initMachineReg();
	return ebp;
}


Temp_temp F_ESP() {
	if (!esp)
		initMachineReg();
	return esp;
}

bool isMachineReg(Temp_temp temp) {
	if (temp == F_EAX() |
		temp == F_EBX() |
		temp == F_ECX() |
		temp == F_EDX() |
		temp == F_EDI() |
		temp == F_ESI() |
		temp == F_ESP() |
		temp == F_EBP())
		return TRUE;
	else
		return FALSE;
}

void initTempMap(Temp_map temMap) {
	Temp_enter(temMap, F_RV(), "%eax");
	Temp_enter(temMap, F_FP(), "%ebp");
	Temp_enter(temMap, F_EAX(), "%eax");
	Temp_enter(temMap, F_EBX(), "%ebx");
	Temp_enter(temMap, F_ECX(), "%ecx");
	Temp_enter(temMap, F_EDX(), "%edx");
	Temp_enter(temMap, F_EDI(), "%edi");
	Temp_enter(temMap, F_ESI(), "%esi");
	Temp_enter(temMap, F_EBP(), "%ebp");
	Temp_enter(temMap, F_ESP(), "%esp");
}

static Temp_tempList L(Temp_temp h, Temp_tempList t) {
	return Temp_TempList(h, t);
}

AS_instrList F_procEntryExit2(AS_instrList body) {
	Temp_temp tmp1 = Temp_newtemp();
	Temp_temp tmp2 = Temp_newtemp();
	Temp_temp tmp3 = Temp_newtemp();
	char *out = (char *)checked_malloc(30);
	sprintf(out, "movl `s0, `d0");
	AS_instrList result = NULL, il = NULL;
	result = il = AS_InstrList(AS_Move(out, L(tmp1, NULL), L(F_EBX(), NULL)), NULL);
	il->tail = AS_InstrList(AS_Move(out, L(tmp2, NULL), L(F_EDI(), NULL)), NULL);
	il = il->tail;
	il->tail = AS_InstrList(AS_Move(out, L(tmp3, NULL), L(F_ESI(), NULL)), NULL);
	il = il->tail;
	il->tail = body;
	while(il->tail)
		il = il->tail;
	il->tail = AS_InstrList(AS_Move(out, L(F_ESI(), NULL), L(tmp3, NULL)), NULL);
	il = il->tail;
	il->tail = AS_InstrList(AS_Move(out, L(F_EDI(), NULL), L(tmp2, NULL)), NULL);
	il = il->tail;
	il->tail = AS_InstrList(AS_Move(out, L(F_EBX(), NULL), L(tmp1, NULL)), NULL);
	return result;
}

AS_proc F_procEntryExit3(F_frame f, AS_instrList body) {
	char *prologue = (char *)checked_malloc(100);
	char *epilogue = (char *)checked_malloc(100);
	sprintf(prologue, "push %%ebp\n");
	sprintf(prologue, "%smovl %%esp, %%ebp\n", prologue);
	if (f->f_cnt != 0) {
		sprintf(prologue, "%ssubl $%d, %%esp\n", prologue, f->f_cnt * F_wordSize);
		sprintf(epilogue, "%saddl $%d, %%esp\n", epilogue, f->f_cnt * F_wordSize);
	}
	sprintf(epilogue, "%spopl %%ebp\n", epilogue);
	sprintf(epilogue, "%sret\n", epilogue);

	return AS_Proc(prologue, body, epilogue);
}

