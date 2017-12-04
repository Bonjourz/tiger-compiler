#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"
#include "types.h"
#include "env.h"
#include "semant.h"
#include "helper.h"
#include "translate.h"

/*Lab5: Your implementation of lab5.*/

struct expty 
{
	Tr_exp exp; 
	Ty_ty ty;
};

//In Lab4, the first argument exp should always be **NULL**.
struct expty expTy(Tr_exp exp, Ty_ty ty)
{
	struct expty e;

	e.exp = exp;
	e.ty = ty;

	return e;
}

F_fragList SEM_transProg(A_exp exp){
	S_table e_base_tenv = E_base_tenv();
	S_table e_base_venv = E_base_venv();
	Tr_level tr_outermost = Tr_outermost();
	Temp_label breakl = NULL;
	struct expty e = transExp(e_base_venv, e_base_tenv, exp, tr_outermost, breakl);
	Tr_procEntryExit(tr_outermost, e.exp, Tr_TrLevelVar(tr_outermost));
	return Tr_getResult();
}

Ty_ty actual_ty(Ty_ty ty) {
	if (ty->kind == Ty_name)
		return actual_ty(ty->u.name.ty);

	else
		return ty;
}

int match(Ty_ty left, Ty_ty right) {
	if (left->kind == Ty_nil && right->kind == Ty_nil)
		return 0;

	if (left == right)
		return 1;

	if (right->kind == Ty_nil && left->kind == Ty_record)
		return 1;

	if (left->kind == Ty_nil && right->kind == Ty_record)
		return 1;

	return 0;
}

Ty_fieldList makeTyFieldList(S_table tenv, A_fieldList recordList) {
	if (!recordList)
		return NULL;

	Ty_ty record_ty = S_look(tenv, recordList->head->typ);
	if (!record_ty) {
		EM_error(recordList->head->pos, "undefined type %s", S_name(recordList->head->typ));
		return NULL;
	}
	Ty_field ty_field = Ty_Field(recordList->head->name, record_ty);
	return Ty_FieldList(ty_field, makeTyFieldList(tenv, recordList->tail));
}

Ty_tyList makeFormalTyList(S_table tenv, A_fieldList params) {
	if (!params)
		return NULL;

	Ty_ty ty = S_look(tenv, params->head->typ);
	if (!ty) {
		EM_error(params->head->pos, "undefined type %s", S_name(params->head->typ));
		return NULL;
	}
	return Ty_TyList(actual_ty(ty), makeFormalTyList(tenv, params->tail));
}

int cycleDefine(S_table tenv, A_namety namety) {
	if (namety->ty->kind != A_nameTy)
		return 0;

	Ty_ty ty = S_look(tenv, namety->name);
	Ty_ty actual = ty->u.name.ty;
	while(actual->kind == Ty_name) {
		actual = actual->u.name.ty;
		if (ty == actual)
			return 1;
	}

	// Here the type of actual is not 'Ty_name'
	ty->u.name.ty = actual;
	return 0;
}

Tr_expList makeTrExpList(Tr_exp exp, Tr_expList l) {
	Tr_expList ele = Tr_ExpList(exp, NULL);
	if (!l)
		return ele;
	
	else {
		Tr_expList tmp = l;
		while(tmp->tail)
			tmp = tmp->tail;

		tmp->tail = ele;
		return l; 
	}
}

U_boolList makeFunEscape(A_fieldList params) {
	if (!params || !(params->head))
		return NULL;

	return U_BoolList(params->head->escape, makeFunEscape(params->tail));
}

