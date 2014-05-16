# This file is Copyright (C) 2014 Christoph Reichenbach
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the
#   Free Software Foundation, Inc.
#   59 Temple Place, Suite 330
#   Boston, MA  02111-1307
#   USA
#
# The author can be reached as "creichen" at the usual gmail server.


import sys

class TemplateFile(object):
    def __init__(self, file_name):
        with open(file_name) as f:
            content = f.readlines()
        holes = dict()
        for i in range(0, len(content)):
            content[i] = content[i].rstrip()
            if content[i].startswith('$$') and content[i].strip().endswith('$$'):
                hole_name = content[i].strip()[2:-2]
                assert hole_name not in holes, ('Template file "%s" contains hole "%s" more than once!' % (file_name, hole_name))
                holes[hole_name] = i

        self.content = content
        self.holes = holes
        assert len(holes) > 0, ('Template file "%s" contains no holes!' % file_name)

    def printFile(self, pmap):
        '''
        Prints after plugging holes
        '''
        lines = self.content
        hole_keys = self.holes.keys()
        for k,v in pmap.iteritems():
            assert k in self.holes, ('Hole "%s" not defined in %s' % (k, self.holes.keys()))
            lines[self.holes[k]] = v
            hole_keys.remove(k)

        assert len(hole_keys) == 0, ('Not all holes plugged: %s missing' % hole_keys)
        print '/* ** AUTOMATICALLY GENERATED.  DO NOT MODIFY. ** */'
        for line in lines:
            print line

def strjoin(l):
    return '\n'.join(l) + '\n'


########################################
# Syntactic productions
class ASTGen(object):
    '''
    General-purpose class for generating AST generation code
    '''
    def __init__(self):
        pass

    def sub(self):
        '''Recursively yield all ASTGens contained within'''
        return []

    def selfAndSub(self):
        return [self] + self.sub()

    def valueNode(self):
        '''Returns either None or a pair (ctype, fieldname) to represent values'''
        return None

    def getASTName(self):
        '''Returns AST tag base name, if any'''
        return None

    def getASTFullName(self):
        '''Returns AST tag full name, if any'''
        return None

    def getBuiltinName(self):
        '''Returns builtin operation name if this node represents a builtin operation'''
        return None

    def getNT(self):
        '''get associated nonterminal, if any'''
        return None

    def hasASTRepresentation(self):
        '''Is there a valid unique AST node type and and ASTFullName reserved for this paticular entity?'''
        return False


########################################
# Syntactic productions

class Regexpr(object):
    def __init__(self, re, processing_expr, flags, term):
        self.re = re;
        self.expr = processing_expr;
        self.flags = flags
        self.term = term

    def str(self):
        term = self.term
        body = [];

        if self.expr is not None:
            body.append('\tyylval.' + term.varname + ' = ' + self.expr + ';');

        if self.flags is not None:
            pass  # fixme: maintain this information for unparsing

        body.append('\treturn ' + term.getTokenID() + ';');
        
        return self.re + ' {\n' + '\n'.join(body) + '\n}\n'


def ResultVar(v):
    '''
    `recognise' helper: Decodes a ResultVar into that variables address
    '''
    if v is None:
        return 'NULL'
    return v

def ResultVarRef(v):
    '''
    `recognise' helper: Decodes a ResultVar into that variables address
    '''
    if v is None:
        return 'NULL'
    return '&' + v



class Term(ASTGen):
    all = set()
    stringterms = dict()

    '''
    Represents a terminal given by regexps and a means for handling the regexps.
    '''
    def __init__(self, name, varname, c_type = None, token_id = None):
        ASTGen.__init__(self)
        self.name = name # may be None for single-char terminals without any processing
        self.c_type = c_type
        self.varname = varname
        self.regexps = []
        self.priority = 0
        self.format_string = 'ERROR'
        self.is_stringterm = False
        self.error_name = None
        if token_id is None:
            if self.name is not None:
                self.token_id = 'T__' + self.name
            #self.token_id = self.name
        else:
            self.token_id = token_id
        Term.all.add(self)

    def __hash__(self):
        return hash(self.token_id)

    def __eq__(self, other):
        return issubclass(type(other), Term) and self.token_id == other.token_id

    def __str__(self):
        if self.name is None:
            return self.token_id
        return self.name

    def hasASTRepresentation(self):
        return True

    def setErrorName(self, en):
        self.error_name = en
        return self

    def setFormatString(self, fs):
        '''Format string (e.g., '%p') to be used for AST unparsing'''
        self.format_string = fs
        return self

    def getFormatString(self):
        return self.format_string

    def errorDescription(self):
        if self.name is None: # simple token?
            return self.token_id
        if self.error_name is not None:
            return self.error_name
        return str(self)

    def setIsStringterm(self):
        self.is_stringterm = True

    def resultStorage(self):
        if  self.is_stringterm:
            return None
        else:
            return 'ast_node_t *'

    def resultStorageInit(self):
        return 'NULL';

    def freeResultStorage(self):
        return None

    def recognise(self, resultvar):
        '''
        Generates pair (stmt-list, expr) where the expr after the stmt-list is true iff
        the production was recognised.
        @param resultvar: Any intermediate result (e.g., ast_node_t) must be assigned to resultvar
        '''
        return ([], 'accept(%s, %s)' % (self.getTokenID(), ResultVarRef(resultvar)))

    def pushBack(self, resultvar):
        return 'push_back(%s, %s)' % (self.getTokenID(), ResultVar(resultvar))

    def getASTName(self):
        if self.name is None:
            return self.varname.upper()
        return self.name

    def getASTFullName(self):
        return 'AST_VALUE_' + self.getASTName()

    def hasSymbolicTokenID(self):
        '''
        Are we using a symbolic (non-literal-character) token for this?
        '''
        return self.name is not None

    def getTokenID(self):
        '''
        Retrieves a suitable identifier for the token represented herein, or a literal character, if appropriate
        '''
        return self.token_id

    def addRegexp(self, regexp, processing_expr, flags=None):
        '''
        Adds one particular regexp for this terminal
        @param regexp: The regexp to process
        @param processing_expr: an expression that computes the yylval (may reference yytext)
        @param flag: Name (!) of an optional AST node flag to set
        '''
        self.regexps.append(Regexpr(regexp, processing_expr, flags, self))

    def setPriority(self, nr):
        '''Changes NT priority; used to order results in file (ascending)'''
        self.priority = nr
        return self

    def valueNode(self):
        if self.c_type == None:
            return None
        return (self.c_type, self.varname)

    def generateASTGen(self, genVar):
        return genVar(self, 0)


