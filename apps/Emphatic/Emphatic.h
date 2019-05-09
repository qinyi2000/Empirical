/**
 *  @note This file is part of Empirical, https://github.com/devosoft/Empirical
 *  @copyright Copyright (C) Michigan State University, MIT Software license; see doc/LICENSE.md
 *  @date 2019
 *
 *  @file  Emphatic.h
 *  @brief A system to generate dynamic concept code for C++17.
 *  @note Status: ALPHA
 *
 *  NOTES:
 *   - TYPE_OR_EXPRESSION: Collect everything until you hit an unmatched close-mark: ')', ']', '}', or '>'
 *   - STATEMENT: Collect everything until you hit a ';' outside of parens.
 *   - STATEMENT_LIST: (nothing)                  (an unmatched close-mark next requires this option)
 *                   | STATEMENT STATEMENT_LIST
 *   - BLOCK: '{' STATEMENT_LIST '}'
 * 
 *   - TYPE: ID TYPE_END
 *   - TYPE_END: (nothing)
 *             | "::" TYPE
 *             | "<" TYPE_OR_EXPRESSION ">" TYPE_END
 *             | '&'
 *             | '*'
 *   - DECLARE: TYPE ID
 *   - FUNCTION: DECLARE '(' PARAMS ')' BLOCK
 *             | DECLARE '(' PARAMS ')' '=' "required" ';'
 *             | DECLARE '(' PARAMS ')' '=' "default" ';'
 *   - PARAMS: (nothing)
 *           | PARAM_LIST
 *   - PARAM_LIST: PARAM
 *               | PARAM ',' PARAM_LIST
 *   - PARAM: DECLARE
 *          | OVERLOAD '(' ID ')'
 *   - MEMBER: DECLARE ';'
 *           | FUNCTION
 *           | "using" ID '=' TYPE ';'
 *           | "using" ID '=' "required" ';'
 */

#include <fstream>
#include <iostream>
#include <set>
#include <string>

#include "AST.h"
#include "EmphaticLexer.h"

class Emphatic {
private:
  std::string filename;             ///< Source for for code to generate.
  EmphaticLexer lexer;              ///< Lexer to process input code.
  emp::vector<emp::Token> tokens;   ///< Tokenized version of input file.
  bool debug = false;               ///< Should we print full debug information?

  AST_Scope ast_root;

  // -- Helper functions --
  bool HasToken(int pos) const { return (pos >= 0) && (pos < (int) tokens.size()); }
  bool IsID(int pos) const { return HasToken(pos) && lexer.IsID(tokens[pos]); }
  bool IsNumber(int pos) const { return HasToken(pos) && lexer.IsNumber(tokens[pos]); }
  bool IsString(int pos) const { return HasToken(pos) && lexer.IsString(tokens[pos]); }
  bool IsPP(int pos) const { return HasToken(pos) && lexer.IsPP(tokens[pos]); }
  char AsChar(int pos) const {
    return (HasToken(pos) && lexer.IsSymbol(tokens[pos])) ? tokens[pos].lexeme[0] : 0;
  }
  const std::string & AsLexeme(int pos) const {
    return HasToken(pos) ? tokens[pos].lexeme : emp::empty_string();
  }

  std::string ConcatLexemes(size_t start_pos, size_t end_pos) {
    emp_assert(start_pos <= end_pos);
    emp_assert(end_pos <= tokens.size());
    std::stringstream ss;    
    for (size_t i = start_pos; i < end_pos; i++) {
      if (i > start_pos) ss << " ";
      ss << tokens[i].lexeme;
    }
    return ss.str();
  }

  void Error(int pos, const std::string & msg) {
    std::cout << "Error (token " << pos << "): " << msg << "\nAborting." << std::endl;
    exit(1);
  }

  template <typename... Ts>
  void Debug(Ts... args) {
    if (debug) std::cout << "DEBUG: " << emp::to_string(args...) << std::endl;
  }

