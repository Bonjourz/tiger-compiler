#include <stdio.h>
#include "util.h"
#include "table.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "tree.h"
#include "printtree.h"
#include "frame.h"
#include "translate.h"
#include "errormsg.h"

//LAB5: you can modify anything you want.
struct Tr_access_ {
	Tr_level level;
	F_access access;
};

struct Tr_level_ {
	F_frame frame;
	Tr_level parent;
	Tr_accessList var;
};

static Tr_level tr_outermost = NULL;

Tr_level Tr_outermost() {
	if (!tr_outermost)
		tr_outermost = Tr_newLevel(NULL, S_Symbol("tigermain"), NULL);

	return tr_outermost;
}

Tr_access Tr_Access(Tr_level level, F_access acc) {
	Tr_access a = (Tr_access)checked_malloc(sizeof(*a));
	a->level = level;
	a->access = acc;
	return a; 
}

Tr_accessList Tr_AccessList(Tr_access head, Tr_accessList tail) {
	Tr_accessList l = (Tr_accessList)checked_malloc(sizeof(*l));
	l->head = head;
	l->tail = tail;
	return l;
}

Tr_accessList Tr_TrLevelVar(Tr_level level) {
	return level->var;
}

Tr_accessList makeTrAccessList(Tr_level level, F_accessList f_acc) {
	if (!f_acc)
		return NULL;

	return Tr_AccessList(Tr_Access(level, f_acc->head), makeTrAccessList(level, f_acc->tail));
}

Tr_access Tr_allocLocal(Tr_level level, bool escape) {
	Tr_access acc = (Tr_access)checked_malloc(sizeof(*acc));
	acc->level = level;
	acc->access = F_allocLocal(level->frame, escape);

	// Update the access List, add the new access list to the tail
	Tr_accessList tmp = level->var;
	if (!tmp)
		level->var = Tr_AccessList(acc, NULL);

	else {
		while (tmp && tmp->tail) 
			tmp = tmp->tail;
		tmp->tail = Tr_AccessList(acc, NULL);
	}
	return acc;
}

Tr_level Tr_newLevel(Tr_level parent, Temp_label name, U_boolList formals) {
	Tr_level l = (Tr_level)checked_malloc(sizeof(*l));
	l->parent = parent;
	l->frame = F_newFrame(name, formals);
	
	F_accessList f_acc = F_formals(l->frame);
	l->var = makeTrAccessList(l, f_acc);
	return l;
}

static F_fragList frag_list = NULL;
F_fragList Tr_getResult() {
		return frag_list;
}

typedef struct patchList_ *patchList;
struct patchList_ 
{
	Temp_label *head; 
	patchList tail;
};

struct Cx 
{
	patchList trues; 
	patchList falses; 
	T_stm stm;
};

struct Tr_exp_ {
	enum {Tr_ex, Tr_nx, Tr_cx} kind;
	union {T_exp ex; T_stm nx; struct Cx cx; } u;
};

Tr_expList Tr_ExpList(Tr_exp head, Tr_expList tail) {
	Tr_expList l = (Tr_expList)checked_malloc(sizeof(*l));
	l->head = head;
	l->tail = tail;
	return l;
}

static Tr_exp Tr_Ex(T_exp ex) {
	Tr_exp t = (Tr_exp)checked_malloc(sizeof(*t));
	t->kind = Tr_ex;
	t->u.ex = ex;
	return t;
}

static Tr_exp Tr_Nx(T_stm nx) {
	Tr_exp t = (Tr_exp)checked_malloc(sizeof(*t));
	t->kind = Tr_nx;
	t->u.nx = nx;
	return t;
}

static Tr_exp Tr_Cx(patchList trues, patchList falses, T_stm stm) {
	Tr_exp t = (Tr_exp)checked_malloc(sizeof(*t));
	t->kind = Tr_cx;
	t->u.cx.stm = stm;
	t->u.cx.trues = trues;
	t->u.cx.falses = falses;
	return t;
}