used_names = set()

def StringTerm(strt):
    must_escape = "()*+.|[]?'\\"

    def escape(s):
        return ''.join(['\\' + c if c in must_escape else c
                        for c in s])

    if strt in Term.stringterms:
        return Term.stringterms[strt]
    if len(strt) == 1:
        name = None
    else:
        name = strt.upper()
        stdtrans = { '>' : 'GT',
                     '<' : 'LT',
                     '=' : 'EQ',
                     '!' : 'BANG',
                     '*' : 'STAR',
                     '+' : 'PLUS',
                     '-' : 'DASH',
                     '/' : 'SLASH',
                     '&' : 'AMP',
                     '#' : 'HASH',
                     '%' : 'PERCENT',
                     '@' : 'AT',
                     ',' : 'COMMA',
                     '~' : 'TILDE',
                     ':' : 'COLON',
                     ';' : 'SEMICOLON',
                     '.' : 'PERIOD',
                     '?' : 'QMARK' }
        def trans(t):
            if t in stdtrans:
                return stdtrans[t]
            if t in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789':
                return t
            return ''
        name = ''.join(trans(c) for c in name)
        if name == '':
            name = 'X'
        if name in used_names:
            i = 0
            while name + str(i) in used_names:
                i += 1
            name = name + str(i)
        used_names.add(name)
    if name is None:
        token_id = "'" + strt + "'"
    else:
        token_id = 'T_L_' + name
    retval = Term(name, None, token_id = token_id)
    retval.addRegexp('"' + escape(strt) + '"', None, None)
    retval.setIsStringterm()
    retval.setErrorName("'%s'" % strt)
    Term.stringterms[strt] = retval
    return retval.setPriority(-1)


class NT(ASTGen):
    '''
    Represents a single nonterminal.
    '''
    def __init__(self, name, error_description_name):
        ASTGen.__init__(self)
        self.name = name
        self.error_description_name = error_description_name
        self.fail_handler = None

    def __call__(self, index):
        '''
        Construct an indexed channel
        '''
        return NTSub(self, index)

    def getNT(self):
        '''get associated nonterminal, if any'''
        return self

    def setFailHandlerUntil(self, term):
        '''
        Designates this nonterminal as being able to recover from parse errors by ignoring everything until the next `term'
        '''
        self.fail_handler = term.getTokenID()

    def __repr__(self):
        return self.name

    def __str__(self):
        return self.name

    def resultStorage(self):
        #return 'ast_node_t *'
        if self not in Rule.all:
            return None
        return Rule.all[self][0].resultStorage()

    def resultStorageInit(self):
        #return 'NULL'
        return Rule.all[self][0].resultStorageInit()

    def parseFunctionName(self):
        return 'parse_' + str(self)

    def freeResultStorage(self):
        return None

    def errorDescription(self):
        return self.error_description_name

    def getIndex(self):
        return 0

    def printHandleFail(self, p):
        '''Emit code for handling a failed rule parse'''
        if self.fail_handler is not None:
            p('clear_parse_error(%s);' % self.fail_handler)
            p('return %s(%s);' % (self.parseFunctionName(), 'result'))
        else:
            p('return 0;')

    def recognise(self, resultvar):
        return ([], '%s(%s)' % (self.parseFunctionName(), ResultVarRef(resultvar)))

    def generateASTGen(self, genVar):
        return genVar(self.getNT(), self.getIndex())