struct expty transExp(S_table venv, S_table tenv, A_exp a, Tr_level cur_l, Temp_label breakl) {
	switch(a->kind) {
		case A_varExp: 
			return transVar(venv, tenv, a->u.var, cur_l, breakl);

		case A_nilExp:
			return expTy(Tr_NilExp(), Ty_Nil());

		case A_intExp:
			return expTy(Tr_IntExp(a->u.intt), Ty_Int());

		case A_stringExp:
			return expTy(Tr_StringExp(a->u.stringg), Ty_String());

		case A_callExp: {
			//printf("call %s\n", S_name(a->u.call.func));
			E_enventry x = S_look(venv, a->u.call.func);
			if (x && x->kind == E_funEntry) {
				A_expList args = a->u.call.args;
				Ty_tyList formals = x->u.fun.formals;
				Tr_expList tr_expList = NULL;
				for (; args && formals; args = args->tail, formals = formals->tail) {
					struct expty argTy = transExp(venv, tenv, args->head, cur_l, breakl);
					if (!match(formals->head, argTy.ty))
						EM_error(args->head->pos, "para type mismatch");

					tr_expList = makeTrExpList(argTy.exp, tr_expList);
				}
				if (formals)
						EM_error(a->pos, "too more params in function %s", S_name(a->u.call.func));
				if (args)
						EM_error(a->pos, "too many params in function %s", S_name(a->u.call.func));

				//printf("p\n");
				return expTy(Tr_CallExp(x->u.fun.label, tr_expList, cur_l, x->u.fun.level), x->u.fun.result);
			}

			else {
				EM_error(a->pos, "undefined function %s", S_name(a->u.call.func));
                return expTy(NULL, Ty_Int());
			}
		}

		case A_opExp: {
			struct expty left = transExp(venv, tenv, a->u.op.left, cur_l, breakl);
			struct expty right = transExp(venv, tenv, a->u.op.right, cur_l, breakl);
			A_oper oper = a->u.op.oper;
			switch(oper) {
				case A_plusOp:
				case A_minusOp:
				case A_timesOp:
				case A_divideOp:{
					if (left.ty->kind != Ty_int)
						EM_error(a->u.op.left->pos, "integer required");
					if (right.ty->kind != Ty_int)
						EM_error(a->u.op.right->pos, "integer required");
					return expTy(Tr_ArithExp(oper, left.exp, right.exp), Ty_Int());
				}

				case A_eqOp:
				case A_neqOp:{
					if (!match(left.ty, right.ty)) {
						EM_error(a->pos, "same type required");
					}

					if (left.ty->kind == Ty_string)
						return expTy(Tr_CmpOp(oper, left.exp, right.exp, 1), Ty_Int());
					else if (left.ty->kind == Ty_int)
						return expTy(Tr_CmpOp(oper, left.exp, right.exp, 0), Ty_Int());
					else if (right.ty->kind == Ty_nil)
						return expTy(Tr_CmpOp(oper, left.exp, right.exp, 0), Ty_Int());
				}

				case A_ltOp:
				case A_leOp:
				case A_gtOp:
				case A_geOp:{
					if (left.ty->kind == Ty_int && right.ty->kind == Ty_int)
						return expTy(Tr_CmpOp(oper, left.exp, right.exp, 1), Ty_Int());

					else if (left.ty->kind == Ty_string && right.ty->kind == Ty_string)
						return expTy(Tr_CmpOp(oper, left.exp, right.exp, 0), Ty_Int());

					else {
						EM_error(a->pos, "same type required");
						return expTy(NULL, Ty_Int());
					}
				}
				default:
					assert(0);
			}
		}

		case A_recordExp: {
			/* struct {S_symbol typ; A_efieldList fields;} record; */
			Ty_ty record_ty = S_look(tenv, a->u.record.typ);
			if (!record_ty) {
				EM_error(a->pos, "undefined type %s", S_name(a->u.record.typ));
				return expTy(NULL, Ty_Int());
			}

			Ty_ty act_re_ty = actual_ty(record_ty);
			if (act_re_ty->kind != Ty_record)
				EM_error(a->pos, "The type of %s is not record type", S_name(a->u.record.typ));

			/* Field list from exp */
			A_efieldList e_l = a->u.record.fields;
			/* Field list from ty */
			Ty_fieldList t_f_l = act_re_ty->u.record;
			int size = 0;
			Tr_expList tr_expList = NULL;
			for (; e_l && t_f_l; e_l = e_l->tail, t_f_l = t_f_l->tail) {
				A_efield a_field = e_l->head;
				Ty_field ty_field = t_f_l->head;
				struct expty field_exp = transExp(venv, tenv, a_field->exp, cur_l, breakl);
				if (!match(actual_ty(ty_field->ty), field_exp.ty))
					EM_error(a_field->exp->pos, "type mismatch");

				size++;
				tr_expList = makeTrExpList(field_exp.exp, tr_expList);
			}

			if (e_l || t_f_l)
				EM_error(a->pos, "the num of record mismatch");

			return expTy(Tr_RecordExp(tr_expList, size), act_re_ty);
		}

		case A_seqExp: {
			// TO DO : the result of transexp
			/* Must consider if there is no sequence of expression */
			Tr_exp exp = Tr_NullExp();
			struct expty r = expTy(exp, Ty_Void());
			A_expList seq = a->u.seq;
			int count = 0; // need to delete
			for (; seq; seq = seq->tail) {
				struct expty tmp = transExp(venv, tenv, seq->head, cur_l, breakl);
				r.exp = Tr_SeqExp(r.exp, tmp.exp);
				r.ty = tmp.ty;
				//printf("seq exp %d\n", count++);
				//transExp(venv, tenv, seq->head, cur_l, breakl);
			}

			return r;
		}

		case A_assignExp: {
			/* Check if the variable is read only */
			if (a->u.assign.var->kind == A_simpleVar) {
				E_enventry x = S_look(venv, a->u.assign.var->u.simple);
				if (!x) {
					EM_error(a->pos, "Undefined x");
					return expTy(NULL, Ty_Void());
				}

				else if (x->kind == E_varEntry && x->readonly == 1) {
					EM_error(a->pos, "loop variable can't be assigned");
					return expTy(NULL, Ty_Void());
				}
			}

			struct expty var = transVar(venv, tenv, a->u.assign.var, cur_l, breakl);
			struct expty exp = transExp(venv, tenv, a->u.assign.exp, cur_l, breakl);
			/* If lvalue is of a record type, then exp's result type can be nil */
			if (!match(var.ty, exp.ty))
				EM_error(a->pos, "unmatched assign exp");

			return expTy(Tr_AssignExp(var.exp, exp.exp), Ty_Void());
		}

		case A_ifExp: {
			struct expty test = transExp(venv, tenv, a->u.iff.test, cur_l, breakl);
			if (test.ty->kind != Ty_int)
				EM_error(a->u.iff.test->pos, "Require integer");

			struct expty then = transExp(venv, tenv, a->u.iff.then, cur_l, breakl);
			if (a->u.iff.elsee) {
				struct expty elsee = transExp(venv, tenv, a->u.iff.elsee, cur_l, breakl);
				if (!match(then.ty, elsee.ty))
					EM_error(a->u.iff.then->pos, "then exp and else exp type mismatch");
				//EM_error(a->u.iff.test->pos, "A_ifExp"); //delete
				return expTy(Tr_IfExp(test.exp, then.exp, elsee.exp), then.ty);
			}

			else {
				if (then.ty->kind != Ty_void)
					EM_error(a->u.iff.then->pos, "if-then exp's body must produce no value");

				return expTy(Tr_IfExp(test.exp, then.exp, NULL), Ty_Void());		
			}
		}

		case A_whileExp: {
	    	struct expty test = transExp(venv, tenv, a->u.whilee.test, cur_l, breakl);
	    	if (test.ty->kind != Ty_int)
				EM_error(a->u.whilee.test->pos, "Require integer");

			else {
				Temp_label done = Temp_newlabel();
				S_beginScope(tenv);
				struct expty body = transExp(venv, tenv, a->u.whilee.body, cur_l, done);
				S_endScope(tenv);
				if (body.ty->kind != Ty_void)
					EM_error(a->u.whilee.body->pos, "while body must produce no value");
				else
					return expTy(Tr_WhileExp(test.exp, body.exp, done), Ty_Void());
			}

			return expTy(NULL, Ty_Void());
		}

		case A_forExp: {
			//EM_error(a->u.forr.lo->pos, "forexp"); //delete
			struct expty lo = transExp(venv, tenv, a->u.forr.lo, cur_l, breakl);
			struct expty hi = transExp(venv, tenv, a->u.forr.hi, cur_l, breakl);
	    	if (lo.ty->kind != Ty_int || hi.ty->kind != Ty_int)
				EM_error(a->pos, "for exp's range type is not integer");

			S_beginScope(venv);
	    	/* The id defined by for should be read-only */
			Temp_label done = Temp_newlabel();
			Tr_access acc = Tr_allocLocal(cur_l, FALSE);
			S_enter(venv, a->u.forr.var, E_ROVarEntry(acc, Ty_Int()));
			struct expty body = transExp(venv, tenv, a->u.forr.body, cur_l, done);
			S_endScope(venv);

			if (body.ty->kind != Ty_void)
				EM_error(a->pos, "The body of the for loop should return no type");
	    	
			return expTy(Tr_ForExp(lo.exp, hi.exp, body.exp, done, acc), Ty_Void());
	    }

		case A_breakExp: {
			if (!breakl) {
				EM_error(a->pos, "Can't use break in non for-loop or while-loop");
				return expTy(NULL, Ty_Void());
			}
			return expTy(Tr_BreakExp(breakl), Ty_Void());
		}
	    

		case A_letExp: {
			// TO DO: the result of transdec
			struct expty exp;
			Tr_exp decExp = Tr_NullExp();
			A_decList d;
			S_beginScope(venv);
			S_beginScope(tenv);
			//int count = 0; //delete
			for (d = a->u.let.decs; d; d = d->tail) {
				Tr_exp ele = transDec(venv, tenv, d->head, cur_l, breakl);
				decExp = Tr_LetExp(ele, decExp);
				//printf("let count %d\n", count++);
			}

			//printf("letexp\n");
			exp = transExp(venv, tenv, a->u.let.body, cur_l, breakl);
			Tr_exp r = Tr_LetExp(decExp, exp.exp);
			S_endScope(tenv);
			S_endScope(venv);
			return expTy(r, exp.ty);
		}

		case A_arrayExp: {
			/* struct {S_symbol typ; A_exp size, init;} array; */
			struct expty size = transExp(venv, tenv, a->u.array.size, cur_l, breakl);
			struct expty init = transExp(venv, tenv, a->u.array.init, cur_l, breakl);
			if (size.ty->kind != Ty_int)
				EM_error(a->u.array.init->pos, "The size of the array should be integer");
			
			Ty_ty arr_ty = S_look(tenv, a->u.array.typ);
			if (!arr_ty)
				EM_error(a->pos, "Undefied variable");

			/* TO DO: To much arr_ty */
			if (!match(actual_ty(actual_ty(arr_ty)->u.array), init.ty)) {
				EM_error(a->pos, "type mismatch");
				return expTy(NULL, Ty_Void());
			}
			return expTy(Tr_ArrayExp(size.exp, init.exp), actual_ty(arr_ty));
		}

		assert(0);
	}
}