static patchList PatchList(Temp_label *head, patchList tail)
{
	patchList list;
	list = (patchList)checked_malloc(sizeof(struct patchList_));
	list->head = head;
	list->tail = tail;
	return list;
}

void doPatch(patchList tList, Temp_label label)
{
	for(; tList; tList = tList->tail)
		*(tList->head) = label;
}

patchList joinPatch(patchList first, patchList second)
{
	if(!first) return second;
	for(; first->tail; first = first->tail);
	first->tail = second;
	return first;
}

static T_exp unEx(Tr_exp e) {
	switch(e->kind) {
		case Tr_ex: 
			return e->u.ex;

		case Tr_cx: {
			Temp_temp r = Temp_newtemp();
			Temp_label t = Temp_newlabel(), f = Temp_newlabel();
			doPatch(e->u.cx.trues, t);
			doPatch(e->u.cx.falses, f);
			return T_Eseq(T_Move(T_Temp(r), T_Const(1)),
							T_Eseq(e->u.cx.stm,
								T_Eseq(T_Label(f),
									T_Eseq(T_Move(T_Temp(r), T_Const(0)),
										T_Eseq(T_Label(t),
											T_Temp(r))))));
		}

		case Tr_nx:
			return T_Eseq(e->u.nx, T_Const(0));
	}
	assert(0);	/* can't get here */
}

static T_stm unNx(Tr_exp e) {
	switch(e->kind) {
		case Tr_ex:
			return T_Exp(e->u.ex);

		case Tr_nx:
			return e->u.nx;

		case Tr_cx: {
			Temp_temp r = Temp_newtemp();
			Temp_label t = Temp_newlabel(), f = Temp_newlabel();
			doPatch(e->u.cx.trues, t);
			doPatch(e->u.cx.falses, f);
			return T_Seq(T_Move(T_Temp(r), T_Const(1)),
							T_Seq(e->u.cx.stm,
								T_Seq(T_Label(f),
									T_Seq(T_Move(T_Temp(r), T_Const(0)),
										T_Seq(T_Label(t), T_Exp(T_Temp(r)))))));
		}
	}
	assert(0);
}

static struct Cx unCx(Tr_exp e) {
	switch(e->kind) {
		case Tr_ex: {
			Temp_label t = Temp_newlabel(), f = Temp_newlabel();
			struct Cx cx;
			cx.stm = T_Cjump(T_ne, e->u.ex, T_Const(0), NULL, NULL);
			cx.trues = PatchList(&(cx.stm->u.CJUMP.true), NULL);
			cx.falses = PatchList(&(cx.stm->u.CJUMP.false), NULL);
			return cx;
		}

		case Tr_nx: {
			EM_error(0, "Can't translate nx to cx");
			assert(0);
		}

		case Tr_cx:
			return e->u.cx;
	}
}

void Tr_procEntryExit(Tr_level level, Tr_exp body, Tr_accessList formals) {
	T_stm stm = T_Move(T_Temp(F_RV()), unEx(body));
	F_frag f = F_ProcFrag(stm, level->frame);
	frag_list = F_FragList(f, frag_list);
}

static T_exp makeStaticLink(Tr_level cur, Tr_level dst) {
	if (dst == NULL) 
		return NULL;

	T_exp sl = T_Temp(F_FP());
	while (cur && cur != dst) {
		sl = T_Mem(T_Binop(T_plus, sl, T_Const(8)));
		cur = cur->parent;
	}

	return sl;
}

static T_expList makeTExpList(Tr_expList l) {
	if (l && l->head)
		return T_ExpList(unEx(l->head), makeTExpList(l->tail));

	else 
		return NULL;
}

Tr_exp Tr_NullExp() {
	return Tr_Ex(T_Const(0));
}

Tr_exp Tr_NilExp() {
	return Tr_Nx(T_Exp(T_Const(0)));
}

Tr_exp Tr_IntExp(int i) {
	return Tr_Ex(T_Const(i));
}

Tr_exp Tr_StringExp(string str) {
	Temp_label label = Temp_newlabel();
	F_frag f = F_StringFrag(label, str);
	frag_list = F_FragList(f, frag_list);
	return Tr_Ex(T_Name(label));
}

