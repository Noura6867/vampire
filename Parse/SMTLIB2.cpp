/**
 * @file SMTLIB.cpp
 * Implements class SMTLIB.
 */

#include <climits>
#include <fstream>

#include "Lib/Environment.hpp"
#include "Lib/NameArray.hpp"
#include "Lib/StringUtils.hpp"
#include "Kernel/Clause.hpp"
#include "Kernel/ColorHelper.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/Inference.hpp"
#include "Kernel/Signature.hpp"
#include "Kernel/SortHelper.hpp"

#include "Shell/LispLexer.hpp"
#include "Shell/Options.hpp"
#include "Shell/SMTLIBLogic.hpp"

#include "SMTLIB2.hpp"

#include "TPTP.hpp"

#undef LOGGING
#define LOGGING 0

#if LOGGING
#define LOG1(arg) cout << arg << endl;
#define LOG2(a1,a2) cout << a1 << a2 << endl;
#define LOG3(a1,a2,a3) cout << a1 << a2 << a3 << endl;
#define LOG4(a1,a2,a3,a4) cout << a1 << a2 << a3 << a4 << endl;
#else
#define LOG1(arg)
#define LOG2(a1,a2)
#define LOG3(a1,a2,a3)
#define LOG4(a1,a2,a3,a4)
#endif