Tr_exp transDec(S_table venv, S_table tenv, A_dec d, Tr_level cur_l, Temp_label breakl) {
	switch(d->kind) {
		case A_functionDec: {
			A_fundecList f_l;
			for (f_l = d->u.function; f_l; f_l = f_l->tail) {
				/* Check repeatly define */
				A_fundecList f_l_c = f_l->tail;
				for (; f_l_c; f_l_c = f_l_c->tail) {
					if (f_l_c->head->name == f_l->head->name)
						EM_error(f_l->head->pos, "two functions have the same name");		
				}

				A_fundec funcdec = f_l->head;
				Ty_ty result_ty;
				if (funcdec->result == S_Symbol(""))
					result_ty = Ty_Void();

				else
					result_ty = S_look(tenv, funcdec->result);

				Ty_tyList formalTys = makeFormalTyList(tenv, funcdec->params);
				U_boolList escape = makeFunEscape(funcdec->params);
				Tr_level new_l = Tr_newLevel(cur_l, funcdec->name, escape);
				S_enter(venv, funcdec->name, E_FunEntry(new_l, funcdec->name, formalTys, actual_ty(result_ty)));
			}

			for (f_l = d->u.function; f_l; f_l = f_l->tail) {
				S_beginScope(venv);
				A_fundec f = f_l->head;
				A_fieldList l;
				E_enventry e = S_look(venv, f->name);
				Tr_level new_l = e->u.fun.level;
				Tr_accessList acc_l = Tr_TrLevelVar(new_l);
				for (l = f->params; l && acc_l; l = l->tail, acc_l = acc_l->tail)
					S_enter(venv, l->head->name, E_VarEntry(acc_l->head, actual_ty(S_look(tenv, l->head->typ))));

				struct expty body = transExp(venv, tenv, f->body, new_l, breakl);
				if (body.ty->kind != Ty_void && f->result == S_Symbol(""))
					EM_error(f->pos, "procedure returns value");
				S_endScope(venv);
				Tr_procEntryExit(new_l, body.exp,  Tr_TrLevelVar(new_l));
			}
			return Tr_NullExp();
		}

		case A_varDec: {
			/* struct {S_symbol var; S_symbol typ; A_exp init; bool escape;} var; */
			struct expty e = transExp(venv, tenv, d->u.var.init, cur_l, breakl);
			/* var id : type-id := exp */
			Tr_access acc = NULL;
			if (d->u.var.typ != S_Symbol("")) {
				Ty_ty type = S_look(tenv, d->u.var.typ);
				/* Check if the type has been defined */
				if (!type) {
					EM_error(d->pos, "The type is not defined");
					break;
				}
				Ty_ty actual_type = actual_ty(type); 
				if (!match(actual_type, e.ty)) {
					EM_error(d->pos, "type mismatch");
					break;
				}
				acc = Tr_allocLocal(cur_l, d->u.var.escape);
				S_enter(venv, d->u.var.var, E_VarEntry(acc, type));
			}

			/* var id := exp */
			else {
				/* "var a := nil" is illegal */
				if (e.ty->kind == Ty_nil)
					EM_error(d->pos, "init should not be nil without type specified");

				else {
					acc = Tr_allocLocal(cur_l, d->u.var.escape);
					S_enter(venv, d->u.var.var, E_VarEntry(acc, e.ty));
				}
			}
			return Tr_VarDec(acc, e.exp);
		}
		case A_typeDec: {
			/* A_nametyList type; */
			A_nametyList list;
			/* First enter the symbol */
			for (list = d->u.type; list; list = list->tail) {
				/* Check repeat define */
				A_nametyList list_c = list->tail;
				for (; list_c; list_c = list_c->tail) {
					if (list_c->head->name == list->head->name)
						EM_error(list->head->ty->pos, "two types have the same name");
				}

				S_enter(tenv, list->head->name, Ty_Name(list->head->name, NULL));
			}

			for (list = d->u.type; list; list = list->tail) {
				Ty_ty r = transTy(tenv, list->head);
				if (!r)
					return NULL;
				Ty_ty ty = S_look(tenv, list->head->name);
				ty->u.name.ty = r;
			}

			/* Check the recursive definition */
			for (list = d->u.type; list; list = list->tail)
				if (cycleDefine(tenv, list->head)) {
					EM_error(list->head->ty->pos, "illegal type cycle");
					return NULL;
				}

			return Tr_NullExp();
		}
	}
	return NULL;
}