Tr_exp Tr_CallExp(Temp_label label, Tr_expList expList, Tr_level cur, Tr_level dst) {
	T_exp staticLink = makeStaticLink(cur, dst->parent);
	T_expList l = makeTExpList(expList);
	if (staticLink)
		return Tr_Ex(T_Call(T_Name(label), T_ExpList(staticLink, l)));
	else
		return Tr_Ex(T_Call(T_Name(label), l));
}

Tr_exp Tr_ArithExp(A_oper oper, Tr_exp left, Tr_exp right) {
	T_binOp op;
	switch(oper) {
		case A_plusOp:{op = T_plus; break;}
		case A_minusOp:{op = T_minus; break;}
		case A_timesOp:{op = T_mul; break;}
		case A_divideOp:{op = T_div; break;}
		default: assert(0);
	}
	return Tr_Ex(T_Binop(op, unEx(left), unEx(right)));
}

Tr_exp Tr_CmpOp(A_oper oper, Tr_exp left, Tr_exp right, int isstr) {
	T_relOp op;
	switch(oper) {
		case A_eqOp:{op = T_eq; break;}
		case A_neqOp:{op = T_ne; break;}
		case A_ltOp:{op = T_lt; break;}
		case A_leOp:{op = T_le; break;}
		case A_gtOp:{op = T_gt; break;}
		case A_geOp:{op = T_ge; break;}
		default: assert(0);
	}
	T_stm stm;
	if (isstr) 
		stm = T_Cjump(op,
						F_externalCall("stringEqual", T_ExpList(unEx(left), T_ExpList(unEx(right), NULL))),
							T_Const(1), NULL, NULL);

	else
		stm = T_Cjump(op, unEx(left), unEx(right), NULL, NULL);

	patchList trues = PatchList(&(stm->u.CJUMP.true), NULL);
	patchList falses = PatchList(&(stm->u.CJUMP.false), NULL);
	return Tr_Cx(trues, falses, stm);
}

static T_stm makeRecordMove(Temp_temp r, Tr_expList expList, int size, int index) {
	if (size == 0)
		return T_Exp(T_Const(0));

	else if (size == 1)
		return T_Move(T_Mem(T_Binop(T_plus, T_Temp(r), T_Const(index * F_wordSize))), 
										unEx(expList->head));
	else
		return T_Seq(T_Move(T_Mem(T_Binop(T_plus, T_Temp(r), T_Const(index * F_wordSize))), 
										unEx(expList->head)),
							makeRecordMove(r, expList->tail, size - 1, index + 1));
}

Tr_exp Tr_RecordExp(Tr_expList expList, int size) {
	Temp_temp r = Temp_newtemp();
	T_exp call = F_externalCall("allocRecord", T_ExpList(T_Const(size * F_wordSize), NULL));
	T_stm move = makeRecordMove(r, expList, size, 0);
	return Tr_Ex(T_Eseq(T_Seq(T_Move(T_Temp(r), call),
												move), T_Temp(r)));
}


Tr_exp Tr_AssignExp(Tr_exp l, Tr_exp r) {
	return Tr_Nx(T_Move(unEx(l), unEx(r)));
}

Tr_exp Tr_SeqExp(Tr_exp head, Tr_exp tail) {
	return Tr_Ex(T_Eseq(unNx(head), unEx(tail)));
}

