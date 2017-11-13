#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"
#include "types.h"
#include "helper.h"
#include "env.h"
#include "semant.h"

/*Lab4: Your implementation of lab4*/

/* Used in break check */
static int level = 0;

typedef void* Tr_exp;
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

void SEM_transProg(A_exp exp){
    transExp(E_base_venv(), E_base_tenv(), exp);
}

Ty_ty actual_ty(Ty_ty ty) {
	if (ty->kind == Ty_name)
		return ty->u.name.ty;

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

Ty_fieldList construct_field_ty(S_table tenv, A_fieldList recordList, A_pos pos) {
	if (!recordList)
		return NULL;

	Ty_ty record_ty = S_look(tenv, recordList->head->typ);
	if (!record_ty) {
		EM_error(pos, "undefined type %s", S_name(recordList->head->typ));
		return NULL;
	}
	Ty_field ty_field = Ty_Field(recordList->head->name, record_ty);
	return Ty_FieldList(ty_field, construct_field_ty(tenv, recordList->tail, pos));
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

struct expty transExp(S_table venv, S_table tenv, A_exp a) {
	switch(a->kind) {
		case A_varExp: 
			return transVar(venv, tenv, a->u.var);

		case A_nilExp:
			return expTy(NULL, Ty_Nil());

		case A_intExp:
			return expTy(NULL, Ty_Int());

		case A_stringExp:
			return expTy(NULL, Ty_String());

		case A_callExp: {
			E_enventry x = S_look(venv, a->u.call.func);
			if (x && x->kind == E_funEntry) {
				A_expList args = a->u.call.args;
				Ty_tyList formals = x->u.fun.formals;
				for (; args && formals; args = args->tail, formals = formals->tail) {
					struct expty argTy = transExp(venv, tenv, args->head);
					if (!match(formals->head, argTy.ty))
						EM_error(args->head->pos, "para type mismatch");
				}
				if (formals)
						EM_error(a->pos, "too more params in function %s", S_name(a->u.call.func));
				if (args)
						EM_error(a->pos, "too many params in function %s", S_name(a->u.call.func));

				return expTy(NULL, x->u.fun.result);
			}

			else {
				EM_error(a->pos, "undefined function %s", S_name(a->u.call.func));
                return expTy(NULL, Ty_Int());
			}
		}

		case A_opExp: {
			struct expty left = transExp(venv, tenv, a->u.op.left);
			struct expty right = transExp(venv, tenv, a->u.op.right);
			A_oper oper = a->u.op.oper;
			if (oper == A_plusOp || oper == A_minusOp ||
				oper == A_timesOp || oper == A_divideOp ) {

				if (left.ty->kind != Ty_int)
					EM_error(a->u.op.left->pos, "integer required");
				if (right.ty->kind != Ty_int)
					EM_error(a->u.op.right->pos, "integer required");
				return expTy(NULL, Ty_Int());
			}

			else if (oper == A_eqOp || oper == A_neqOp){
				if (!match(left.ty, right.ty)) {
					EM_error(a->pos, "same type required");
                }
                return expTy(NULL, Ty_Int());
			}

			else {
				if (left.ty->kind == Ty_int && right.ty->kind == Ty_int)
					return expTy(NULL, Ty_Int());

				else if (left.ty->kind == Ty_string && right.ty->kind == Ty_string)
					return expTy(NULL, Ty_Int());

				else {
					EM_error(a->pos, "same type required");
					return expTy(NULL, Ty_Int());
				}

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
			A_efieldList efield_list = a->u.record.fields;
			/* Field list from ty */
			Ty_fieldList ty_field_list = act_re_ty->u.record;
			for (; efield_list && ty_field_list; 
				efield_list = efield_list->tail, ty_field_list = ty_field_list->tail) {
				A_efield a_field = efield_list->head;
				Ty_field ty_field = ty_field_list->head;
				struct expty filed_exp = transExp(venv, tenv, a_field->exp);
				if (!match(actual_ty(ty_field->ty), filed_exp.ty))
					EM_error(a_field->exp->pos, "type mismatch");

			}

			if (efield_list || ty_field_list)
				EM_error(a->pos, "the num of record mismatch");

			return expTy(NULL, act_re_ty);
		}

		case A_seqExp: {
			/* Must consider if there is no sequence of expression */
			struct expty r = expTy(NULL, Ty_Void());
			A_expList seq = a->u.seq;
			for (; seq; seq = seq->tail)
				r = transExp(venv, tenv, seq->head);

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

			struct expty var = transVar(venv, tenv, a->u.assign.var);
			struct expty exp = transExp(venv, tenv, a->u.assign.exp);
			/* If lvalue is of a record type, then exp's result type can be nil */
			if (!match(var.ty, exp.ty))
				EM_error(a->pos, "unmatched assign exp");

			return expTy(NULL, Ty_Void());
		}

		case A_ifExp: {
			struct expty test = transExp(venv, tenv, a->u.iff.test);
			if (test.ty->kind != Ty_int)
				EM_error(a->u.iff.test->pos, "Require integer");

			struct expty then = transExp(venv, tenv, a->u.iff.then);
			if (a->u.iff.elsee) {
				struct expty elsee = transExp(venv, tenv, a->u.iff.elsee);
				if (!match(then.ty, elsee.ty))
					EM_error(a->u.iff.then->pos, "then exp and else exp type mismatch");

				return expTy(NULL, then.ty);
			}

			else {
				if (then.ty->kind != Ty_void)
					EM_error(a->u.iff.then->pos, "if-then exp's body must produce no value");

				return expTy(NULL, Ty_Void());		
			}
		}

	    case A_whileExp: {
	    	struct expty test = transExp(venv, tenv, a->u.whilee.test);
	    	if (test.ty->kind != Ty_int)
				EM_error(a->u.whilee.test->pos, "Require integer");

			else {
				level ++;
				S_beginScope(tenv);
				S_enter(venv, S_Symbol("break"), E_BreakEntry(level));
				struct expty body = transExp(venv, tenv, a->u.whilee.body);
				S_endScope(tenv);
				level --;
				if (body.ty->kind != Ty_void)
					EM_error(a->u.whilee.body->pos, "while body must produce no value");
				else
					return expTy(NULL, Ty_Void());
			}

			return expTy(NULL, Ty_Void());
	    }

	    case A_forExp: {
	    	struct expty lo = transExp(venv, tenv, a->u.forr.lo);
	    	struct expty hi = transExp(venv, tenv, a->u.forr.hi);
	    	if (lo.ty->kind != Ty_int || hi.ty->kind != Ty_int)
	    		EM_error(a->pos, "for exp's range type is not integer");

	    	level++;
	    	S_beginScope(venv);
	    	S_enter(venv, S_Symbol("break"), E_BreakEntry(level));
	    	/* The id defined by for should be read-only */
	    	S_enter(venv, a->u.forr.var, E_ROVarEntry(Ty_Int()));
	    	struct expty body = transExp(venv, tenv, a->u.forr.body);
	    	S_endScope(venv);
	    	level--;

	    	if (body.ty->kind != Ty_void)
	    		EM_error(a->pos, "The body of the for loop should return no type");
	    	
	    	return expTy(NULL, Ty_Void());
	    }

	    case A_breakExp: {
	    	E_enventry e = S_look(venv, S_Symbol("break"));
	    	if (!e || e->u.breake.level != level)
	    		EM_error(a->pos, "Break must exist in while and for loop!");
	    	
	    	return expTy(NULL, Ty_Void());
	    }

		case A_letExp: {
			struct expty exp;
			A_decList d;
			S_beginScope(venv);
			S_beginScope(tenv);
			for (d = a->u.let.decs; d; d = d->tail)
				transDec(venv, tenv, d->head);

			exp = transExp(venv, tenv, a->u.let.body);
			S_endScope(tenv);
			S_endScope(venv);
			return exp;
		}

		case A_arrayExp: {
			/* struct {S_symbol typ; A_exp size, init;} array; */
			struct expty size = transExp(venv, tenv, a->u.array.size);
			struct expty init = transExp(venv, tenv, a->u.array.init);
			if (size.ty->kind != Ty_int)
				EM_error(a->u.array.init->pos, "The size of the array should be integer");
			
			Ty_ty arr_ty = S_look(tenv, a->u.array.typ);
			if (!arr_ty)
				EM_error(a->pos, "Undefied variable");

			if (!match(actual_ty(arr_ty->u.array), init.ty)) {
				EM_error(a->pos, "type mismatch");
				return expTy(NULL, Ty_Void());
			}

			return expTy(NULL, actual_ty(arr_ty));
		}

		assert(0);
	}
}

void transDec(S_table venv, S_table tenv, A_dec d) {
	switch(d->kind) {
		case A_functionDec: {
			A_fundecList funcdec_list;
			for (funcdec_list = d->u.function; funcdec_list; funcdec_list = funcdec_list->tail) {
				/* Check repeatly define */
				A_fundecList funcdec_list_c = funcdec_list->tail;
				for (; funcdec_list_c; funcdec_list_c = funcdec_list_c->tail) {
					if (funcdec_list_c->head->name == funcdec_list->head->name)
						EM_error(funcdec_list->head->pos, "two functions have the same name");		
				}

				A_fundec funcdec = funcdec_list->head;
				Ty_ty result_ty;
				if (funcdec->result == S_Symbol(""))
					result_ty = Ty_Void();

				else
					result_ty = S_look(tenv, funcdec->result);

				Ty_tyList formalTys = makeFormalTyList(tenv, funcdec->params);
				S_enter(venv, funcdec->name, E_FunEntry(formalTys, actual_ty(result_ty)));
			}

			for (funcdec_list = d->u.function; funcdec_list; funcdec_list = funcdec_list->tail) {
				S_beginScope(venv);
				A_fundec f = funcdec_list->head;
				A_fieldList l;
				for (l = f->params; l; l = l->tail)
					S_enter(venv, l->head->name, E_VarEntry(actual_ty(S_look(tenv, l->head->typ))));

				struct expty body_expty = transExp(venv, tenv, f->body);
				if (body_expty.ty->kind != Ty_void && f->result == S_Symbol(""))
					EM_error(f->pos, "procedure returns value");
				S_endScope(venv);
			}

			break;
		}

		case A_varDec: {
			/* struct {S_symbol var; S_symbol typ; A_exp init; bool escape;} var; */
			struct expty e = transExp(venv, tenv, d->u.var.init);
			/* var id : type-id := exp */
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

				S_enter(venv, d->u.var.var, E_VarEntry(type));
			}

			/* var id := exp */
			else {
				/* "var a := nil" is illegal */
				if (e.ty->kind == Ty_nil)
					EM_error(d->pos, "init should not be nil without type specified");

				else
					S_enter(venv, d->u.var.var, E_VarEntry(e.ty));
			}

			break;
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

			/* Calculate the num of Ty_name, and initialize the dec of record and array */
			int name_dec_loop = 0;
			for (list = d->u.type; list; list = list->tail) {
				A_ty dec_ty = list->head->ty;
				switch(dec_ty->kind) {
					case A_nameTy: {
						/* type l_ty = r_ty */
						Ty_ty l_ty = S_look(tenv, list->head->name);
						Ty_ty r_ty = S_look(tenv, dec_ty->u.name);
						if (!r_ty) {
							EM_error(dec_ty->pos, "The type named %s doesn't exit", S_name(dec_ty->u.name));
							return;
						}
						l_ty->u.name.ty = r_ty;
						l_ty->u.name.sym = list->head->name;
						name_dec_loop ++;
						break;
					}

					case A_recordTy: {
						Ty_ty ty = S_look(tenv, list->head->name);
						ty->u.record = construct_field_ty(tenv, dec_ty->u.record, dec_ty->pos);
						ty->kind = Ty_record;
						break;
					}

					case A_arrayTy: {
						Ty_ty ty = S_look(tenv, list->head->name);
						/* The type of the element of the array */
						Ty_ty ele_ty = S_look(tenv, dec_ty->u.array);

						if (!ele_ty) {
							EM_error(dec_ty->pos, "The type named %s doesn't exist", S_name(dec_ty->u.array));
							return;
						}

						ty->kind = Ty_array;
						ty->u.array = ele_ty;
						break;
					}
				}
			}

			/* Check the recursive definition */
			for (list = d->u.type; list; list = list->tail) {
				if (list->head->ty->kind != A_nameTy)
					continue;
				/* The definition must be name_ty */
				int cur_name_loop = name_dec_loop;
				/* type b_name = r_name */
				S_symbol b_name = list->head->name;
				S_symbol r_name = list->head->ty->u.name;
				while (1) {
					if (cur_name_loop == 0) {
						EM_error(list->head->ty->pos, "illegal type cycle");
						return;
					}

					Ty_ty r_ty = S_look(tenv, r_name);
					if (r_ty->kind != Ty_name) {
						Ty_ty look_ty = S_look(tenv, b_name);
						look_ty->u.name.ty = r_ty;
						name_dec_loop--;
						break;
					} 
					else {
						r_name = r_ty->u.name.sym;
						cur_name_loop--;
					}
				}
			}

			break;
		}
	}
}

struct expty transVar(S_table venv, S_table tenv, A_var v) {
	switch(v->kind) {
		case A_simpleVar: {
			E_enventry x = S_look(venv, v->u.simple);
			if (x && x->kind == E_varEntry)
				return expTy(NULL, actual_ty(x->u.var.ty));
			else {
				EM_error(v->pos, "undefined variable %s", S_name(v->u.simple));
				return expTy(NULL, Ty_Int());
			}
		}

		case A_fieldVar: {
			struct expty var_ty = transVar(venv, tenv, v->u.field.var);
			if (var_ty.ty->kind != Ty_record) {
				EM_error(v->u.field.var->pos, "not a record type");
				return expTy(NULL, Ty_Int());
			}

			Ty_fieldList field_list;
			for (field_list = var_ty.ty->u.record; field_list; field_list = field_list->tail) {
				Ty_field field = field_list->head;
				if (field->name == v->u.field.sym)
					return expTy(NULL, actual_ty(field->ty));
			}

			EM_error(v->u.field.var->pos, "field %s doesn't exist", S_name(v->u.field.sym));
		}

		case A_subscriptVar: {
			struct expty var_ty = transVar(venv, tenv, v->u.subscript.var);
			struct expty exp_ty = transExp(venv, tenv, v->u.subscript.exp);
			if (var_ty.ty->kind != Ty_array) {
				EM_error(v->u.subscript.var->pos, "array type required");
				return expTy(NULL, Ty_Int());
			}

			if (exp_ty.ty->kind != Ty_int) {
				EM_error(v->u.subscript.exp->pos, "integer required");
				return expTy(NULL, Ty_Int());
			}

			return expTy(NULL, actual_ty(var_ty.ty));
		}
	}
}

void transTy(S_table tenv, A_namety n) {
	/* empty */
}