struct expty transVar(S_table venv, S_table tenv, A_var v, Tr_level cur_l, Temp_label breakl) {
	switch(v->kind) {
		case A_simpleVar: {
			E_enventry x = S_look(venv, v->u.simple);
			if (x && x->kind == E_varEntry)
				return expTy(Tr_SimpleVar(x->u.var.access, cur_l), actual_ty(x->u.var.ty));
			else {
				EM_error(v->pos, "undefined variable %s", S_name(v->u.simple));
				return expTy(NULL, Ty_Int());
			}
		}

		case A_fieldVar: {
			struct expty var = transVar(venv, tenv, v->u.field.var, cur_l, breakl);
			if (var.ty->kind != Ty_record) {
				EM_error(v->u.field.var->pos, "not a record type");
				return expTy(NULL, Ty_Int());
			}

			Ty_fieldList f_l;
			int index = 0;
			for (f_l = var.ty->u.record; f_l; f_l = f_l->tail) {
				Ty_field field = f_l->head;
				if (field->name == v->u.field.sym)
					return expTy(Tr_FieldVar(var.exp, index), actual_ty(field->ty));
				index++;
			}

			EM_error(v->u.field.var->pos, "field %s doesn't exist", S_name(v->u.field.sym));
		}

		case A_subscriptVar: {
			struct expty var = transVar(venv, tenv, v->u.subscript.var, cur_l, breakl);
			struct expty exp = transExp(venv, tenv, v->u.subscript.exp, cur_l, breakl);

			if (var.ty->kind != Ty_array) {
				EM_error(v->u.subscript.var->pos, "array type required");
				return expTy(NULL, Ty_Int());
			}

			if (exp.ty->kind != Ty_int) {
				EM_error(v->u.subscript.exp->pos, "integer required");
				return expTy(NULL, Ty_Int());
			}

			return expTy(Tr_SubscriptVar(var.exp, exp.exp), actual_ty(actual_ty(var.ty)->u.array));
		}
	}
}

Ty_ty transTy(S_table tenv, A_namety a) {
	A_ty t = a->ty;
	switch(t->kind) {
		case A_nameTy: {
			Ty_ty ty = S_look(tenv, t->u.name);
			if (!ty) {
				EM_error(t->pos, "The type named %s doesn't exit", S_name(t->u.name));
				return NULL;
			}

			return ty;
		}

		case A_recordTy: {
			Ty_fieldList l = makeTyFieldList(tenv, t->u.record);
			if (!l)
				return NULL;

			return Ty_Record(l);
		}

		case A_arrayTy: {
			Ty_ty ty = S_look(tenv, t->u.array);
			if (!ty) {
				EM_error(t->pos, "The type named %s doesn't exit", S_name(t->u.name));
				return NULL;
			}

			return Ty_Array(ty);
		}
	}
	assert(0);
}