  void RequireID(int pos, const std::string & error_msg) {
    if (!IsID(pos)) { Error(pos, error_msg); }
  }
  void RequireNumber(int pos, const std::string & error_msg) {
    if (!IsNumber(pos)) { Error(pos, error_msg); }
  }
  void RequireString(int pos, const std::string & error_msg) {
    if (!IsString(pos)) { Error(pos, error_msg); }
  }
  void RequireChar(char req_char, int pos, const std::string & error_msg) {
    if (AsChar(pos) != req_char) { Error(pos, error_msg); }
  }
  void RequireLexeme(const std::string & req_str, int pos, const std::string & error_msg) {
    if (AsLexeme(pos) != req_str) { Error(pos, error_msg); }
  }

public:
  Emphatic(std::string in_filename) : filename(in_filename) {
    std::ifstream file(filename);
    tokens = lexer.Tokenize(file);
    file.close();
  }

  /// Print out the original state of the code.
  void PrintEcho(std::ostream & os) const {
    ast_root.PrintEcho(os, "");
  }

  /// Print out the original state of the code.
  void PrintOutput(std::ostream & os) const {
    ast_root.PrintOutput(os, "");
  }

  /// Collect a line of code, ending with a semi-colon OR mis-matched bracket.
  /// Always stops at a mis-matched ')' '}' or ']'
  /// If match_angle_bracket is set, will also stop at a mis-matched '>'
  /// If multi_line is set, will NOT stop with a ';'
  std::string ProcessCode(size_t & pos, bool match_angle_bracket=false, bool multi_line=false) {
    const size_t start_pos = pos;
    std::vector<char> open_symbols;
    bool finished = false;
    while (!finished && pos < tokens.size()) {
      char cur_char = AsChar(pos++);
      switch (cur_char) {
        case ';':
          if (multi_line == false) finished = true;
          break;
        case '<':
          if (match_angle_bracket == false) break;
          [[fallthrough]]
        case '(':
        case '[':
        case '{':
          open_symbols.push_back(cur_char);  // Store this open bracket to be matched later.
          break;
        case '>':
          if (match_angle_bracket == false) break;
          [[fallthrough]]
        case ')':
        case ']':
        case '}':
          if (open_symbols.size()) {
            // @CAO should check to make sure this is a CORRECT match...
            open_symbols.pop_back();
            break;
          }
          // We will only make it here is this is an unmatched bracket.
          pos--;              // Leave close bracket to still be processed.
          finished = true;
          break;
      }
    }

    return ConcatLexemes(start_pos, pos);
  }

  /// Collect all tokens used to describe a type.
  std::string ProcessType(size_t & pos) {
    const size_t start_pos = pos;
    // A type may start with a const.
    if (AsLexeme(pos) == "const") pos++;

    // Figure out the identifier (with possible "::" requiring another id)
    bool need_id = true;
    while (need_id) {
      if (AsLexeme(pos) == "typename") pos++;  // May specify a typename is next.
      if (AsLexeme(pos) == "template") pos++;  // May specify a template is next.

      RequireID(pos, emp::to_string("Expecting type, but found '", tokens[pos].lexeme, "'."));
      pos++;
      need_id = false;

      // In case this is a template, we need to evaluate parameters.
      if (AsLexeme(pos) == "<") {
        ProcessCode(++pos, true);
        RequireChar('>', pos++, "Templates must end in a close angle bracket.");
      }

      if (AsLexeme(pos) == "::") {
        pos++;
        need_id = true;
      }
    }

    // Type may end in a symbol...
    if (AsLexeme(pos) == "&") pos++;
    if (AsLexeme(pos) == "*") pos++;

    // Collect all of the lexemes
    return ConcatLexemes(start_pos, pos);
  }

  /// Collect all of the parameter definitions for a function.
  emp::vector<ParamInfo> ProcessParams(size_t & pos) {
    emp::vector<ParamInfo> params;

    while (AsChar(pos) != ')') {
      // If this isn't the first parameter, make sure we have a comma to separate them.
      if (params.size()) {
        RequireChar(',', pos++, "Parameters must be separated by commas.");
      }

      // Start with a type...
      std::string type_name = ProcessType(pos);

      // If an identifier is specified for this parameter, grab it.
      std::string identifier = IsID(pos) ? tokens[pos++].lexeme : "";

      params.emplace_back(ParamInfo{type_name, identifier});
    }

    return params;
  }