class NTSub(NT):
    '''
    Represents an NT reference in a rule construction, to the index-th occurrence of the nonterminal (base 0)
    '''
    def __init__(self, owner, index):
        NT.__init__(self, owner.name, owner.error_description_name)
        self.owner = owner
        self.index = index

    def getIndex(self):
        return self.index

    def getNT(self):
        return self.owner

    def __call__(self, index):
        raise Exception("Can't sub-index NTSub")


tmp_count = 0
def TmpVar():
    return 'tmp_' + str(tmp_count)
    

class Repeat(ASTGen):
    '''
    Repeats a potentially infinite repetition of a nonterminal.
    '''

    def __init__(self, nt, separator=None):
        self.nt = nt
        self.separator = separator

    def __str__(self):
        return 'repeat_' + str(self.nt)

    def storesResult(self):
        return 'ast_node_t **'

    def freeResultStorage(self):
        return 'free_vector'

    def recognise(self, resultvar):
        tmpvar = TmpVar()
        stmts = [
            'ast_node_t *tmp_' + tmpvar + ';',
            'while (%s) {' % self.nt.recognise(tmpvar)[1],
            '\tadd_to_vector(%s, %s);' % (ResultVarRef(resultvar), tmpvar)]
        if (self.separator):
            stmts.append('\tif (!%s) {;' % self.separator.recognise(None)[1])
            stmts.append('\t\tbreak;')
            stmts.append('\t}')
        stmts.append('}')
        return (stmts, None)


########################################
# Construction specifications
#
# Construction specs build AST nodes after matching a sequence of tokens.  The following rule
# constructions are permissible:
#
# Attr('s')			Generates an attribute of the specified string, encoded as a bit mask
# AddAttribute(n, attr)		Adds attribute `attr' to node n
# Update(n0, index, n1)		Updates the specified channel index of `n0' to value `n1', returns updated `n0'
# Cons('name', [ns])		Constructs the specified AST node together with the specified channels (see below) as children,
# n				Directly return the specified node
#
# Nodes `n' are constructor references to the lhs of a rule.
# They can be of the following kinds:
#  - NT    (meaning that there's a reference to the single NT)
#  - NT(i) (meaning a reference to the ith NT)


class ASTCons(ASTGen):
    '''
    Abstract AST construction superclass
    '''
    def __init__(self):
        ASTGen.__init__(self)
        pass

    def getASTFullName(self):
        return 'AST_NODE_' + self.getASTName()

    def generateASTGen(self, genVar):
        '''
        Return code to generate an AST fragment.
        genVar(nt, index) can be used to get the names of variables with previously parsed AST fragments.
        '''
        raise Exception('Unsupported ASTCons in %s' % type(self))

    def resultStorage(self):
        return 'ast_node_t *'

    def resultStorageInit(self):
        return 'NULL'



class NULL(ASTCons):
    def __init__(self):
        ASTCons.__init__(self)

    def generateASTGen(self, genVar):
        return 'NULL'

NULL = NULL()

class Cons(ASTCons):
    cons_names = set()

    def __init__(self, cons, channels, attrs = []):
        ASTCons.__init__(self)
        self.consname = cons
        Cons.cons_names.add(cons)
        self.channels = channels
        self.attrs = attrs

    def __hash__(self):
        return hash(self.consname)

    def __eq__(self, other):
        if not issubclass(type(other), Cons):
            return False
        return other.consname == self.consname

    def sub(self):
        r = []
        for c in self.channels:
            r = r + [c] + c.sub()
        return r

    def getASTName(self):
        return self.consname

    def hasASTRepresentation(self):
        return True

    def generateASTGen(self, genVar):

        args = ''
        if len(self.channels):
            args = ', ' + ', '.join(c.generateASTGen(genVar) for c in self.channels)

        return ('ast_node_alloc_generic(%s, %d%s)'
                % (self.getASTFullName(),
                   len(self.channels),
                   args))


class Builtin(ASTCons):
    def __init__(self, builtin_name):
        ASTCons.__init__(self)
        self.name = builtin_name

    def valueNode(self):
        return ('int', 'ident')

    def getASTName(self):
        return 'ID'

    def getASTFullName(self):
        return 'AST_VALUE_' + self.getASTName()

    def getBuiltinName(self):
        return self.name

    def getBuiltinFullName(self):
        return 'BUILTIN_OP_' + self.getBuiltinName()

    def generateASTGen(self, genVar):
        return 'value_node_alloc_generic(AST_VALUE_ID, (ast_value_union_t) { .ident = %s })' % self.getBuiltinFullName()


class _Attr(ASTCons):
    attr_map = {}

    '''
    AST attribute construction: constructs a value to be encoded into the cons' attribute set
    '''
    def __init__(self, attrname):
        ASTCons.__init__(self)
        self.attrname = attrname
        self.attr_bit = -1
        _Attr.attr_map[attrname] = self

    def getTagName(self):
        return self.attrname

    def getFullTagName(self):
        return 'AST_FLAG_%s' % self.getTagName()

    def generateASTGen(self, genVar):
        return self.getFullTagName()

    def resultStorage(self):
        return 'unsigned int'

    def resultStorageInit(self):
        return '0';


