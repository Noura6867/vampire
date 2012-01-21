/**
 * @file SMTLIB.hpp
 * Defines class SMTLIB.
 */

#ifndef __SMTLIB__
#define __SMTLIB__

#include "Forwards.hpp"

#include "Kernel/Sorts.hpp"
#include "Kernel/Term.hpp"

#include "Shell/LispParser.hpp"


namespace Parse {

using namespace Lib;
using namespace Kernel;
using namespace Shell;

class SMTLIB {
public:
  enum Mode {
    READ_BENCHMARK = 0,
    DECLARE_SYMBOLS = 1,
    BUILD_FORMULA = 2
  };

  /**
   * Information from a function or predicate declaration
   */
  struct FunctionInfo {
    FunctionInfo(string name, Stack<string> argSorts, string domainSort)
     : name(name), argSorts(argSorts), rangeSort(domainSort) {}

    string name;
    Stack<string> argSorts;
    string rangeSort;
  };

  SMTLIB(Mode mode = BUILD_FORMULA);

  /**
   * @param bench lisp list having atom "benchmark" as the first element
   */
  void parse(LExpr* bench);
  void parse(istream& str);

  //these functions can be used after a call to a parse() function
  const Stack<string> getUserSortNames() const { return _userSorts; }
  const Stack<FunctionInfo> getFuncInfos() const { return _funcs; }
  LExpr* getLispFormula() const { ASS(_lispFormula); return _lispFormula; }

  /**
   * Return parsed formula.
   *
   * This function can be called after calling some of the parse() functions and
   * when the mode in is set to BUILD_FORMULA.
   */
  Unit* getFormula() const { ASS(_formula); return _formula; }
private:

  void readBenchmark(LExprList* bench);
  void readSort(string name) { _userSorts.push(name); }
  void readFunction(LExprList* decl);
  void readPredicate(LExprList* decl);

  unsigned getSort(string name);
  void doDeclarations();

  string _benchName;
  string _statusStr;

  Stack<string> _userSorts;
  Stack<FunctionInfo> _funcs;
  LExpr* _lispFormula;

  FormulaUnit* _formula;

  Mode _mode;
#if VDEBUG
  bool _haveParsed;
#endif

private:
  //formula building

  /**
   * Possible symbols in the beginning of a lisp list representing formula
   */
  enum FormulaSymbol
  {
    FS_EQ,
    FS_AND,
    FS_EXISTS,
    FS_FLET,
    FS_FORALL,
    FS_IF_THEN_ELSE,
    FS_IFF,
    FS_IMPLIES,
    FS_LET,
    FS_NOT,
    FS_OR,
    FS_XOR,

    FS_USER_PRED_SYMBOL
  };
  static const char * s_formulaSymbolNameStrings[];

  DHMap<LExpr*,Formula*> _forms;
  DHMap<LExpr*,TermList> _terms;

  //lets are set when we their scope appears on a to-do stack and unset
  //when we remove them from there
  DHMap<string,Formula*> _formVars;
  DHMap<string,TermList> _termVars;

  /** Next quantified variable index to be used */
  unsigned _nextQuantVar;

  /**
   * _varSorts[i] contains sort of variable Xi.
   */
  Stack<unsigned> _varSorts;

  /**
   * The second element in the pair is a bool determining whether the
   * lisp expression denotes a formula (true) or a term (false).
   */
  typedef pair<LExpr*,bool> ToDoEntry;
  /**
   * Stack with lisp expressions that need have their corresponding
   * terms and formulas built.
   */
  Stack<ToDoEntry> _todo;

  /**
   * True if we are entering a new list expression on the to-do stack.
   * False if we have returned to the expression after evaluating its children.
   */
  bool _entering;
  ToDoEntry _current;

  bool tryGetArgumentTerm(LExpr* parent, LExpr* argument, TermList& res);
  bool tryGetArgumentFormula(LExpr* parent, LExpr* argument, Formula*& res);

  void requestSubexpressionProcessing(LExpr* subExpr, bool formula);

  static FormulaSymbol getFormulaSymbol(string str);
  static unsigned getMandatoryConnectiveArgCnt(FormulaSymbol fsym);
//  unsigned getPredSymbolArity(FormulaSymbol fsym, string str);

  unsigned getSort(TermList t);
  void ensureArgumentSorts(bool pred, unsigned symNum, TermList* args);

  TermList readTermFromAtom(string str);
  Formula* readFormulaFromAtom(string str);

  bool tryReadTerm(LExpr* e, Term*& res);
  bool tryReadNonPropAtom(FormulaSymbol fsym, LExpr* e, Literal*& res);
  bool tryReadConnective(FormulaSymbol fsym, LExpr* e, Formula*& res);
  bool tryReadQuantifier(bool univ, LExpr* e, Formula*& res);
  bool tryReadFormula(Formula*& res);
  void buildFormula();


};

}

#endif // __SMTLIB__