namespace Parse {

SMTLIB2::SMTLIB2(const Options& opts)
: _logicSet(false),
  _logic(SMT_UNDEFINED),
  _numeralsAreReal(false),
  _formulas(nullptr)
{
  CALL("SMTLIB2::SMTLIB2");
}

void SMTLIB2::parse(istream& str)
{
  CALL("SMTLIB2::parse(istream&)");

  LispLexer lex(str);
  LispParser lpar(lex);
  LExpr* expr = lpar.parse();
  parse(expr);
}

void SMTLIB2::parse(LExpr* bench)
{
  CALL("SMTLIB2::parse(LExpr*)");

  ASS(bench->isList());
  readBenchmark(bench->list);
}

void SMTLIB2::readBenchmark(LExprList* bench)
{
  CALL("SMTLIB2::readBenchmark");
  LispListReader bRdr(bench);

  // iteration over benchmark top level entries
  while(bRdr.hasNext()){
    LExpr* lexp = bRdr.next();

    LOG2("readBenchmark ",lexp->toString(true));

    LispListReader ibRdr(lexp);

    if (ibRdr.tryAcceptAtom("set-logic")) {
      if (_logicSet) {
        USER_ERROR("set-logic can appear only once in a problem");
      }
      readLogic(ibRdr.readAtom());
      ibRdr.acceptEOL();
      continue;
    }

    if (ibRdr.tryAcceptAtom("set-info")) {

      if (ibRdr.tryAcceptAtom(":status")) {
        _statusStr = ibRdr.readAtom();
        ibRdr.acceptEOL();
        continue;
      }

      if (ibRdr.tryAcceptAtom(":source")) {
        _sourceInfo = ibRdr.readAtom();
        ibRdr.acceptEOL();
        continue;
      }

      // ignore unknown info
      ibRdr.readAtom();
      ibRdr.readAtom();
      ibRdr.acceptEOL();
      continue;
    }

    if (ibRdr.tryAcceptAtom("declare-sort")) {
      vstring name = ibRdr.readAtom();
      vstring arity = ibRdr.readAtom();

      readDeclareSort(name,arity);

      ibRdr.acceptEOL();

      continue;
    }

    if (ibRdr.tryAcceptAtom("define-sort")) {
      vstring name = ibRdr.readAtom();
      LExprList* args = ibRdr.readList();
      LExpr* body = ibRdr.readNext();

      readDefineSort(name,args,body);

      ibRdr.acceptEOL();

      continue;
    }

    if (ibRdr.tryAcceptAtom("declare-fun")) {
      vstring name = ibRdr.readAtom();
      LExprList* iSorts = ibRdr.readList();
      LExpr* oSort = ibRdr.readNext();

      readDeclareFun(name,iSorts,oSort);

      ibRdr.acceptEOL();

      continue;
    }

    if (ibRdr.tryAcceptAtom("declare-datatypes")) {
      LExprList* sorts = ibRdr.readList();
      LExprList* datatypes = ibRdr.readList();

      readDeclareDatatypes(sorts, datatypes, false);

      ibRdr.acceptEOL();

      continue;
    }

    if (ibRdr.tryAcceptAtom("declare-codatatypes")) {
      LExprList* sorts = ibRdr.readList();
      LExprList* datatypes = ibRdr.readList();

      readDeclareDatatypes(sorts, datatypes, true);

      ibRdr.acceptEOL();

      continue;
    }
    
    if (ibRdr.tryAcceptAtom("declare-const")) {
      vstring name = ibRdr.readAtom();
      LExpr* oSort = ibRdr.readNext();

      readDeclareFun(name,nullptr,oSort);

      ibRdr.acceptEOL();

      continue;
    }

    if (ibRdr.tryAcceptAtom("define-fun")) {
      vstring name = ibRdr.readAtom();
      LExprList* iArgs = ibRdr.readList();
      LExpr* oSort = ibRdr.readNext();
      LExpr* body = ibRdr.readNext();

      readDefineFun(name,iArgs,oSort,body);

      ibRdr.acceptEOL();

      continue;
    }

    if (ibRdr.tryAcceptAtom("assert")) {
      readAssert(ibRdr.readNext());

      ibRdr.acceptEOL();

      continue;
    }

    if (ibRdr.tryAcceptAtom("check-sat")) {
      if (bRdr.hasNext()) {
        LispListReader exitRdr(bRdr.readList());
        if (!exitRdr.tryAcceptAtom("exit")) {
          if(env.options->mode()!=Options::Mode::SPIDER) {
            env.beginOutput();
            env.out() << "% Warning: check-sat is not the last entry. Skipping the rest!" << endl;
            env.endOutput();
          }
        }
      }
      break;
    }

    if (ibRdr.tryAcceptAtom("exit")) {
      bRdr.acceptEOL();
      break;
    }

    USER_ERROR("unrecognized entry "+ibRdr.readAtom());
  }
}

//  ----------------------------------------------------------------------

const char * SMTLIB2::s_smtlibLogicNameStrings[] = {
    "ALIA",
    "AUFLIA",
    "AUFLIRA",
    "AUFNIRA",
    "BV",
    "LIA",
    "LRA",
    "NIA",
    "NRA",
    "QF_ABV",
    "QF_ALIA",
    "QF_ANIA",
    "QF_AUFBV",
    "QF_AUFLIA",
    "QF_AUFNIA",
    "QF_AX",
    "QF_BV",
    "QF_IDL",
    "QF_LIA",
    "QF_LIRA",
    "QF_LRA",
    "QF_NIA",
    "QF_NIRA",
    "QF_NRA",
    "QF_RDL",
    "QF_UF",
    "QF_UFBV",
    "QF_UFIDL",
    "QF_UFLIA",
    "QF_UFLRA",
    "QF_UFNIA",
    "QF_UFNRA",
    "UF",
    "UFBV",
    "UFIDL",
    "UFLIA",
    "UFLRA",
    "UFNIA"
};

SMTLIBLogic SMTLIB2::getLogicFromString(const vstring& str)
{
  CALL("SMTLIB2::getLogicFromString");

  static NameArray smtlibLogicNames(s_smtlibLogicNameStrings, sizeof(s_smtlibLogicNameStrings)/sizeof(char*));
  ASS_EQ(smtlibLogicNames.length, SMT_UNDEFINED);

  int res = smtlibLogicNames.tryToFind(str.c_str());
  if(res==-1) {
    return SMT_UNDEFINED;
  }
  return static_cast<SMTLIBLogic>(res);
}

void SMTLIB2::readLogic(const vstring& logicStr)
{
  CALL("SMTLIB2::checkLogic");

  _logic = getLogicFromString(logicStr);
  _logicSet = true;

  switch (_logic) {
  case SMT_ALIA:
  case SMT_AUFLIA:
  case SMT_AUFLIRA:
  case SMT_AUFNIRA:
  case SMT_LIA:
  case SMT_NIA:
  case SMT_QF_ALIA:
  case SMT_QF_ANIA:
  case SMT_QF_AUFLIA:
  case SMT_QF_AUFNIA:
  case SMT_QF_AX:
  case SMT_QF_IDL:
  case SMT_QF_LIA:
  case SMT_QF_LIRA:
  case SMT_QF_NIA:
  case SMT_QF_NIRA:
  case SMT_QF_UF:
  case SMT_QF_UFIDL:
  case SMT_QF_UFLIA:
  case SMT_QF_UFNIA:
  case SMT_UF:
  case SMT_UFIDL:
  case SMT_UFLIA:
  case SMT_UFNIA:
    break;

  // pure real arithmetic theories treat decimals as Real constants
  case SMT_LRA:
  case SMT_NRA:
  case SMT_QF_LRA:
  case SMT_QF_NRA:
  case SMT_QF_RDL:
  case SMT_QF_UFLRA:
  case SMT_QF_UFNRA:
  case SMT_UFLRA:
    _numeralsAreReal = true;
    break;

  // we don't support bit vectors
  case SMT_BV:
  case SMT_QF_ABV:
  case SMT_QF_AUFBV:
  case SMT_QF_BV:
  case SMT_QF_UFBV:
  case SMT_UFBV:
    USER_ERROR("unsupported logic "+logicStr);
  default:
    USER_ERROR("unrecognized logic "+logicStr);
  }

}

//  ----------------------------------------------------------------------

const char * SMTLIB2::s_builtInSortNameStrings[] = {
    "Array",
    "Bool",
    "Int",
    "Real"
};

SMTLIB2::BuiltInSorts SMTLIB2::getBuiltInSortFromString(const vstring& str)
{
  CALL("SMTLIB::getBuiltInSortFromString");

  static NameArray builtInSortNames(s_builtInSortNameStrings, sizeof(s_builtInSortNameStrings)/sizeof(char*));
  ASS_EQ(builtInSortNames.length, BS_INVALID);

  int res = builtInSortNames.tryToFind(str.c_str());
  if(res==-1) {
    return BS_INVALID;
  }
  return static_cast<BuiltInSorts>(res);
}

bool SMTLIB2::isAlreadyKnownSortSymbol(const vstring& name)
{
  CALL("SMTLIB::isAlreadyKnownSortSymbol");

  if (getBuiltInSortFromString(name) != BS_INVALID) {
    return true;
  }

  if (_declaredSorts.find(name)) {
    return true;
  }

  if (_sortDefinitions.find(name)) {
    return true;
  }

  return false;
}

void SMTLIB2::readDeclareSort(const vstring& name, const vstring& arity)
{
  CALL("SMTLIB2::readDeclareSort");

  if (isAlreadyKnownSortSymbol(name)) {
    USER_ERROR("Redeclaring built-in, declared or defined sort symbol: "+name);
  }

  if (not StringUtils::isPositiveInteger(arity)) {
    USER_ERROR("Unrecognized declared sort arity: "+arity);
  }

  unsigned val;
  if (!Int::stringToUnsignedInt(arity, val)) {
    USER_ERROR("Couldn't convert sort arity: "+arity);
  }

  ALWAYS(_declaredSorts.insert(name,val));
}

void SMTLIB2::readDefineSort(const vstring& name, LExprList* args, LExpr* body)
{
  CALL("SMTLIB2::readDefineSort");

  if (isAlreadyKnownSortSymbol(name)) {
    USER_ERROR("Redeclaring built-in, declared or defined sort symbol: "+name);
  }

  // here we could check the definition for well-formed-ness
  // current solution: crash only later, at application site

  ALWAYS(_sortDefinitions.insert(name,SortDefinition(args,body)));
}

//  ----------------------------------------------------------------------

/**
 * SMTLIB sort expression turned into vampire sort id.
 *
 * Taking into account built-in sorts, declared sorts and sort definitions.
 */
unsigned SMTLIB2::declareSort(LExpr* sExpr)
{
  CALL("SMTLIB2::declareSort");

  enum SortParseOperation {
    SPO_PARSE,
    SPO_POP_LOOKUP,
    SPO_CHECK_ARITY
  };
  static Stack<pair<SortParseOperation,LExpr*> > todo;
  ASS(todo.isEmpty());

  ASS_EQ(Sorts::SRT_DEFAULT,0); // there is no default sort in smtlib, so we can use 0 as a results separator
  static const int SEPARATOR = 0;
  static Stack<unsigned> results;
  ASS(results.isEmpty());

  // evaluation contexts for the expansion of sort definitions
  typedef DHMap<vstring,unsigned> SortLookup;
  static Stack<SortLookup*> lookups;
  ASS(lookups.isEmpty());

  // to store defined sort's identifier when expanding its definition
  // (for preventing circular non-sense)
  static Stack<vstring> forbidden;
  ASS(forbidden.isEmpty());

  todo.push(make_pair(SPO_PARSE,sExpr));

  while (todo.isNonEmpty()) {
    pair<SortParseOperation,LExpr*> cur = todo.pop();
    SortParseOperation op = cur.first;

    if (op == SPO_POP_LOOKUP) {
      delete lookups.pop();
      forbidden.pop();
      continue;
    }

    if (op == SPO_CHECK_ARITY) {
      if (results.size() < 2) {
        goto malformed;
      }
      unsigned true_result = results.pop();
      unsigned separator   = results.pop();

      if (true_result == SEPARATOR || separator != SEPARATOR) {
        goto malformed;
      }
      results.push(true_result);

      continue;
    }

    ASS_EQ(op,SPO_PARSE);
    LExpr* exp = cur.second;

    if (exp->isList()) {
      LExprList::Iterator lIt(exp->list);

      todo.push(make_pair(SPO_CHECK_ARITY,nullptr));
      results.push(SEPARATOR);

      while (lIt.hasNext()) {
        todo.push(make_pair(SPO_PARSE,lIt.next()));
      }
    } else {
      ASS(exp->isAtom());
      vstring& id = exp->str;

      // try (top) context lookup
      if (lookups.isNonEmpty()) {
        SortLookup* lookup = lookups.top();
        unsigned res;
        if (lookup->find(id,res)) {
          results.push(res);
          continue;
        }
      }

      {
        for (unsigned i = 0; i < forbidden.size(); i++) {
          if (id == forbidden[i]) {
            USER_ERROR("Expanding circular sort definition "+ id);
          }
        }
      }

      // try declared sorts
      unsigned arity;
      if (_declaredSorts.find(id,arity)) {
        // building an arbitrary but unique sort string
        // TODO: this may not be good enough for a tptp-compliant output!
        vstring sortName = id + "(";
        while (arity--) {
          if (results.isEmpty() || results.top() == SEPARATOR) {
            goto malformed;
          }
          sortName += Int::toString(results.pop());
          if (arity) {
            sortName += ",";
          }
        }
        sortName += ")";

        unsigned newSort = env.sorts->addSort(sortName);
        results.push(newSort);

        continue;
      }

      // try defined sorts
      SortDefinition def;
      if (_sortDefinitions.find(id,def)) {
        SortLookup* lookup = new SortLookup();

        LispListReader argRdr(def.args);

        while (argRdr.hasNext()) {
          if (results.isEmpty() || results.top() == SEPARATOR) {
            goto malformed;
          }
          unsigned argSort = results.pop();
          const vstring& argName = argRdr.readAtom();
          // TODO: could check if the same string names more than one argument positions
          // the following just takes the first and ignores the others
          lookup->insert(argName,argSort);
        }

        lookups.push(lookup);
        forbidden.push(id);

        todo.push(make_pair(SPO_POP_LOOKUP,nullptr)); //schedule lookup deletion (see above)
        todo.push(make_pair(SPO_PARSE,def.body));

        continue;
      }

      // try built-ins
      BuiltInSorts bs = getBuiltInSortFromString(id);
      switch (bs) {
        case BS_BOOL:
          results.push(Sorts::SRT_BOOL);
          continue;
        case BS_INT:
          results.push(Sorts::SRT_INTEGER);
          continue;
        case BS_REAL:
          results.push(Sorts::SRT_REAL);
          continue;
        case BS_ARRAY:
          if (results.size() < 2) {
            goto malformed;
          } else {
            unsigned indexSort = results.pop();
            unsigned innerSort = results.pop();
            if (indexSort == SEPARATOR || innerSort == SEPARATOR) {
              goto malformed;
            }
            results.push(env.sorts->addArraySort(indexSort,innerSort));
            continue;
          }

        default:
          ASS_EQ(bs,BS_INVALID);
      }

      USER_ERROR("Unrecognized sort identifier "+id);
    }
  }

  if (results.size() == 1) {
    return results.pop();
  } else {
    malformed:
    USER_ERROR("Malformed type expression "+sExpr->toString());
  }
}

static const char* EXISTS = "exists";
static const char* FORALL = "forall";

const char * SMTLIB2::s_formulaSymbolNameStrings[] = {
    "<",
    "<=",
    "=",
    "=>",
    ">",
    ">=",
    "and",
    "distinct",
    EXISTS,
    "false",
    FORALL,
    "is_int",
    "not",
    "or",
    "true",
    "xor"
};

SMTLIB2::FormulaSymbol SMTLIB2::getBuiltInFormulaSymbol(const vstring& str)
{
  CALL("SMTLIB::getFormulaSymbol");

  static NameArray formulaSymbolNames(s_formulaSymbolNameStrings, sizeof(s_formulaSymbolNameStrings)/sizeof(char*));
  ASS_EQ(formulaSymbolNames.length, FS_USER_PRED_SYMBOL);

  int res = formulaSymbolNames.tryToFind(str.c_str());
  if(res==-1) {
    return FS_USER_PRED_SYMBOL;
  }
  return static_cast<FormulaSymbol>(res);
}

static const char* LET = "let";

const char * SMTLIB2::s_termSymbolNameStrings[] = {
    "*",
    "+",
    "-",
    "/",
    "abs",
    "div",
    "ite",
    LET,
    "mod",
    "select",
    "store",
    "to_int",
    "to_real"
};

SMTLIB2::TermSymbol SMTLIB2::getBuiltInTermSymbol(const vstring& str)
{
  CALL("SMTLIB::getTermSymbol");

  static NameArray termSymbolNames(s_termSymbolNameStrings, sizeof(s_termSymbolNameStrings)/sizeof(char*));
  ASS_EQ(termSymbolNames.length, TS_USER_FUNCTION);

  int resInt = termSymbolNames.tryToFind(str.c_str());
  if(resInt==-1) {
    return TS_USER_FUNCTION;
  }
  return static_cast<TermSymbol>(resInt);
}

bool SMTLIB2::isAlreadyKnownFunctionSymbol(const vstring& name)
{
  CALL("SMTLIB2::isAlreadyKnownFunctionSymbol");

  if (getBuiltInFormulaSymbol(name) != FS_USER_PRED_SYMBOL) {
    return true;
  }

  if (getBuiltInTermSymbol(name) != TS_USER_FUNCTION) {
    return true;
  }

  if (_declaredFunctions.find(name)) {
    return true;
  }

  return false;
}

void SMTLIB2::readDeclareFun(const vstring& name, LExprList* iSorts, LExpr* oSort)
{
  CALL("SMTLIB2::readDeclareFun");

  if (isAlreadyKnownFunctionSymbol(name)) {
    USER_ERROR("Redeclaring function symbol: "+name);
  }

  unsigned rangeSort = declareSort(oSort);

  LispListReader isRdr(iSorts);

  static Stack<unsigned> argSorts;
  argSorts.reset();

  while (isRdr.hasNext()) {
    argSorts.push(declareSort(isRdr.next()));
  }

  declareFunctionOrPredicate(name,rangeSort,argSorts);
}

SMTLIB2::DeclaredFunction SMTLIB2::declareFunctionOrPredicate(const vstring& name, signed rangeSort, const Stack<unsigned>& argSorts)
{
  CALL("SMTLIB2::declareFunctionOrPredicate");

  bool added;
  unsigned symNum;
  Signature::Symbol* sym;
  BaseType* type;

  if (rangeSort == Sorts::SRT_BOOL) { // predicate
    symNum = env.signature->addPredicate(name, argSorts.size(), added);

    sym = env.signature->getPredicate(symNum);

    type = new PredicateType(argSorts.size(),argSorts.begin());

    LOG1("declareFunctionOrPredicate-Predicate");
  } else { // proper function
    if (argSorts.size() > 0) {
      symNum = env.signature->addFunction(name, argSorts.size(), added);
    } else {
      symNum = TPTP::addUninterpretedConstant(name,_overflow,added);
    }

    sym = env.signature->getFunction(symNum);

    type = new FunctionType(argSorts.size(), argSorts.begin(), rangeSort);

    LOG1("declareFunctionOrPredicate-Function");
  }

  ASS(added);
  sym->setType(type);

  DeclaredFunction res = make_pair(symNum,type->isFunctionType());

  LOG2("declareFunctionOrPredicate -name ",name);
  LOG2("declareFunctionOrPredicate -symNum ",symNum);

  ALWAYS(_declaredFunctions.insert(name,res));

  return res;
}

//  ----------------------------------------------------------------------

void SMTLIB2::readDefineFun(const vstring& name, LExprList* iArgs, LExpr* oSort, LExpr* body)
{
  CALL("SMTLIB2::readDefineFun");

  if (isAlreadyKnownFunctionSymbol(name)) {
    USER_ERROR("Redeclaring function symbol: "+name);
  }

  unsigned rangeSort = declareSort(oSort);

  _nextVar = 0;
  ASS(_scopes.isEmpty());
  TermLookup* lookup = new TermLookup();

  static Stack<unsigned> argSorts;
  argSorts.reset();

  static Stack<TermList> args;
  args.reset();

  LispListReader iaRdr(iArgs);
  while (iaRdr.hasNext()) {
    LExprList* pair = iaRdr.readList();
    LispListReader pRdr(pair);

    vstring vName = pRdr.readAtom();
    unsigned vSort = declareSort(pRdr.readNext());

    pRdr.acceptEOL();

    TermList arg = TermList(_nextVar++, false);
    args.push(arg);

    if (!lookup->insert(vName,make_pair(arg,vSort))) {
      USER_ERROR("Multiple occurrence of variable "+vName+" in the definition of function "+name);
    }

    argSorts.push(vSort);
  }

  _scopes.push(lookup);

  ParseResult res = parseTermOrFormula(body);

  delete _scopes.pop();

  TermList rhs;
  if (res.asTerm(rhs) != rangeSort) {
    USER_ERROR("Defined function body "+body->toString()+" has different sort than declared "+oSort->toString());
  }

  // Only after parsing, so that the definition cannot be recursive
  DeclaredFunction fun = declareFunctionOrPredicate(name,rangeSort,argSorts);

  unsigned symbIdx = fun.first;
  bool isTrueFun = fun.second;

  TermList lhs;
  if (isTrueFun) {
    lhs = TermList(Term::create(symbIdx,args.size(),args.begin()));
  } else {
    Formula* frm = new AtomicFormula(Literal::create(symbIdx,args.size(),true,false,args.begin()));
    lhs = TermList(Term::createFormula(frm));
  }

  Formula* fla = new AtomicFormula(Literal::createEquality(true,lhs,rhs,rangeSort));

  FormulaUnit* fu = new FormulaUnit(fla, new Inference(Inference::INPUT), Unit::ASSUMPTION);

  UnitList::push(fu, _formulas);
}

void SMTLIB2::readDeclareDatatypes(LExprList* sorts, LExprList* datatypes, bool codatatype)
{
  CALL("SMTLIB2::readDeclareDatatypes");
  
  if (sorts->length() > 0) {
    USER_ERROR("unsupported parametric datatypes declaration");
  }

  // first declare all the sorts, and then only the constructors, in
  // order to allow mutually recursive datatypes definitions
  LispListReader dtypesRdr(datatypes);
  while (dtypesRdr.hasNext()) {
    LispListReader dtypeRdr(dtypesRdr.readList());

    const vstring& dtypeName = dtypeRdr.readAtom();
    if (isAlreadyKnownSortSymbol(dtypeName)) {
      USER_ERROR("Redeclaring built-in, declared or defined sort symbol as datatype: "+dtypeName);
    }

    ALWAYS(_declaredSorts.insert(dtypeName, 0));
    bool added;
    env.sorts->addSort(dtypeName + "()", added);
    ASS(added);
  }

  List<Signature::TermAlgebra*>* algebras = nullptr;

  LispListReader dtypesRdr2(datatypes);
  while(dtypesRdr2.hasNext()) {
    LispListReader dtypeRdr(dtypesRdr2.readList());
    const vstring& sortName = dtypeRdr.readAtom() + "()";
    bool added;
    Signature::TermAlgebra* ta = new Signature::TermAlgebra(sortName, env.sorts->addSort(sortName, added));
    ASS(!added);

    while (dtypeRdr.hasNext()) {
      // read each constructor declaration
      LExpr *constr = dtypeRdr.next();
      if (constr->isAtom()) {
        // atom, construtor of arity 0
        ta->addConstr(constr->str);
      } else {
        ASS(constr->isList());
        LispListReader constrRdr(constr);
        ta->addConstr(constrRdr.readAtom());

        while (constrRdr.hasNext()) {
          LExpr *arg = constrRdr.next();
          LispListReader argRdr(arg);
          const vstring& destructorName = argRdr.readAtom();
          unsigned argSort = declareSort(argRdr.next());
          if (argRdr.hasNext()) {
            USER_ERROR("Bad constructor argument:" + arg->toString());
          }
          ta->addConstrArg(destructorName, argSort);
        }
      }
    }
    algebras = algebras->cons(ta);
  }

  List<Signature::TermAlgebra*>::Iterator it(algebras);
  while (it.hasNext()) {
    declareTermAlgebra(it.next(), codatatype);
  }
    
  algebras->destroy();
}

void SMTLIB2::declareTermAlgebra(Signature::TermAlgebra *ta, bool coalgebra)
{
  CALL("SMTLIB2::declareTermAlgebra");

  if (!coalgebra && !ta->wellFoundedAlgebra()) {
    USER_ERROR("Datatype " + ta->name() + " is not well-founded");
  }

  ASS(!env.signature->isTermAlgebraSort(ta->sort()));
  env.signature->addTermAlgebra(ta);

  List<Signature::TermAlgebraConstructor*>::Iterator constrsIt(ta->constructors());
  while (constrsIt.hasNext()) {
    declareTermAlgebraConstructor(constrsIt.next(), ta->sort());
  }

  UnitList::push(new FormulaUnit(exhaustivenessAxiom(ta),
                                 new Inference(Inference::TERM_ALGEBRA_EXHAUSTIVENESS),
                                 Unit::AXIOM),
                 _formulas);
  if (!env.options->termAlgebraInferences()) {
    UnitList::push(new FormulaUnit(distinctnessAxiom(ta),
                                   new Inference(Inference::TERM_ALGEBRA_DISTINCTNESS),
                                   Unit::AXIOM),
                   _formulas);
    UnitList::push(new FormulaUnit(injectivityAxiom(ta),
                                   new Inference(Inference::TERM_ALGEBRA_INJECTIVITY),
                                   Unit::AXIOM),
                   _formulas);
    if (env.options->termAlgebraCyclicityCheck()) {
      UnitList::push(new FormulaUnit(acyclicityAxiom(ta),
                                     new Inference(Inference::TERM_ALGEBRA_ACYCLICITY),
                                     Unit::AXIOM),
                     _formulas);
    }
  }
}

void SMTLIB2::declareTermAlgebraConstructor(Signature::TermAlgebraConstructor *c, unsigned rangeSort)
{
  CALL("SMTLIB2::declareTermAlgebraConstructor");

  Stack<unsigned> destrArgSort;
  destrArgSort.push(rangeSort);
  Stack<unsigned> argSorts;
  List<pair<vstring,unsigned>>::Iterator argsIt(c->args());
  
  while (argsIt.hasNext()) {
    pair<vstring,unsigned> arg = argsIt.next();
    argSorts.push(arg.second);
    // declare destructor
    if (isAlreadyKnownFunctionSymbol(arg.first)) {
      USER_ERROR("Redeclaring function symbol: " + arg.first);
    }
    declareFunctionOrPredicate(arg.first, arg.second, destrArgSort);
  }
  // declare constructor
  if (isAlreadyKnownFunctionSymbol(c->name())) {
    USER_ERROR("Redeclaring function symbol: " + c->name());
  }
  DeclaredFunction df = declareFunctionOrPredicate(c->name(), rangeSort, argSorts);
  env.signature->getFunction(df.first)->markTermAlgebraCons();
  c->setFunctor(df.first);
}

Formula *SMTLIB2::exhaustivenessAxiom(Signature::TermAlgebra *ta)
{
  CALL("SMTLIB2::exhaustivenessAxiom");

  TermList x(0, false);
  Stack<TermList> argTerms;

  List<Signature::TermAlgebraConstructor*>::Iterator it1(ta->constructors());

  FormulaList *l = FormulaList::empty();

  while (it1.hasNext()) {
    Signature::TermAlgebraConstructor *c = it1.next();
    List<pair<vstring, unsigned>>::Iterator it2(c->args());
    argTerms.reset();
    
    while (it2.hasNext()) {
      pair<vstring, unsigned> a = it2.next();
      unsigned dn = env.signature->getFunctionNumber(a.first, 1);
      TermList t(Term::create1(dn, x));
      argTerms.push(t);
    }
    
    TermList rhs(Term::create(env.signature->getFunctionNumber(c->name(), argTerms.size()),
                              argTerms.size(),
                              argTerms.begin()));
    l = l->cons(new AtomicFormula(Literal::createEquality(true, x, rhs, ta->sort())));
  }

  Formula::VarList* vars = Formula::VarList::empty()->cons(x.var());
  Formula::SortList* sorts = Formula::SortList::empty()->cons(ta->sort());
  
  return new QuantifiedFormula(Connective::FORALL,
                               vars,
                               sorts,
                               new JunctionFormula(Connective::OR, l));
}

Formula *SMTLIB2::distinctnessAxiom(Signature::TermAlgebra *ta)
{
  CALL("SMTLIB2::distinctnessAxiom");

  unsigned varnum = 0;
  FormulaList *l = FormulaList::empty();
  Formula::VarList* vars = Formula::VarList::empty();
  Formula::SortList* sorts = Formula::SortList::empty();

  List<Signature::TermAlgebraConstructor*>* constrs = ta->constructors();
  List<pair<vstring, unsigned>>::Iterator argit;
  Stack<TermList> argTerms;

  while (List<Signature::TermAlgebraConstructor*>::isNonEmpty(constrs)) {
    pair<vstring,unsigned> a;
    Signature::TermAlgebraConstructor* c = constrs->head();
    List<Signature::TermAlgebraConstructor*>::Iterator cit(constrs->tail());

    // build LHS
    argTerms.reset();
    argit.reset(c->args());
    while (argit.hasNext()) {
      a = argit.next();
      TermList var(varnum++, false);
      argTerms.push(var);
      vars = vars->cons(var.var());
      sorts = sorts->cons(a.second);
    }
    TermList lhs(Term::create(env.signature->getFunctionNumber(c->name(), argTerms.size()),
                              argTerms.size(),
                              argTerms.begin()));
    
    while (cit.hasNext()) {
      c = cit.next();

      // build RHS
      argTerms.reset();
      argit.reset(c->args());
      while (argit.hasNext()) {
        a = argit.next();
        TermList var(varnum++, false);
        argTerms.push(var);
        vars = vars->cons(var.var());
        sorts = sorts->cons(a.second);
      }
      TermList rhs(Term::create(env.signature->getFunctionNumber(c->name(), argTerms.size()),
                                argTerms.size(),
                                argTerms.begin()));

      l = l->cons(new AtomicFormula(Literal::createEquality(false, lhs, rhs, ta->sort())));
    }

    constrs = constrs->tail();
  }

  switch (l->length()) {
  case 0:
    return Formula::trueFormula();
    break;
  case 1:
    return new QuantifiedFormula(Connective::FORALL, vars, sorts, l->head());
    break;
  default:
    return new QuantifiedFormula(Connective::FORALL,
                                 vars,
                                 sorts,
                                 new JunctionFormula(Connective::AND, l));
  }
}

Formula *SMTLIB2::injectivityAxiom(Signature::TermAlgebra *ta)
{
  CALL("SMTLIB2::injectivityAxiom");

  FormulaList *l = FormulaList::empty();
  List<Signature::TermAlgebraConstructor*>::Iterator it(ta->constructors());
  Stack<TermList> argTermsX;
  Stack<TermList> argTermsY;
  unsigned varnum = 0;
  
  while (it.hasNext()) {
    Signature::TermAlgebraConstructor* c = it.next();
    
    if (c->args()->length() != 0) {
      FormulaList *implied = FormulaList::empty();
      Formula::VarList* vars = Formula::VarList::empty();
      Formula::SortList* sorts = Formula::SortList::empty();

      argTermsX.reset();
      argTermsY.reset();
      
      List<pair<vstring, unsigned>>::Iterator argit(c->args());

      while (argit.hasNext()) {
        pair<vstring, unsigned> arg = argit.next();
        TermList x(varnum++, false);
        TermList y(varnum++, false);
        sorts = sorts->cons(arg.second)->cons(arg.second);
        vars = vars->cons(x.var())->cons(y.var());
        argTermsX.push(x);
        argTermsY.push(y);
        implied = implied->cons(new AtomicFormula(Literal::createEquality(true, x, y, arg.second)));
      }

      TermList lhs(Term::create(env.signature->getFunctionNumber(c->name(), argTermsX.size()),
                                argTermsX.size(),
                                argTermsX.begin()));
      TermList rhs(Term::create(env.signature->getFunctionNumber(c->name(), argTermsY.size()),
                                argTermsY.size(),
                                argTermsY.begin()));
      Formula *eq = new AtomicFormula(Literal::createEquality(true, lhs, rhs, ta->sort()));
      Formula *impliedf;
      switch (implied->length()) {
      case 0:
        ASSERTION_VIOLATION;
        break;
      case 1:
        impliedf = implied->head();
        break;
      default:
        impliedf = new JunctionFormula(Connective::AND, implied);
        break;
      }
      
      l = l->cons(new QuantifiedFormula(Connective::FORALL,
                                        vars,
                                        sorts,
                                        new BinaryFormula (Connective::IMP, eq, impliedf)));

    }
  }

  switch (l->length()) {
  case 0:
    return Formula::trueFormula();
  case 1:
    return l->head();
  default:
    return new JunctionFormula(Connective::AND, l);
  }
}
  
Formula *SMTLIB2::acyclicityAxiom(Signature::TermAlgebra *ta)
{
  CALL("SMTLIB2::acyclicityAxiom");

  //TODO
  return Formula::trueFormula();
}

bool SMTLIB2::ParseResult::asFormula(Formula*& resFrm)
{
  CALL("SMTLIB2::ParseResult::asFormula");

  if (formula) {
    ASS_EQ(sort, Sorts::SRT_BOOL);
    resFrm = frm;

    LOG2("asFormula formula ",resFrm->toString());
    return true;
  }

  if (sort == Sorts::SRT_BOOL) {
    // can we unwrap instead of wrapping back and forth?
    if (trm.isTerm()) {
      Term* t = trm.term();
      if (t->isFormula()) {
        resFrm = t->getSpecialData()->getFormula();

        // t->destroy(); -- we cannot -- it can be accessed more than once

        LOG2("asFormula unwrap ",trm.toString());

        return true;
      }
    }

    LOG2("asFormula wrap ",trm.toString());

    resFrm = new BoolTermFormula(trm);
    return true;
  }

  return false;
}

unsigned SMTLIB2::ParseResult::asTerm(TermList& resTrm)
{
  CALL("SMTLIB2::ParseResult::asTerm");

  if (formula) {
    ASS_EQ(sort, Sorts::SRT_BOOL);

    LOG2("asTerm wrap ",frm->toString());

    resTrm = TermList(Term::createFormula(frm));

    LOG2("asTerm sort ",sort);
    return Sorts::SRT_BOOL;
  } else {
    resTrm = trm;

    LOG2("asTerm native ",trm.toString());

    LOG2("asTerm sort ",sort);

    return sort;
  }
}

vstring SMTLIB2::ParseResult::toString()
{
  CALL("SMTLIB2::ParseResult::toString");
  if (isSeparator()) {
    return "separator";
  }
  if (formula) {
    return "formula of sort "+Int::toString(sort)+": "+frm->toString();
  }
  return "term of sort "+Int::toString(sort)+": "+trm.toString();
}

Interpretation SMTLIB2::getFormulaSymbolInterpretation(FormulaSymbol fs, unsigned firstArgSort)
{
  CALL("SMTLIB2::getFormulaSymbolInterpretation");

  switch(fs) {
  case FS_LESS:
    switch(firstArgSort) {
    case Sorts::SRT_INTEGER:
  return Theory::INT_LESS;
    case Sorts::SRT_REAL:
  return Theory::REAL_LESS;
    default:
      break;
    }
    break;
  case FS_LESS_EQ:
    switch(firstArgSort) {
    case Sorts::SRT_INTEGER:
  return Theory::INT_LESS_EQUAL;
    case Sorts::SRT_REAL:
      return Theory::REAL_LESS_EQUAL;
    default:
      break;
    }
    break;
  case FS_GREATER:
    switch(firstArgSort) {
    case Sorts::SRT_INTEGER:
      return Theory::INT_GREATER;
    case Sorts::SRT_REAL:
      return Theory::REAL_GREATER;
    default:
      break;
    }
    break;
  case FS_GREATER_EQ:
    switch(firstArgSort) {
    case Sorts::SRT_INTEGER:
      return Theory::INT_GREATER_EQUAL;
    case Sorts::SRT_REAL:
      return Theory::REAL_GREATER_EQUAL;
    default:
      break;
    }
    break;

  default:
    ASSERTION_VIOLATION;
  }
  USER_ERROR("invalid sort "+env.sorts->sortName(firstArgSort)+" for interpretation "+vstring(s_formulaSymbolNameStrings[fs]));
}

Interpretation SMTLIB2::getUnaryMinusInterpretation(unsigned argSort)
{
  CALL("SMTLIB2::getUnaryMinusInterpretation");

  switch(argSort) {
    case Sorts::SRT_INTEGER:
      return Theory::INT_UNARY_MINUS;
    case Sorts::SRT_REAL:
      return Theory::REAL_UNARY_MINUS;
    default:
      USER_ERROR("invalid sort "+env.sorts->sortName(argSort)+" for interpretation -");
  }
}

Interpretation SMTLIB2::getTermSymbolInterpretation(TermSymbol ts, unsigned firstArgSort)
{
  CALL("SMTLIB2::getTermSymbolInterpretation");

  switch(ts) {
  case TS_MINUS:
    switch(firstArgSort) {
    case Sorts::SRT_INTEGER:
  return Theory::INT_MINUS;
    case Sorts::SRT_REAL:
  return Theory::REAL_MINUS;
    default:
      break;
    }
    break;
  case TS_PLUS:
    switch(firstArgSort) {
    case Sorts::SRT_INTEGER:
  return Theory::INT_PLUS;
    case Sorts::SRT_REAL:
      return Theory::REAL_PLUS;
    default:
      break;
    }
    break;
  case TS_MULTIPLY:
    switch(firstArgSort) {
    case Sorts::SRT_INTEGER:
      return Theory::INT_MULTIPLY;
    case Sorts::SRT_REAL:
      return Theory::REAL_MULTIPLY;
    default:
      break;
    }
    break;

  case TS_DIVIDE:
    if (firstArgSort == Sorts::SRT_REAL)
      return Theory::REAL_QUOTIENT;
    break;

  case TS_DIV:
    if (firstArgSort == Sorts::SRT_INTEGER)
      return Theory::INT_QUOTIENT_E;
    break;

  default:
    ASSERTION_VIOLATION_REP(ts);
  }
    USER_ERROR("invalid sort "+env.sorts->sortName(firstArgSort)+" for interpretation "+vstring(s_termSymbolNameStrings[ts]));
}

void SMTLIB2::complainAboutArgShortageOrWrongSorts(const vstring& symbolClass, LExpr* exp)
{
  CALL("SMTLIB2::complainAboutArgShortageOrWrongSorts");

  USER_ERROR("Not enough arguments or wrong sorts for "+symbolClass+" application "+exp->toString());
}

void SMTLIB2::parseLetBegin(LExpr* exp)
{
  CALL("SMTLIB2::parseLetBegin");

  LOG2("parseLetBegin  ",exp->toString());

  ASS(exp->isList());
  LispListReader lRdr(exp->list);

  // the let atom
  const vstring& theLetAtom = lRdr.readAtom();
  ASS_EQ(theLetAtom,LET);

  // now, there should be a list of bindings
  LExprList* bindings = lRdr.readList();

  // and the actual body term
  if (!lRdr.hasNext()) {
    complainAboutArgShortageOrWrongSorts(LET,exp);
  }
  LExpr* body = lRdr.readNext();

  // and that's it
  lRdr.acceptEOL();

  // now read the following bottom up:

  // this will later create the actual let term and kill the lookup
  _todo.push(make_pair(PO_LET_END,exp));

  // this will parse the let's body (in the context of the lookup)
  _todo.push(make_pair(PO_PARSE,body));

  // this will create the lookup when all bindings' expressions are parsed (and their sorts known)
  _todo.push(make_pair(PO_LET_PREPARE_LOOKUP,exp));

  // but we start by parsing the bound expressions
  LispListReader bindRdr(bindings);
  while (bindRdr.hasNext()) {
    LExprList* pair = bindRdr.readList();
    LispListReader pRdr(pair);

    pRdr.readAtom(); // for now ignore the identifier
    LExpr* expr = pRdr.readNext();

    _todo.push(make_pair(PO_PARSE,expr)); // just parse the expression
    pRdr.acceptEOL();
  }
}

void SMTLIB2::parseLetPrepareLookup(LExpr* exp)
{
  CALL("SMTLIB2::parseLetPrepareLookup");
  LOG2("PO_LET_PREPARE_LOOKUP",exp->toString());

  // so we know it is let
  ASS(exp->isList());
  LispListReader lRdr(exp->list);
  const vstring& theLetAtom = lRdr.readAtom();
  ASS_EQ(theLetAtom,LET);

  // with a list of bindings
  LispListReader bindRdr(lRdr.readList());

  // corresponding results have already been parsed
  ParseResult* boundExprs = _results.end();

  TermLookup* lookup = new TermLookup();

  while (bindRdr.hasNext()) {
    LExprList* pair = bindRdr.readList();
    LispListReader pRdr(pair);

    const vstring& cName = pRdr.readAtom();
    unsigned sort = (--boundExprs)->sort; // the should be big enough (

    TermList trm;
    if (sort == Sorts::SRT_BOOL) {
      unsigned symb = env.signature->addFreshPredicate(0,"sLP");
      PredicateType* type = new PredicateType(0, nullptr);
      env.signature->getPredicate(symb)->setType(type);

      Formula* atom = new AtomicFormula(Literal::create(symb,0,true,false,nullptr));
      trm = TermList(Term::createFormula(atom));
    } else {
      unsigned symb = env.signature->addFreshFunction (0,"sLF");
      FunctionType* type = new FunctionType(0, nullptr, sort);
      env.signature->getFunction(symb)->setType(type);

      trm = TermList(Term::createConstant(symb));
    }

    if (!lookup->insert(cName,make_pair(trm,sort))) {
      USER_ERROR("Multiple bindings of symbol "+cName+" in let expression "+exp->toString());
    }
  }

  _scopes.push(lookup);
}

void SMTLIB2::parseLetEnd(LExpr* exp)
{
  CALL("SMTLIB2::parseLetEnd");
  LOG2("PO_LET_END ",exp->toString());

  // so we know it is let
  ASS(exp->isList());
  LispListReader lRdr(exp->list);
  const vstring& theLetAtom = lRdr.readAtom();
  ASS_EQ(getBuiltInTermSymbol(theLetAtom),TS_LET);

  // with a list of bindings
  LispListReader bindRdr(lRdr.readList());

  TermLookup* lookup = _scopes.pop();

  // there has to be the body result:
  TermList let;
  unsigned letSort = _results.pop().asTerm(let);

  LOG2("LET body  ",let.toString());

  while (bindRdr.hasNext()) {
    LExprList* pair = bindRdr.readList();
    LispListReader pRdr(pair);

    const vstring& cName = pRdr.readAtom();
    TermList boundExpr;
    _results.pop().asTerm(boundExpr);

    LOG2("BOUND name  ",cName);
    LOG2("BOUND term  ",boundExpr.toString());

    SortedTerm term;
    ALWAYS(lookup->find(cName,term));
    TermList exprTerm = term.first;
    unsigned exprSort = term.second;

    unsigned symbol = 0;
    if (exprSort == Sorts::SRT_BOOL) { // it has to be formula term, with atomic formula
      symbol = exprTerm.term()->getSpecialData()->getFormula()->literal()->functor();
    } else {
      symbol = exprTerm.term()->functor();
    }

    let = TermList(Term::createLet(symbol, nullptr, boundExpr, let, letSort));
  }

  _results.push(ParseResult(letSort,let));

  delete lookup;
}

void SMTLIB2::parseQuantBegin(LExpr* exp)
{
  CALL("SMTLIB2::parseQuantBegin");

  ASS(exp->isList());
  LispListReader lRdr(exp->list);

  // the quant atom
  const vstring& theQuantAtom = lRdr.readAtom();
  ASS(theQuantAtom == FORALL || theQuantAtom == EXISTS);

  // there should next be a list of sorted variables
  LispListReader varRdr(lRdr.readList());

  TermLookup* lookup = new TermLookup();

  while (varRdr.hasNext()) {
    LExprList* pair = varRdr.readList();
    LispListReader pRdr(pair);

    vstring vName = pRdr.readAtom();
    unsigned vSort = declareSort(pRdr.readNext());

    pRdr.acceptEOL();

    if (!lookup->insert(vName,make_pair(TermList(_nextVar++,false),vSort))) {
      USER_ERROR("Multiple occurrence of variable "+vName+" in quantification "+exp->toString());
    }
  }

  _scopes.push(lookup);

  _todo.push(make_pair(PO_PARSE_APPLICATION,exp)); // will create the actual quantified formula and clear the lookup...
  _todo.push(make_pair(PO_PARSE,lRdr.readNext())); // ... from the only remaining argument, the body
  lRdr.acceptEOL();
}

static const char* EXCLAMATION = "!";

void SMTLIB2::parseAnnotatedTerm(LExpr* exp)
{
  CALL("SMTLIB2::parseAnnotatedTerm");

  ASS(exp->isList());
  LispListReader lRdr(exp->list);

  // the exclamation atom
  const vstring& theExclAtom = lRdr.readAtom();
  ASS_EQ(theExclAtom,EXCLAMATION);

  LExpr* toParse = lRdr.readListExpr();

  static bool annotation_warning = false; // print warning only once

  if (!annotation_warning) {
    env.beginOutput();
    env.out() << "% Warning: term annotations ignored!" << endl;
    env.endOutput();
    annotation_warning = true;
  }

  // we ignore the rest in lRdr (no matter the number of remaining arguments and their structure)

  _todo.push(make_pair(PO_PARSE,toParse));
}

bool SMTLIB2::parseAsScopeLookup(const vstring& id)
{
  CALL("SMTLIB2::parseAsScopeLookup");

  Scopes::Iterator sIt(_scopes);
  while (sIt.hasNext()) {
    TermLookup* lookup = sIt.next();

    SortedTerm st;
    if (lookup->find(id,st)) {
      _results.push(ParseResult(st.second,st.first));
      return true;
    }
  }

  return false;
}

bool SMTLIB2::parseAsSpecConstant(const vstring& id)
{
  CALL("SMTLIB2::parseAsSpecConstant");

  if (StringUtils::isPositiveInteger(id)) {
    if (_numeralsAreReal) {
      goto real_constant; // just below
    }

    unsigned symb = TPTP::addIntegerConstant(id,_overflow,false);
    TermList res = TermList(Term::createConstant(symb));
    _results.push(ParseResult(Sorts::SRT_INTEGER,res));

    return true;
  }

  if (StringUtils::isPositiveDecimal(id)) {
    real_constant:

    unsigned symb = TPTP::addRealConstant(id,_overflow,false);
    TermList res = TermList(Term::createConstant(symb));
    _results.push(ParseResult(Sorts::SRT_REAL,res));

    return true;
  }

  return false;
}

bool SMTLIB2::parseAsUserDefinedSymbol(const vstring& id,LExpr* exp)
{
  CALL("SMTLIB2::parseAsUserDefinedSymbol");

  DeclaredFunction fun;
  if (!_declaredFunctions.find(id,fun)) {
    return false;
  }

  unsigned symbIdx = fun.first;
  bool isTrueFun = fun.second;

  Signature::Symbol* symbol = isTrueFun ? env.signature->getFunction(symbIdx) : env.signature->getPredicate(symbIdx);
  BaseType* type = isTrueFun ? static_cast<BaseType*>(symbol->fnType()) : static_cast<BaseType*>(symbol->predType());

  unsigned arity = symbol->arity();

  static Stack<TermList> args;
  args.reset();

  LOG2("DeclaredFunction of arity ",arity);

  for (unsigned i = 0; i < arity; i++) {
    unsigned sort = type->arg(i);

    TermList arg;
    if (_results.isEmpty() || _results.top().isSeparator() ||
        _results.pop().asTerm(arg) != sort) {
      complainAboutArgShortageOrWrongSorts("user defined symbol",exp);
    }

    args.push(arg);
  }

  if (isTrueFun) {
    unsigned sort = symbol->fnType()->result();
    TermList res = TermList(Term::create(symbIdx,arity,args.begin()));
    _results.push(ParseResult(sort,res));
  } else {
    Formula* res = new AtomicFormula(Literal::create(symbIdx,arity,true,false,args.begin()));
    _results.push(ParseResult(res));
  }

  return true;
}

static const char* BUILT_IN_SYMBOL = "built-in symbol";

bool SMTLIB2::parseAsBuiltinFormulaSymbol(const vstring& id, LExpr* exp)
{
  CALL("SMTLIB2::parseAsBuiltinFormulaSymbol");

  FormulaSymbol fs = getBuiltInFormulaSymbol(id);
  switch (fs) {
    case FS_TRUE:
      _results.push(ParseResult(Formula::trueFormula()));
      return true;
    case FS_FALSE:
      _results.push(ParseResult(Formula::falseFormula()));
      return true;
    case FS_NOT:
    {
      if (_results.isEmpty() || _results.top().isSeparator()) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }
      Formula* argFla;
      if (!(_results.pop().asFormula(argFla))) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }
      Formula* res = new NegatedFormula(argFla);
      _results.push(ParseResult(res));
      return true;
    }
    case FS_AND:
    case FS_OR:
    {
      FormulaList* argLst = nullptr;

      LOG1("FS_AND and FS_OR");

      unsigned argcnt = 0;
      while (_results.isNonEmpty() && (!_results.top().isSeparator())) {
        argcnt++;
        Formula* argFla;
        if (!(_results.pop().asFormula(argFla))) {
          complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
        }
        FormulaList::push(argFla,argLst);
      }

      if (argcnt < 1) { // TODO: officially, we might want to disallow singleton AND and OR, but they are harmless and appear in smtlib
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }

      Formula* res;
      if (argcnt > 1) {
        res = new JunctionFormula( (fs==FS_AND) ? AND : OR, argLst);
      } else {
        res = argLst->head();
        argLst->destroy();
      }
      _results.push(ParseResult(res));

      return true;
    }
    case FS_IMPLIES: // done in a right-assoc multiple-argument fashion
    case FS_XOR: // they say XOR should be left-associative, but semantically, it does not matter
    {
      Connective con = (fs==FS_IMPLIES) ? IMP : XOR;

      static Stack<Formula*> args;
      ASS(args.isEmpty());

      // put argument formulas on stack (reverses the order)
      while (_results.isNonEmpty() && (!_results.top().isSeparator())) {
        Formula* argFla;
        if (!(_results.pop().asFormula(argFla))) {
          complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
        }
        args.push(argFla);
      }

      if (args.size() < 2) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }

      // the last two go first
      Formula* arg_n = args.pop();
      Formula* arg_n_1 = args.pop();
      Formula* res = new BinaryFormula(con, arg_n_1, arg_n);

      // keep on adding in a right-assoc way
      while(args.isNonEmpty()) {
        res = new BinaryFormula(con, args.pop(), res);
      }

      _results.push(ParseResult(res));

      return true;
    }
    // all the following are "chainable" and need to respect sorts
    case FS_EQ:
    case FS_LESS:
    case FS_LESS_EQ:
    case FS_GREATER:
    case FS_GREATER_EQ:
    {
      // read the first two arguments
      TermList first;
      if (_results.isEmpty() || _results.top().isSeparator()) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }
      unsigned sort = _results.pop().asTerm(first);
      TermList second;
      if (_results.isEmpty() || _results.top().isSeparator() ||
          _results.pop().asTerm(second) != sort) { // has the same sort as first

        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }

      Formula* lastConjunct;
      unsigned pred = 0;
      if (fs == FS_EQ) {
        lastConjunct = new AtomicFormula(Literal::createEquality(true, first, second, sort));
      } else {
        Interpretation intp = getFormulaSymbolInterpretation(fs,sort);
        pred = Theory::instance()->getPredNum(intp);
        lastConjunct = new AtomicFormula(Literal::create2(pred,true,first,second));
      }

      FormulaList* argLst = nullptr;
      // for every other argument ... pipelining
      while (_results.isEmpty() || !_results.top().isSeparator()) {
        TermList next;
        if (_results.pop().asTerm(next) != sort) {
          complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
        }
        // store the old conjunct
        FormulaList::push(lastConjunct,argLst);
        // shift the arguments
        first = second;
        second = next;
        // create next conjunct
        if (fs == FS_EQ) {
          lastConjunct = new AtomicFormula(Literal::createEquality(true, first, second, sort));
        } else {
          lastConjunct = new AtomicFormula(Literal::create2(pred,true,first,second));
        }
      }
      if (argLst == nullptr) { // there were only two arguments, let's return lastConjunct
        _results.push(lastConjunct);
      } else {
        // add the last lastConjunct created (pipelining)
        FormulaList::push(lastConjunct,argLst);
        // create the actual conjunction
        Formula* res = new JunctionFormula( AND, argLst);
        _results.push(ParseResult(res));
      }

      return true;
    }
    case FS_DISTINCT:
    {
      static Stack<TermList> args;
      args.reset();

      // read the first argument and its sort
      TermList first;
      if (_results.isEmpty() || _results.top().isSeparator()) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }
      unsigned sort = _results.pop().asTerm(first);

      args.push(first);

      // put remaining arguments on stack (reverses the order, which does not matter)
      while (_results.isNonEmpty() && (!_results.top().isSeparator())) {
        TermList argTerm;
        if (_results.pop().asTerm(argTerm) != sort) {
          complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
        }
        args.push(argTerm);
      }

      if (args.size() < 2) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }

      Formula* res;
      if(args.size()==2) { // if there are 2 just create a disequality
        res = new AtomicFormula(Literal::createEquality(false,args[0],args[1],sort));
      } else { // Otherwise create a formula list of disequalities
        FormulaList* diseqs = nullptr;

        for(unsigned i=0;i<args.size();i++){
          for(unsigned j=0;j<i;j++){
            Formula* new_dis = new AtomicFormula(Literal::createEquality(false,args[i],args[j],sort));
            FormulaList::push(new_dis,diseqs);
          }
        }

        res = new JunctionFormula(AND, diseqs);
      }

      _results.push(res);

      return true;
    }
    case FS_IS_INT:
    {
      TermList arg;
      if (_results.isEmpty() || _results.top().isSeparator() ||
          _results.pop().asTerm(arg) != Sorts::SRT_REAL) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }

      unsigned pred = Theory::instance()->getPredNum(Theory::REAL_IS_INT);
      Formula* res = new AtomicFormula(Literal::create1(pred,true,arg));

      _results.push(res);

      return true;
    }
    case FS_EXISTS:
    case FS_FORALL:
    {
      Formula* argFla;
      if (_results.isEmpty() || _results.top().isSeparator() ||
          !(_results.pop().asFormula(argFla))) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }

      Formula::VarList* qvars = nullptr;
      Formula::SortList* qsorts = nullptr;

      TermLookup::Iterator varIt(*_scopes.top());
      while(varIt.hasNext()) {
        SortedTerm vTerm = varIt.next();
        unsigned varIdx = vTerm.first.var();
        unsigned sort = vTerm.second;
        Formula::VarList::push(varIdx, qvars);
        Formula::SortList::push(sort,qsorts);
      }
      delete _scopes.pop();

      Formula* res = new QuantifiedFormula((fs==FS_EXISTS) ? Kernel::EXISTS : Kernel::FORALL, qvars, qsorts, argFla);

      _results.push(ParseResult(res));
      return true;
    }

    default:
      ASS_EQ(fs,FS_USER_PRED_SYMBOL);
      return false;
  }
}