def Attr(attrname):
    if not attrname in _Attr.attr_map:
        _Attr.attr_map[attrname] = _Attr(attrname)
    return _Attr.attr_map[attrname]


class Update(ASTCons):
    '''
    AST child node update construction
    '''
    def __init__(self, baseobj, index, channel):
        ASTCons.__init__(self)
        self.baseobj = baseobj
        self.index = index
        self.channel = channel

    def sub(self):
        return self.baseobj.sub()

    def generateASTGen(self, genVar):
        return 'node_update(%s, %d, %s)' % (self.baseobj.generateASTGen(genVar),
                                            self.index,
                                            self.channel.generateASTGen(genVar))

class AddAttribute(ASTCons):
    '''
    Add attribute tags to node
    '''
    def __init__(self, baseobj, attr):
        ASTCons.__init__(self)
        self.baseobj = baseobj
        self.attr = attr

    def sub(self):
        return self.baseobj.sub()

    def generateASTGen(self, genVar):
        return 'node_add_attribute(%s, %s)' % (self.baseobj.generateASTGen(genVar),
                                               self.attr.generateASTGen(genVar))

########################################
# Rule object

class Rule(object):
    all = dict()

    def __init__(self, nt, rhs, astgen):
        self.nt = nt
        self.rhs = rhs
        self.astgen = astgen

        if nt in Rule.all:
            Rule.all[nt].append(self)

            def tyMatch(a, b):
                return a is None or b is None or a == b

            assert tyMatch(Rule.all[nt][0].resultStorage(), self.resultStorage()), 'Rules for nonterminal %s disagree about result type' % nt
        else:
            Rule.all[nt] = [self]
        for i in range(0, len(rhs)):
            if type(rhs[i]) is str:
                rhs[i] = StringTerm(rhs[i])

    def resultStorage(self):
        return self.astgen.resultStorage()

    def resultStorageInit(self):
        return self.astgen.resultStorageInit()


########################################
# Rule specifications

# Terminals
REAL = Term('REAL', 'real', 'double').setErrorName('real number').setFormatString('%f')
REAL.addRegexp('((({DIGIT}*"."{DIGIT}+)|({DIGIT}+"."))([eE][+-]?{DIGIT}+)?)|({DIGIT}+[eE][+-]?{DIGIT}+)',
               'strtod(yytext, NULL)')
REAL.addRegexp('0x(({HEXDIGIT}*"."{HEXDIGIT}+)|({HEXDIGIT}+"."))([pP][+-]?{DIGIT}+)',
               'strtod(yytext, NULL)', 'HEX_REPR')


INT = Term('INT', 'num', 'signed long int').setErrorName('integer').setFormatString('%li')
INT.addRegexp('0x{HEXDIGIT}+',
              'strtol(yytext + 2, NULL, 16)', 'HEX_REPR')
INT.addRegexp('{DIGIT}+',
              'strtol(yytext, NULL, 10)')


STRING = Term('STRING', 'str', 'char *').setErrorName('string').setFormatString('\\"%s\\"')
STRING.addRegexp('\\"(\\\\.|[^\\"\\\\])*\\"',
                 'unescape_string(yytext)')


ID = Term('NAME', 'str', 'char *').setPriority(10).setErrorName('identifier').setFormatString('%s')
ID.addRegexp('{IDENTIFIER}', 'mk_unique_string(yytext)')


# Nonterminals
STMT = NT('stmt', 'statement')
STMT.setFailHandlerUntil(StringTerm(';'))
EXPR = NT('expr', 'expression')
EXPR1 = NT('expr1', 'expression')
EXPR2 = NT('expr2', 'expression')
EXPR3 = NT('expr3', 'expression')
VALEXPR = NT('valexpr', 'value')
TY = NT('ty', 'type specifier')

# LIMITATIONS:
# - no left recursion (direct or indirect; feel free to add the standard transformation algorithm, though)
# - no disambiguation across sub-rules:
#     A ::= B | C | D
#     B ::= E '+' A
#     C ::= D '-' A
#     D ::= '1' | '2'
#     E ::= D
#   this will NOT work correctly:  If you have "1 - 2", the parser will issue a syntax error, because
#   when it backtracks from B, it will have already converted the D to an E, which the C can't parse.
#   In other words, backtracking is limited.

