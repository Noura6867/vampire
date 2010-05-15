/**
 * @file ArithmeticIndex.cpp
 * Implements class ArithmeticIndex.
 */

#include "../Debug/Assertion.hpp"
#include "../Debug/Tracer.hpp"

#include "../Lib/Environment.hpp"

#include "../Kernel/BDD.hpp"
#include "../Kernel/Signature.hpp"

#include "../Shell/Options.hpp"

#include "ArithmeticIndex.hpp"

namespace Indexing
{

using namespace Kernel;

ConstraintDatabase::ConstraintDatabase()
{
  theory=Theory::instance();
}

bool ConstraintDatabase::isNonEqual(TermList t, InterpretedType val, Clause*& premise)
{
  CALL("ConstraintDatabase::isNonEqual");

  ConstraintInfo* ci;
  if(!_termConstraints.find(t, ci)) {
    return false;
  }
  ASS(ci);
  if( ci->hasLowerLimit && (ci->lowerLimit > val ||
      (ci->strongLowerLimit && ci->lowerLimit==val) ) ) {
    premise=ci->lowerLimitPremise;
    return true;
  }
  if( ci->hasUpperLimit && (ci->upperLimit < val ||
      (ci->strongUpperLimit && ci->upperLimit==val) ) ) {
    premise=ci->upperLimitPremise;
    return true;
  }
  return false;
}

bool ConstraintDatabase::isGreater(TermList t, InterpretedType val, Clause*& premise)
{
  CALL("ConstraintDatabase::isGreater");

  ConstraintInfo* ci;
  if(!_termConstraints.find(t, ci)) {
    return false;
  }
  ASS(ci);
  if( ci->hasLowerLimit && (ci->lowerLimit > val ||
      (ci->strongLowerLimit && ci->lowerLimit==val) ) ) {
    premise=ci->lowerLimitPremise;
    return true;
  }
  return false;
}


void ConstraintDatabase::handleLiteral(Literal* lit, bool adding, Clause* premise, bool negative)
{
  CALL("ConstraintDatabase::handleLiteral");

  Signature::Symbol* sym0=env.signature->getPredicate(lit->functor());

  if(lit->arity()!=2 || !sym0->interpreted()) {
    return;
  }
  Signature::InterpretedSymbol* sym=static_cast<Signature::InterpretedSymbol*>(sym0);
  Interpretation itp=sym->getInterpretation();
  if(itp!=Theory::GREATER) {
    return;
  }

  TermList arg;
  InterpretedType num;
  bool numFirst=theory->isInterpretedConstant(*lit->nthArgument(0));
  if(numFirst) {
    //if both arguments were numbers, the predicate would have been simplified
    if(theory->isInterpretedConstant(*lit->nthArgument(1))) {
      //comparison of two interpreted constants is of no use to us
      return;
    }
    num=theory->interpretConstant(*lit->nthArgument(0));
    arg=*lit->nthArgument(1);
  }
  else {
    if(!theory->isInterpretedConstant(*lit->nthArgument(1))) {
      //we don't have a comparison with a number
      return;
    }
    num=theory->interpretConstant(*lit->nthArgument(1));
    arg=*lit->nthArgument(0);
  }

  bool litPositive = static_cast<bool>(lit->polarity()) ^ negative;
  bool strong = litPositive;
  bool upper = !(numFirst ^ litPositive);

  if(adding) {
    ConstraintInfo** pctr;
    if(_termConstraints.getValuePtr(arg, pctr)) {
      *pctr=new ConstraintInfo;
    }
    ConstraintInfo* ctr=*pctr;
    if(upper) {
      if( !ctr->hasUpperLimit || ctr->upperLimit > num ||
	  (strong && !ctr->strongUpperLimit && ctr->upperLimit==num) ) {
	ctr->hasUpperLimit=true;
	ctr->strongUpperLimit=strong;
	ctr->upperLimit=num;
	ctr->upperLimitPremise=premise;
      }
    }
    else {
      if( !ctr->hasLowerLimit || ctr->lowerLimit < num ||
	  (strong && !ctr->strongLowerLimit && ctr->lowerLimit==num) ) {
	ctr->hasLowerLimit=true;
	ctr->strongLowerLimit=strong;
	ctr->lowerLimit=num;
	ctr->lowerLimitPremise=premise;
      }
    }
  }
  else {
    ConstraintInfo* ctr=_termConstraints.get(arg);
    if(upper) {
      if(premise==ctr->upperLimitPremise) {
	ctr->hasUpperLimit=false;
      }
    }
    else {
      if(premise==ctr->lowerLimitPremise) {
	ctr->hasLowerLimit=false;
      }
    }
  }
}

ArithmeticIndex::ArithmeticIndex()
{
}

void ArithmeticIndex::handleClause(Clause* c, bool adding)
{
  CALL("ArithmeticIndex::handleClause");
  ASS(env.options->interpretedEvaluation()); //this index should be used only when we interpret symbols

  if(c->length()!=1 || !BDD::instance()->isFalse(c->prop())) {
    return;
  }

  Literal* lit=(*c)[0];
  _db.handleLiteral(lit, adding, c);
}

}
