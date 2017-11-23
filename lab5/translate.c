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


struct Tr_accessList_ {
	Tr_access head;
	Tr_accessList tail;	
};

struct Tr_level_ {
	F_frame frame;
	Tr_level parent;
};

static Tr_level tr_outermost = NULL;

Tr_level Tr_outermost() {
	if (!tr_outermost)
		tr_outermost = (Tr_level)checked_malloc(sizeof(*tr_outermost));

	return tr_outermost;
}

Tr_accessList Tr_AccessList(Tr_access head, Tr_accessList tail) {
	Tr_accessList l = (Tr_accessList)checked_malloc(sizeof(*l));
	l->head = head;
	l->tail = tail;
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
			/* To Do */
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
			/* To Do */
			cx.stm = T_Cjump(T_eq, e->u.ex, T_Const(0), NULL, NULL);
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
	F_frag f = F_ProcFrag(unNx(body), NULL);
	frag_list = F_FragList(f, frag_list);
}

Tr_exp Tr_Null() {
	return Tr_Ex(T_Const(0));
}

Tr_exp Tr_String(string str) {
	Temp_label label = Temp_newlabel();
	F_frag f = F_StringFrag(label, str);
	frag_list = F_FragList(f, frag_list);
	return Tr_Ex(T_Name(label));
}