bool SMTLIB2::parseAsBuiltinTermSymbol(const vstring& id, LExpr* exp)
{
  CALL("SMTLIB2::parseAsBuiltinTermSymbol");

  // try built-in term symbols
  TermSymbol ts = getBuiltInTermSymbol(id);
  switch(ts) {
    case TS_ITE:
    {
      Formula* cond;
      if (_results.isEmpty() || _results.top().isSeparator() ||
          !(_results.pop().asFormula(cond))) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }
      TermList thenBranch;
      if (_results.isEmpty() || _results.top().isSeparator()){
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }
      unsigned sort = _results.pop().asTerm(thenBranch);
      TermList elseBranch;
      if (_results.isEmpty() || _results.top().isSeparator() ||
          _results.pop().asTerm(elseBranch) != sort){
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }

      TermList res = TermList(Term::createITE(cond, thenBranch, elseBranch, sort));

      _results.push(ParseResult(sort,res));
      return true;
    }
    case TS_TO_REAL:
    {
      TermList theInt;
      if (_results.isEmpty() || _results.top().isSeparator() ||
          _results.pop().asTerm(theInt) != Sorts::SRT_INTEGER) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }

      unsigned fun = Theory::instance()->getFnNum(Theory::INT_TO_REAL);
      TermList res = TermList(Term::create1(fun,theInt));

      _results.push(ParseResult(Sorts::SRT_REAL,res));
      return true;
    }
    case TS_TO_INT:
    {
      TermList theReal;
      if (_results.isEmpty() || _results.top().isSeparator() ||
          _results.pop().asTerm(theReal) != Sorts::SRT_REAL) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }

      unsigned fun = Theory::instance()->getFnNum(Theory::REAL_TO_INT);
      TermList res = TermList(Term::create1(fun,theReal));

      _results.push(ParseResult(Sorts::SRT_INTEGER,res));
      return true;
    }
    case TS_SELECT:
    {
      TermList theArray;
      if (_results.isEmpty() || _results.top().isSeparator()) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }
      unsigned arraySortIdx = _results.pop().asTerm(theArray);
      if (!env.sorts->hasStructuredSort(arraySortIdx,Sorts::StructuredSort::ARRAY)) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }
      Sorts::ArraySort* arraySort = env.sorts->getArraySort(arraySortIdx);

      TermList theIndex;
      if (_results.isEmpty() || _results.top().isSeparator() ||
          _results.pop().asTerm(theIndex) != arraySort->getIndexSort()) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }

      if (arraySort->getInnerSort() == Sorts::SRT_BOOL) {
        unsigned pred = Theory::instance()->getSymbolForStructuredSort(arraySortIdx,Theory::StructuredSortInterpretation::ARRAY_BOOL_SELECT);

        Formula* res = new AtomicFormula(Literal::create2(pred,true,theArray,theIndex));

        _results.push(ParseResult(res));
      } else {
        unsigned fun = Theory::instance()->getSymbolForStructuredSort(arraySortIdx,Theory::StructuredSortInterpretation::ARRAY_SELECT);
        TermList res = TermList(Term::create2(fun,theArray,theIndex));

        _results.push(ParseResult(arraySort->getInnerSort(),res));
      }

      return true;
    }
    case TS_STORE:
    {
      TermList theArray;
      if (_results.isEmpty() || _results.top().isSeparator()) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }
      unsigned arraySortIdx = _results.pop().asTerm(theArray);
      if (!env.sorts->hasStructuredSort(arraySortIdx,Sorts::StructuredSort::ARRAY)) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }
      Sorts::ArraySort* arraySort = env.sorts->getArraySort(arraySortIdx);

      TermList theIndex;
      if (_results.isEmpty() || _results.top().isSeparator() ||
          _results.pop().asTerm(theIndex) != arraySort->getIndexSort()) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }

      TermList theValue;
      if (_results.isEmpty() || _results.top().isSeparator() ||
          _results.pop().asTerm(theValue) != arraySort->getInnerSort()) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }

      unsigned fun = Theory::instance()->getSymbolForStructuredSort(arraySortIdx,Theory::StructuredSortInterpretation::ARRAY_STORE);

      TermList args[] = {theArray, theIndex, theValue};
      TermList res = TermList(Term::Term::create(fun, 3, args));

      _results.push(ParseResult(arraySortIdx,res));

      return true;
    }
    case TS_ABS:
    {
      TermList theInt;
      if (_results.isEmpty() || _results.top().isSeparator() ||
          _results.pop().asTerm(theInt) != Sorts::SRT_INTEGER) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }

      unsigned fun = Theory::instance()->getFnNum(Theory::INT_ABS);
      TermList res = TermList(Term::create1(fun,theInt));

      _results.push(ParseResult(Sorts::SRT_INTEGER,res));

      return true;
    }
    case TS_MOD:
    {
      TermList int1, int2;
      if (_results.isEmpty() || _results.top().isSeparator() || _results.pop().asTerm(int1) != Sorts::SRT_INTEGER ||
          _results.isEmpty() || _results.top().isSeparator() || _results.pop().asTerm(int2) != Sorts::SRT_INTEGER) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }

      unsigned fun = Theory::instance()->getFnNum(Theory::INT_MODULO);
      TermList res = TermList(Term::create2(fun,int1,int2));

      _results.push(ParseResult(Sorts::SRT_INTEGER,res));

      return true;
    }
    case TS_MULTIPLY:
    case TS_PLUS:
    case TS_MINUS:
    case TS_DIVIDE:
    case TS_DIV:
    {
      // read the first argument
      TermList first;
      if (_results.isEmpty() || _results.top().isSeparator()) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }
      unsigned sort = _results.pop().asTerm(first);

      if (_results.isEmpty() || _results.top().isSeparator()) {
        if (ts == TS_MINUS) { // unary minus
          Interpretation intp = getUnaryMinusInterpretation(sort);
          unsigned fun = Theory::instance()->getFnNum(intp);

          TermList res = TermList(Term::create1(fun,first));

          _results.push(ParseResult(sort,res));

          return true;
        } else {
          complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp); // we need at least two arguments otherwise
        }
      }

      Interpretation intp = getTermSymbolInterpretation(ts,sort);
      unsigned fun = Theory::instance()->getFnNum(intp);

      TermList second;
      if (_results.pop().asTerm(second) != sort) {
        complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
      }

      TermList res = TermList(Term::create2(fun,first,second));
      while (_results.isNonEmpty() && !_results.top().isSeparator()) {
        TermList another;
        if (_results.pop().asTerm(another) != sort) {
          complainAboutArgShortageOrWrongSorts(BUILT_IN_SYMBOL,exp);
        }

        res = TermList(Term::create2(fun,res,another));
      }
      _results.push(ParseResult(sort,res));

      return true;
    }
    default:
      ASS_EQ(ts,TS_USER_FUNCTION);
      return false;
  }
}