rules = [
    Rule(TY,	['var'],					Attr('VAR')),
    Rule(TY,	['obj'],					Attr('OBJ')),
    Rule(TY,	['int'],					Attr('INT')),
    Rule(TY,	['real'],					Attr('REAL')),

    Rule(STMT,[EXPR, ';'],EXPR), # just for testing

    # Rule(STMT,	[VARDEF, ';'],					VARDEF),
    # Rule(STMT,	[VARDEF, Opt("oe", ['=', EXPR]), ';'],		Update(VARDEF, 2, P("oe"))),

    Rule(EXPR,     [EXPR1, '==', EXPR1],			Cons('FUNAPP', [Builtin('TEST_EQ'), EXPR1(0), EXPR1(1)])),
    Rule(EXPR,     [EXPR1, '<', EXPR1],				Cons('FUNAPP', [Builtin('TEST_LT'), EXPR1(0), EXPR1(1)])),
    Rule(EXPR,     [EXPR1, '<=', EXPR1],			Cons('FUNAPP', [Builtin('TEST_LE'), EXPR1(0), EXPR1(1)])),
    Rule(EXPR,     [EXPR1, '>', EXPR1],				Cons('FUNAPP', [Builtin('TEST_LE'), EXPR1(1), EXPR1(0)])),
    Rule(EXPR,     [EXPR1, '>=', EXPR1],			Cons('FUNAPP', [Builtin('TEST_LT'), EXPR1(1), EXPR1(0)])),
    Rule(EXPR,	   [EXPR1],					EXPR1),

    Rule(EXPR1,    [EXPR2, '+', EXPR1],				Cons('FUNAPP', [Builtin('ADD'), EXPR2, EXPR1])),
    Rule(EXPR1,    [EXPR2, '-', EXPR1],				Cons('FUNAPP', [Builtin('SUB'), EXPR2, EXPR1])),
    Rule(EXPR1,	   [EXPR2],					EXPR2),

    Rule(EXPR2,    [VALEXPR, '*', EXPR2],			Cons('FUNAPP', [Builtin('MUL'), VALEXPR, EXPR2])),
    Rule(EXPR2,    [VALEXPR, '/', EXPR2],			Cons('FUNAPP', [Builtin('DIV'), VALEXPR, EXPR2])),
    Rule(EXPR2,	   [VALEXPR],					VALEXPR),

    Rule(VALEXPR, ['[', EXPR, ':', TY , ']'], AddAttribute(EXPR, TY)),
    
    Rule(VALEXPR,  [INT],					INT),
    Rule(VALEXPR,  [STRING],					STRING),
    Rule(VALEXPR,  [REAL],					REAL),
    Rule(VALEXPR,  [ID],					ID),
    Rule(VALEXPR,  ['(', EXPR, ')'],				EXPR),
]

BITS_FOR_NODE_TYPE_TOTAL = 16
TOTAL_AST_NODE_TYPES = len(Cons.cons_names) + len(Term.all) + 2 # 2 for identifiers and `invalid'
BITS_FOR_FLAGS = BITS_FOR_NODE_TYPE_TOTAL - TOTAL_AST_NODE_TYPES.bit_length()
AST_NODE_MASK = 0xffff >> BITS_FOR_FLAGS
assert BITS_FOR_FLAGS > len(_Attr.attr_map), "Not enough bits left to store all attributes (need %d, have %d)" % (len(_Attr.attr_map), BITS_FOR_FLAGS)

i = TOTAL_AST_NODE_TYPES.bit_length()
for attr in _Attr.attr_map.itervalues():
    attr.attr_bit = i
    i += 1

exported_nonterminals = [EXPR]

########################################
# Processing

# TOKENS, VALUES, PARSER_DECLS
# -> parser.h
lexer_header_template = TemplateFile('parser.template.h')

# RULES
# -> lexer.l
lexer_template = TemplateFile('lexer.template.l')

# NODE_TYPES, AV_VALUE_GETTERS (extract value from value node), AV_FLAGS, VALUE_UNION, BUILTIN_IDS
# -> ast.h
ast_header_template = TemplateFile('ast.template.h')

# VALUE_TOKEN_DECODING, PARSING
# -> ast.h
parser_template = TemplateFile('parser.template.c')


########################################
# Gen lexer/parser header

def printLexerParserHeader():
    tokens = []
    values = { 'node' : 'ast_node_t*' }

    i = 102
    for t in sorted(list(Term.all)):
        if t.varname is not None:
            if t.varname in values:
                assert values[t.varname] == t.c_type, ('Inconsistent types for var %s: %s vs %s' %
                                                       (t.varname, t.c_type, values[t.varname]))
            else:
                values[t.varname] = t.c_type

        if t.hasSymbolicTokenID():
            tokens.append(t.getTokenID() + ' = ' + str(i))
            i += 1

        vlist = []
        for name, ty in values.iteritems():
            # prettify
            while ty.endswith('*'):
                ty = ty[0:-1]
                name = '*' + name;
            vlist.append(ty + ' ' + name)

    ppl = []
    def pprint(s):
        ppl.append(s)

    for lhs in Rule.all.iterkeys():
        if lhs in exported_nonterminals:
            printRuleHeader(lhs, pprint, ';')

    lexer_header_template.printFile({ 'TOKENS': ',\n'.join('\t' + t for t in tokens) + '\n',
                                      'VALUES': strjoin('\t' + v + ';' for v in vlist),
                                      'PARSER_DECLS': '\n'.join(ppl) })

########################################
# Gen lexer

def printLexer():
    rules = []

    terms = list(Term.all)
    terms.sort(lambda a, b: a.priority - b.priority)

    for term in terms:
        for re in term.regexps:
            rules.append(re.str())

    lexer_template.printFile({ 'RULES': strjoin(rules) })


########################################
# Gen AST header