  /// Collect a series of identifiers, separated by spaces.
  std::set<std::string> ProcessIDList(size_t & pos) {
    std::set<std::string> ids;
    while (IsID(pos)) {
      ids.insert(AsLexeme(pos));
      pos++;
    }
    return ids;
  }

  /// Collect information about a template; if there is no template, leave the string empty.
  std::string ProcessTemplate(size_t & pos) {
    size_t start_pos = pos;
    if (AsLexeme(pos) != "template") return "";
    pos++;
    RequireChar('<', pos++, "Templates must begin with a '<'");
    // @CAO Must collect parameters..
    RequireChar('>', pos++, "Templates must end with a '>'");
    return ConcatLexemes(start_pos, pos);
  }

  /// Collect information about an element (function, variable, or typedef) definition.
  ElementInfo ProcessElement(size_t & pos) {
    ElementInfo info;
    // @CAO TODO
    return info;
  }

  /// Process the tokens starting from the outer-most scope.
  void ProcessTop(size_t & pos, AST_Scope & cur_scope ) {
    while (pos < tokens.size() && AsChar(pos) != '}') {
      // If this line is a pre-processor statement, just hook it in to print back out and keep going.
      if (IsPP(pos)) {
        AST_PP & new_node = cur_scope.NewChild<AST_PP>();
        new_node.code = emp::to_string( AsLexeme(pos++), '\n');
        continue;
      }

      // Anything other than a lexeme has to begin with a keyword or identifier.
      RequireID(pos, emp::to_string("Statements in outer scope must begin with an identifier or keyword.  (Found: ",
                     AsLexeme(pos), ")."));

      std::string cur_lexeme = AsLexeme(pos++);
      if (cur_lexeme == "concept") {
        ProcessConcept(pos, cur_scope);
      }
      else if (cur_lexeme == "struct" || cur_lexeme == "class") {
        AST_Class & new_class = cur_scope.NewChild<AST_Class>();
        new_class.type = cur_lexeme;
        if (IsID(pos)) new_class.name = AsLexeme(pos++);
        RequireChar('{', pos++, emp::to_string("A ", cur_lexeme, " must be defined in braces ('{' and '}')."));
        new_class.body = ProcessCode(pos, false, true);
        RequireChar('}', pos++, emp::to_string("The end of a ", cur_lexeme, " must have a close brace ('}')."));
        RequireChar(';', pos++, emp::to_string("A ", cur_lexeme, " must end with a semi-colon (';')."));
      }
      else if (cur_lexeme == "namespace") {
        auto & new_ns = cur_scope.NewChild<AST_Namespace>();

        // If a name is provided for this namespace, store it.
        if (IsID(pos)) new_ns.name = AsLexeme(pos++);

        RequireChar('{', pos++, emp::to_string("A ", cur_lexeme, " must be defined in braces ('{' and '}')."));
        ProcessTop(pos, new_ns);
        RequireChar('}', pos++, emp::to_string("The end of a ", cur_lexeme, " must have a close brace ('}')."));
      }
      else if (cur_lexeme == "using") {
        RequireID(pos, "A 'using' command must first specify the new type name.");
        auto & new_using = cur_scope.NewChild<AST_Using>();
        new_using.name = ProcessType(pos);    // Determine new type name being defined.
        RequireChar('=', pos++, "A using statement must provide an equals ('=') to assign the type.");
        new_using.type = ProcessCode(pos);   // Determine code being assigned to.
      }
      // @CAO: Still need to deal with "template", variables and functions, enums, template specializations
      ///      and empty lines (';').
      else {
        Error( pos-1, emp::to_string("Unknown keyword '", cur_lexeme, "'.  Aborting.") );
      }
    }
  }