static const char* UNDERSCORE = "_";

void SMTLIB2::parseRankedFunctionApplication(LExpr* exp)
{
  CALL("SMTLIB2::parseRankedFunctionApplication");

  ASS(exp->isList());
  LispListReader lRdr(exp->list);
  LExpr* head = lRdr.readNext();
  ASS(head->isList());
  LispListReader headRdr(head);

  headRdr.acceptAtom(UNDERSCORE);

  // currently we only support divisible, so this is easy
  headRdr.acceptAtom("divisible");

  const vstring& numeral = headRdr.readAtom();

  if (!StringUtils::isPositiveInteger(numeral)) {
    USER_ERROR("Expected numeral as an argument of a ranked function in "+head->toString());
  }

  unsigned divisorSymb = TPTP::addIntegerConstant(numeral,_overflow,false);
  TermList divisorTerm = TermList(Term::createConstant(divisorSymb));

  TermList arg;
  if (_results.isEmpty() || _results.top().isSeparator() ||
      _results.pop().asTerm(arg) != Sorts::SRT_INTEGER) {
    complainAboutArgShortageOrWrongSorts("ranked function symbol",exp);
  }

  unsigned pred = Theory::instance()->getPredNum(Theory::INT_DIVIDES);
  env.signature->recordDividesNvalue(divisorTerm);

  Formula* res = new AtomicFormula(Literal::create2(pred,true,divisorTerm,arg));

  _results.push(ParseResult(res));
}