def addUnique(m, n, v):
    if n in m:
        assert m[n] == v, ('Mismatching entries for key %s: %s vs %s' %
                           (n, v, m[n]))
    else:
        m[n] = v

def printASTHeader():
    values = { }   # { 'ident' : 'int' }
    idgetter = { 'ID' : 'ident' }

    builtin_names = set()

    max_astname_len = 0
    max_builtin_name_len = 0
    value_nty_names = set()
    nonvalue_nty_names = set()

    for rules in Rule.all.itervalues():
        for rule in rules:
            for astgen in rule.astgen.selfAndSub():
                vn_info = astgen.valueNode()
                addset = nonvalue_nty_names

                if vn_info is not None:
                    (cty, cname) = vn_info
                    addUnique(values, cname, cty)
                    addset = value_nty_names
                    if not (type(astgen) == Builtin) :
                        gname = astgen.name
                        if gname is None:
                            gname = cname.upper()
                        addUnique(idgetter, gname, cname)

                if astgen.getASTName() is not None:
                    name = astgen.getASTFullName()
                    if len(name) > max_astname_len:
                        max_astname_len = len(name)
                    addset.add(name)

                if astgen.getBuiltinName() is not None:
                    n = astgen.getBuiltinFullName()
                    builtin_names.add(n)
                    if len(n) > max_builtin_name_len:
                        max_builtin_name_len = len(n)

    node_ty_decls = []
    node_ty_nr = [0]
    def addNodeTyDecl(nodety, number = None):
        if number is None:
            number = node_ty_nr[0]
            node_ty_nr[0] += 1
        node_ty_decls.append('#define ' + nodety + (' ' * (max_astname_len - len(nodety))) + ' 0x%02x' % number)

    addNodeTyDecl('AST_ILLEGAL') # 0
    addNodeTyDecl('AST_NODE_MASK', AST_NODE_MASK)
    for n in value_nty_names:
        addNodeTyDecl(n)
    addNodeTyDecl('AST_VALUE_MAX', node_ty_nr[0] - 1);
    for n in nonvalue_nty_names:
        addNodeTyDecl(n)
        
    builtin_id_nr = [-1]
    builtin_names = list(builtin_names)
    builtin_names.sort()
    builtin_id_decls = []

    def addBuiltinName(name, number = None):
        if number is None:
            number = builtin_id_nr[0]
            builtin_id_nr[0] -= 1
        builtin_id_decls.append('#define ' + name + (' ' * (max_builtin_name_len - len(name))) + ' %d' % number)
    for n in builtin_names:
        addBuiltinName(n)

    attrmap = {}
    attr_name_len = 0
    for attr in _Attr.attr_map.itervalues():
        name = attr.getFullTagName()
        if len(name) > attr_name_len:
            attr_name_len = len(name)
        attrmap[name] = "0x%04x" % (1 << attr.attr_bit);
        
    ast_header_template.printFile({
        'BUILTIN_IDS'		: '\n'.join(builtin_id_decls) + '\n\n#define BUILTIN_OPS_NR ' + str(len(builtin_names)) + '\n',
        'NODE_TYPES'		: '\n'.join(node_ty_decls),
        'AV_VALUE_GETTERS'	: '\n'.join('#define AV_%s(n) (((ast_value_node_t *)(n))->v.%s)' % (idn, idv) for (idn, idv) in idgetter.iteritems()),
        'AV_FLAGS'		: '\n'.join('#define %s %s %s' % (name, ' ' * (attr_name_len - len(name)), v) for (name, v) in attrmap.iteritems()),
        'VALUE_UNION'		: '\n'.join('\t' + cty + ' ' + cn + ';' for (cn, cty) in values.iteritems())
    })

########################################
# Gen parser

def transitiveClosure(map_to_set):
    changed = True
    while changed:
        changed = False
        old_map_to_set = dict(map_to_set)
        for key, valueset in map_to_set.iteritems():
            for value in set(valueset):
                if value in map_to_set:
                    for subvalue in map_to_set[value]:
                        if subvalue not in valueset:
                            changed = True
                            valueset.add(subvalue)
    return map_to_set

def getIncCountMap(m, key):
    if key in m:
        n = m[key] + 1
        m[key] = n
        return n
    else:
        m[key] = 0
        return 0