  /// We know we are in a concept definition.  Collect appropriate information.
  AST_Concept & ProcessConcept(size_t & pos, AST_Scope & cur_scope) {
    AST_Concept & concept = cur_scope.NewChild<AST_Concept>();

    // A concept must begin with its name.
    RequireID(pos, "Concept declaration must be followed by name identifier.");
    concept.name = tokens[pos++].lexeme;

    // Next, must be a colon...
    RequireChar(':', pos++, "Concept names must be followed by a colon (':').");

    // And then a base-class name.
    RequireID(pos, "Concept declaration must include name of base class.");
    concept.base_name = tokens[pos++].lexeme;

    Debug("Defining concept '", concept.name, "' with base class '", concept.base_name, "'.");

    // Next, must be an open brace...
    RequireChar('{', pos++, "Concepts must be defined in braces ('{' and '}').");

    // Loop through the full definition of concept, incorporating each entry.
    while ( AsChar(pos) != '}' ) {
      // Entries can be a "using" statement, a function definition, or a variable definition.
      RequireID(pos, "Concept members can be either functions, variables, or using-statements.");

      ElementInfo new_element;

      if (tokens[pos].lexeme == "using") {              // ----- USING!! -----
        pos++;  // Move past "using"
        RequireID(pos, "A 'using' command must first specify the new type name.");

        new_element.name = ProcessType(pos);      // Determine new type name being defined.
        RequireChar('=', pos++, "A using statement must provide an equals ('=') to assign the type.");
        new_element.type = ProcessCode(pos);      // Determine code being assigned to.
        concept.typedefs.push_back(new_element);
      }
      else {
        // Start with a type...
        new_element.type = ProcessType(pos);

        // Then an identifier.
        RequireID(pos, "Functions and variables in concept definition must provide identifier after type name.");
        new_element.name = tokens[pos++].lexeme;

        // If and open-paren follows the identifier, we are defining a function, otherwise it's a variable.
        if (AsChar(pos) == '(') {                              // ----- FUNCTION!! -----
          pos++;  // Move past paren.

          new_element.params = ProcessParams(pos);       // Read the parameters for this function.

          RequireChar(')', pos++, "Function arguments must end with a close-parenthesis (')')");
          new_element.attributes = ProcessIDList(pos);   // Read in each of the function attributes, if any.

          char fun_char = AsChar(pos++);

          if (fun_char == '=') {  // Function is "= default;" or "= required;"
            RequireID(pos, "Function must be assigned to 'required' or 'default'");
            std::string fun_assign = AsLexeme(pos++);
            if (fun_assign == "required" || fun_assign == "default") new_element.special_value = fun_assign;
            else Error(pos, "Functions can only be set to 'required' or 'default'");
            RequireChar(';', pos++, emp::to_string(fun_assign, "functions must end in a semi-colon."));
          }
          else if (fun_char == '{') {  // Function is defined in place.
            new_element.default_code = ProcessCode(pos, false, true);  // Read the default function body.

            Debug("   and code: ", new_element.default_code);

            RequireChar('}', pos, emp::to_string("Function body must end with close brace ('}') not '",
                                                  AsLexeme(pos), "'."));
            pos++;
          }
          else {
            Error(pos-1, "Function body must begin with open brace or assignment ('{' or '=')");
          }

          concept.functions.push_back(new_element);

        } else {                                                 // ----- VARIABLE!! -----
          if (AsChar(pos) == ';') { pos++; } // Does the variable declaration end here?
          else {                             // ...or is there a default value for this variable?
            // Determine code being assigned from.
            new_element.default_code = ProcessCode(pos);
          }

          concept.variables.push_back(new_element);

        }
      }
    }

    pos++;  // Skip closing brace.
    RequireChar(';', pos++, "Concept definitions must end in a semi-colon.");

    return concept;
  }
  
  void Process() {
    size_t pos;
    ProcessTop(pos, ast_root);
  }

  /// Print the state of the lexer used for code generation.
  void PrintLexerState() { lexer.Print(); }

  /// Print the set of tokens loaded in from the input file.
  void PrintTokens() {
    for (size_t pos = 0; pos < tokens.size(); pos++) {
      std::cout << pos << ": "
                << lexer.GetTokenName(tokens[pos])
                << " : \"" << AsLexeme(pos) << "\""
                << std::endl;
    }
  }

  /// Setup debug mode (with verbose printing).
  void SetDebug(bool in_debug=true) { debug = in_debug; }

};