SMTLIB2::ParseResult SMTLIB2::parseTermOrFormula(LExpr* body)
{
  CALL("SMTLIB2::parseTermOrFormula");

  ASS(_todo.isEmpty());
  ASS(_results.isEmpty());

  _todo.push(make_pair(PO_PARSE,body));

  while (_todo.isNonEmpty()) {
    /*
    cout << "Results:" << endl;
    for (unsigned i = 0; i < results.size(); i++) {
      cout << results[i].toString() << endl;
    }
    cout << "---" << endl;
    */

    pair<ParseOperation,LExpr*> cur = _todo.pop();
    ParseOperation op = cur.first;
    LExpr* exp = cur.second;

    switch (op) {
      case PO_PARSE: {
        if (exp->isList()) {
          LispListReader lRdr(exp->list);

          // schedule arity check
          _results.push(ParseResult()); // separator into results
          _todo.push(make_pair(PO_CHECK_ARITY,exp)); // check as a todo (exp for error reporting)

          // special treatment of some tokens
          LExpr* fst = lRdr.readNext();
          if (fst->isAtom()) {
            vstring& id = fst->str;

            if (id == FORALL || id == EXISTS) {
              parseQuantBegin(exp);
              continue;
            }

            if (id == LET) {
              parseLetBegin(exp);
              continue;
            }

            if (id == EXCLAMATION) {
              parseAnnotatedTerm(exp);
              continue;
            }

            if (id == UNDERSCORE) {
              USER_ERROR("Indexed identifiers in general term position are not supported: "+exp->toString());

              // we only support indexed identifiers as functors applied to something (see just below)
            }
          } else {
            // this has to be an UNDERSCORE, otherwise we error later when we PO_PARSE_APPLICATION
          }

          // this handles the general function-to-arguments application:

          _todo.push(make_pair(PO_PARSE_APPLICATION,exp));
          // and all the other arguments too
          while (lRdr.hasNext()) {
            _todo.push(make_pair(PO_PARSE,lRdr.next()));
          }

          continue;
        }

        // INTENTIONAL FALL-THROUGH FOR ATOMS
      }
      case PO_PARSE_APPLICATION: { // the arguments have already been parsed
        vstring id;
        if (exp->isAtom()) { // the fall-through case
          id = exp->str;
        } else {
          ASS(exp->isList());
          LispListReader lRdr(exp->list);

          LExpr* head = lRdr.readNext();

          if (head->isList()) {
            parseRankedFunctionApplication(exp);
            continue;
          }
          ASS(head->isAtom());
          id = head->str;
        }

        if (parseAsScopeLookup(id)) {
          continue;
        }

        if (parseAsSpecConstant(id)) {
          continue;
        }

        if (parseAsUserDefinedSymbol(id,exp)) {
          continue;
        }

        if (parseAsBuiltinFormulaSymbol(id,exp)) {
          continue;
        }

        if (parseAsBuiltinTermSymbol(id,exp)) {
          continue;
        }

        USER_ERROR("Unrecognized term identifier "+id);
      }
      case PO_CHECK_ARITY: {
        LOG1("PO_CHECK_ARITY");

        ASS_GE(_results.size(),2);
        ParseResult true_result = _results.pop();
        ParseResult separator   = _results.pop();

        if (true_result.isSeparator() || !separator.isSeparator()) {
          USER_ERROR("Too many arguments in "+exp->toString());
        }
        _results.push(true_result);

        continue;
      }
      case PO_LET_PREPARE_LOOKUP:
        parseLetPrepareLookup(exp);
        continue;
      case PO_LET_END:
        parseLetEnd(exp);
        continue;
    }
  }

  if (_results.size() == 1) {
    return _results.pop();
  } else {
    USER_ERROR("Malformed term expression "+body->toString());
  }
}

void SMTLIB2::readAssert(LExpr* body)
{
  CALL("SMTLIB2::readAssert");

  _nextVar = 0;
  ASS(_scopes.isEmpty());

  ParseResult res = parseTermOrFormula(body);

  Formula* fla;
  if (!res.asFormula(fla)) {
    USER_ERROR("Asserted expression of non-boolean sort "+body->toString());
  }

  FormulaUnit* fu = new FormulaUnit(fla, new Inference(Inference::INPUT), Unit::ASSUMPTION);

  UnitList::push(fu, _formulas);
}

}