def decisionTree(base_nt, depth, ruleprods, dtmap):
    '''
    Takes in rule/production pairs.
    Returns pairs of tree = (completed-rule, repeat-rule, { (recogniser, resultindex) : tree })
    where:
      - completed-rule and repeat-rule are mutually exclusive (the other must be Null)
      - resultindex is a zero-based index counting the occurrence of the given (non)terminal
    '''
    var_index_counter_map = {} #local cache of dtmap: ensures that if we see nt for multiple subrules, they get the same index
    def getIndex(key):
        if key not in var_index_counter_map:
            var_index_counter_map[key] = (key, getIncCountMap(dtmap, key))
        return var_index_counter_map[key]

    final_result = None
    repeat_handler = None
    keyed_results = {}
    for (rule, prod) in ruleprods:
        if len(prod) == 0:
            if final_result is None:
                final_result = rule
            else:
                raise Exception('Multiple seemingly equivalent rules of size %d for nonterminal %s' % (depth, base_nt))
        else:
            cont = (rule, prod[1:])
            index = getIndex(prod[0])
            if issubclass(type(prod[0]), Repeat):
                if repeat_handler is None:
                    repeat_handler = cont
                else:
                    raise Exception('Multiple seemingly equivalent repeat-handler rules at depth %d for nonterminal %s' % (depth, base_nt))
            elif index in keyed_results:
                keyed_results[index].append(cont)
            else:
                keyed_results[index] = [cont]

    if repeat_handler is not None and final_result is not None:
        raise Exception('conflicting end-of-rule and repeat-rule at rules of size %d for nonterminal %s' % (depth, base_nt))

    result_tree = {}
    for key, conts in keyed_results.iteritems():
        dt = decisionTree(base_nt, depth + 1, conts, dict(dtmap))
        result_tree[key] = dt
    return (final_result, repeat_handler, result_tree)

def getIncCountMap(m, key):
    if key in m:
        n = m[key] + 1
        m[key] = n
        return n
    else:
        m[key] = 0
        return 0


def printRuleHeader(lhs, p, suffix=''):
    pfx = ''
    if lhs not in exported_nonterminals:
        pfx = 'static '
    p('%sint' % pfx)
    p('%s(%s *result)%s' % (lhs.parseFunctionName(), lhs.resultStorage(), suffix))


def buildAST(endrule, bound_variables, p, getEnv):
    bound_variables = set(bound_variables)
    def getVar(k, index):
        n = getEnv(k, index)
        if n is None:
            raise Exception('Unknown/unsupported reference to previous parse result for nonterminal %s.%d' %(k, index))
        if n in bound_variables:
            bound_variables.remove(n)
            return n
        else:
            # Used more than once
            return 'ast_node_clone(%s)' % n

    p('*result = %s;' % endrule.astgen.generateASTGen(getVar));
    # Free all unused parse results
    for n in bound_variables:
        p('ast_node_free(%s, 1);' % n)


def buildParseRules(rules):
    results = []
    def pprint(s):
        results.append(s)

    recursions = {}

    for lhs, rulelist in rules.iteritems():
        printRuleHeader(lhs, pprint, ';')
        for rule in rulelist:
            if len(rule.rhs):
                nt = rule.rhs[0].getNT()
                if nt is not None:
                    if lhs in recursions:
                        recursions[lhs].add(nt)
                    else:
                        recursions[lhs] = set([nt])

    pprint('')

    recursions = transitiveClosure(recursions)
    left_recursions = set()
    for k, v in recursions.iteritems():
        if k in v:
            left_recursions.add("%s in %s" % (k, v))
    

    assert len(left_recursions) == 0, 'Left recursion detected: %s' % left_recursions
        
    headers = []
    implementations = []

    # generate code
    for lhs, rulelist in rules.iteritems():
        env = {} # variable environment

        stmts = []

        dt = decisionTree(lhs, 0, [(rule, rule.rhs) for rule in rulelist], {})

        #pprint("// DECISION TREE for %s =%s" % (lhs.parseFunctionName(), dt))

        error_label = lhs.parseFunctionName() + '_fail';

        def gen(dt, p, previously_on_path=[]):
            '''
            dt: decision (sub)tree
            p: print function
            previoulsly_on_path: all rhs elements recognised up to this one (used for backtracking)
            '''
            (endrule, repeatrule, choices) = dt
            def pp(s):
                p('\t' + s)
            subp = p
            cont_else = False
            end = lambda _: ()

            def getEnv(k, kindex):
                index = (k, kindex)
                if index in env:
                    return env[index]
                else:
                    return None

            for (key, kindex), cont in choices.iteritems():
                #pprint("// expecting: %s from %s is %s" % ((key, kindex), env, getEnv(key, kindex)))
                (prepare_stmts, cond) = key.recognise(getEnv(key, kindex))
                for s in prepare_stmts:
                    p(s)
                if cond is not None:
                    pfx = ''
                    if cont_else:
                        pfx = '} else '
                    p(pfx + ('if (%s) {' % cond))
                    subp = pp
                    cont_else = True
                    end = lambda _: p('}')

                    gen(cont, subp, previously_on_path + [(key, kindex)])

            if len(choices) and not (repeatrule or endrule):
                # error is a possibility?
                p('} else {')
                def pp(s):
                    p('\t' + s)
                backtrack_impossible = False
                for predecessor, _ in previously_on_path:
                    if not issubclass(type(predecessor), Term):
                        backtrack_impossible = True

                if backtrack_impossible:
                    msg = ' or '.join(n.errorDescription() for (n, _) in choices.iterkeys())
                    p('\tparse_error("Syntax error in %s: expected %s after %s");' % (lhs.errorDescription(), msg, previously_on_path[-1][0].errorDescription()))
                else:
                    for n, i in previously_on_path:
                        p('\t%s;' % n.pushBack(getEnv(n, i)))

                pp('goto %s;' % error_label);
                end = lambda _: p('}')
                

            end(None)
            if repeatrule:
                (prepare_stmts, cond) = key.recognise(getEnv(key))
                for s in prepare_stmts:
                    p(s)
            if endrule:
                buildAST(endrule, [env[k] for k in previously_on_path if k in env], p, getEnv)
                p('return 1;')
                    

        printRuleHeader(lhs, pprint)
        pprint('{')

        envctr = [0]
        def allocvar(x, i, rule):
            if x.resultStorage() is None:
                return
            item = (x, i)
            if item in env:
                return
            vn = 'v_' + str(x) + '_' + str(i)
            envctr[0] += 1
            pprint('\t%s %s = %s;' % (x.resultStorage(), vn, x.resultStorageInit()))
            env[item] = vn
        for rule in rulelist:
            counters = {}
            #pprint(str(rule.rhs))
            for entry in rule.rhs:
                counter = getIncCountMap(counters, entry)
                allocvar(entry, counter, rule)

        def p(s):
            pprint('\t' + s)
        gen(dt, p)
        pprint('%s:' % error_label)
        lhs.printHandleFail(p);
        pprint('}\n\n')
    return results
    