Tr_exp Tr_IfExp(Tr_exp test, Tr_exp then, Tr_exp elsee) {
	Temp_label t = Temp_newlabel(), f = Temp_newlabel(), done = Temp_newlabel();
	if (elsee == NULL) {
		Temp_temp r = Temp_newtemp();
		struct Cx cx = unCx(test);
		doPatch(cx.trues, t);
		doPatch(cx.falses, f);
		return Tr_Nx(T_Seq(cx.stm,
									T_Seq(T_Label(t), 
										T_Seq(unNx(then), T_Label(f)))));
	}

	else {
		Temp_temp r = Temp_newtemp();
		struct Cx cx = unCx(test);
		doPatch(cx.trues, t);
		doPatch(cx.falses, f);
		if (then->kind == Tr_nx && elsee->kind == Tr_nx) {
			return Tr_Nx(T_Seq(cx.stm,
										T_Seq(T_Label(t),
											T_Seq(unNx(then),
												T_Seq(T_Jump(T_Name(done), Temp_LabelList(done, NULL)),
													T_Seq(T_Label(f),
														T_Seq(unNx(elsee), T_Label(done))))))));
		}

		return Tr_Ex(T_Eseq(T_Seq(cx.stm,
													T_Seq(T_Label(t), 
														T_Seq(T_Move(T_Temp(r), unEx(then)),
															T_Seq(T_Jump(T_Name(done), Temp_LabelList(done, NULL)),
															T_Seq(T_Label(f),
																T_Seq(T_Move(T_Temp(r), unEx(elsee)),
																	T_Label(done))))))), T_Temp(r)));
	}
}

Tr_exp Tr_WhileExp(Tr_exp test, Tr_exp body, Temp_label done) {
	Temp_label t = Temp_newlabel(), b = Temp_newlabel();
	struct Cx cx = unCx(test);
	doPatch(cx.trues, b);
	doPatch(cx.falses, done);
	return Tr_Nx(T_Seq(T_Label(t),
								T_Seq(cx.stm,
									T_Seq(T_Label(b),
										T_Seq(unNx(body),
											T_Seq(T_Jump(T_Name(t), Temp_LabelList(t, NULL)), T_Label(done)))))));
}

Tr_exp Tr_ForExp(Tr_exp lo, Tr_exp hi, Tr_exp body, Temp_label done, Tr_access acc) {
	Temp_label t = Temp_newlabel(), b = Temp_newlabel();
	Temp_temp i =  getReg(acc->access);
	return Tr_Nx(T_Seq(T_Move(T_Temp(i), unEx(lo)),
								T_Seq(T_Label(t),
									T_Seq(T_Cjump(T_le, T_Temp(i), unEx(hi), b, done),
									T_Seq(T_Label(b),
										T_Seq(unNx(body),
											T_Seq(T_Move(T_Temp(i), T_Binop(T_plus, T_Temp(i), T_Const(1))),
												T_Seq(T_Jump(T_Name(t), Temp_LabelList(t, NULL)), T_Label(done)))))))));
}

Tr_exp Tr_BreakExp(Temp_label breakl) {
	return Tr_Nx(T_Jump(T_Name(breakl), Temp_LabelList(breakl, NULL)));
}

Tr_exp Tr_LetExp(Tr_expList decl, Tr_exp body) {
	Tr_exp exp = body;
	Tr_expList l = decl;
	for (; l; l = l->tail) 
		exp = Tr_Ex(T_Eseq(unNx(l->head), unEx(exp)));

	return exp;
}


Tr_exp Tr_ArrayExp(Tr_exp size, Tr_exp init) {
	return Tr_Ex(F_externalCall("initArray", 
									T_ExpList(unEx(size), 
										T_ExpList(unEx(init), NULL))));
}

Tr_exp Tr_SimpleVar(Tr_access acc, Tr_level cur_l) {
	T_exp fp = makeStaticLink(cur_l, acc->level);
	return Tr_Ex(F_Exp(acc->access, fp));
}

Tr_exp Tr_FieldVar(Tr_exp var, int index) {
	return Tr_Ex(T_Mem(T_Binop(T_plus, unEx(var), T_Const(F_wordSize * index))));
}

Tr_exp Tr_SubscriptVar(Tr_exp var, Tr_exp index) {
	return Tr_Ex(T_Mem(T_Binop(T_plus, unEx(var), 
												T_Binop(T_mul, unEx(index), T_Const(F_wordSize)))));
}

Tr_exp Tr_VarDec(Tr_access acc, Tr_exp init) {
	/* The definition of var is always in its own level */
	T_exp dst = F_Exp(acc->access, T_Temp(F_FP()));
	return Tr_Nx(T_Move(dst, unEx(init)));
}