def printParser():

    token_decoding = set()

    for rules in Rule.all.itervalues():
        for rule in rules:
            for astgen in rule.astgen.selfAndSub():
                if issubclass(type(astgen), Term) and astgen.hasSymbolicTokenID():
                    (cty, cname) = astgen.valueNode()
                    token_decoding.add((cname, # ast_value_union_t field name
                                        astgen.getASTFullName(), # AST_VALUE_...
                                        astgen.getTokenID(),   # token type
                                        cname  # yylval field name
                                    ))

    parse_rules = buildParseRules(Rule.all)

    def token_decoding_rule(t):
        (value_union_field, ast_id, token_type_tag, yylval_field) = t
        return ('\tcase %s:\n\t\t*node_ptr = value_node_alloc_generic(%s, (ast_value_union_t) { .%s = yylval.%s });\n\t\tbreak;'
                % (token_type_tag, ast_id, value_union_field, yylval_field))

    parser_template.printFile({
        'VALUE_TOKEN_DECODING':	'\n'.join(token_decoding_rule(t) for t in token_decoding),
        'PARSING':		'\n'.join(parse_rules)
    })

def printUnparser():
    value_ntys = set()
    nonvalue_ntys = set()
    builtin_names = set()
    builtins = set()

    for rules in Rule.all.itervalues():
        for rule in rules:
            for astgen in rule.astgen.selfAndSub():
                vn_info = astgen.valueNode()
                addset = nonvalue_ntys

                if vn_info is not None:
                    addset = value_ntys

                if astgen.getASTName() is not None:
                    if astgen.hasASTRepresentation():
                        addset.add(astgen)

                if astgen.getBuiltinName() is not None:
                    if astgen.getBuiltinName() not in builtin_names:
                        # eliminate dupes
                        builtin_names.add(astgen.getBuiltinName())
                        builtins.add(astgen)

    def printID(n):
        return '\tcase %s:\n\t\tfputs("%s", file);\n\t\tbreak;' % (n.getBuiltinFullName(), n.getBuiltinName())

    def printTag(n):
        return '\tcase %s:\n\t\tfputs("%s", file);\n\t\tbreak;' % (n.getASTFullName(), n.getASTName())

    def printVNode(n):
        return '\tcase %s:\n\t\tfprintf(file, "%s", node->v.%s);\n\t\tbreak;' % (
            n.getASTFullName(),
            n.getFormatString(),
            n.varname
        )

    def printFlag(n):
        return '\tif (ty & %s) fputs("@%s", file);\n' % (n.getFullTagName(), n.getTagName())

    parser_template = TemplateFile('unparser.template.c')
    parser_template.printFile({
        'PRINT_TAGS': '\n'.join(printTag(n) for n in value_ntys.union(nonvalue_ntys)),
        'PRINT_FLAGS': '\n'.join(printFlag(n) for n in _Attr.attr_map.itervalues()),
        'PRINT_IDS': '\n'.join(printID(n) for n in builtins),
        'PRINT_VNODES': '\n'.join(printVNode(n) for n in value_ntys) })

########################################
# Frontend

def printUsage():
    print 'usage: '
    print '\t' + sys.argv[0] + ' parser.h'
    print '\t' + sys.argv[0] + ' lexer.l'
    print '\t' + sys.argv[0] + ' ast.h'
    print '\t' + sys.argv[0] + ' parser.c'
    print '\t' + sys.argv[0] + ' unparser.c'
#    print '\t' + sys.argv[0] + ' ast.c'
    

if len(sys.argv) > 1:
    if sys.argv[1] == 'parser.h':
        printLexerParserHeader()

    elif sys.argv[1] == 'lexer.l':
        printLexer()

    elif sys.argv[1] == 'ast.h':
        printASTHeader()

    elif sys.argv[1] == 'parser.c':
        printParser()

    elif sys.argv[1] == 'unparser.c':
        printUnparser()

    else:
        printUsage()

else:
    printUsage()
