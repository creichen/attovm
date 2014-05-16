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

# assume 16 registers total

import sys

class BitPattern(object):
    '''
    Represents part of a bit pattern string, which is used to encode information (specifically register data).

    We represent bit pattern strings as lists of BitPattern objects, from msb to lsb.

    byteid: offset into a byte string (first = 0)
    bitid: offset into the byte (lsb = 0)
    bits_nr: number of bits to encode starting at bitid (inclusive)
    '''
    def __init__(self, byteid, bitid, bits_nr):
        self.byteid = byteid
        self.bitid = bitid
        self.bits_nr = bits_nr

    def strExtract(self, varname, bitoffset):
        '''
        Generates code for extracting relevant bits from `varname', starting at its `bitoffset'
        '''
        total_rightshift = bitoffset - self.bitid
        body = varname
        if total_rightshift > 0:
            body = '(' + body + ' >> ' + str(total_rightshift) + ')'
        elif total_rightshift < 0:
            body = '(' + body + ' << ' + str(-total_rightshift) + ')'

        return '%s & 0x%02x' % (body, self.maskIn())

    def maskIn(self):
        mask = 0
        for i in range(0, self.bits_nr):
            mask = ((mask << 1) | 1)

        mask = mask << self.bitid
        return mask
        
    def maskOut(self):
        return 0xff ^ self.maskIn()

    def strDecode(self, byte):
        return '(' + byte + (' & 0x%02x' % self.maskIn()) + ') >> ' + str(self.bitid)


class Arg(object):
    '''
    Represents an argument to an instruction.
    '''

    def setName(self, name):
        self.name = name

    def strName(self):
        return self.name

    def getExclusiveRegion(self):
        '''
        Determines whether the argument fully determines the contents of a particular sequence of bytes in this instruction.

        @return None or (min_inclusive, max_inclusive)
        '''
        return None

    def inExclusiveRegion(self, offset):
        exreg = self.getExclusiveRegion()
        if exreg is not None:
            (min_v, max_v) = exreg
            return offset >= min_v and offset <= max_v

    def maskOut(self, offset):
        if self.inExclusiveRegion(offset):
            return 0x00
        return 0xff

    def getBuilderFor(self, offset):
        return None

    def printCopyToExclusiveRegion(self, dataptr):
        pass

    def printDisassemble(self, dataptr, p):
        '''
        prints C code to diassemble this particular argument

        @param p: print function
        @return a tuple ([printf format strings], [args to format strings])
        '''
        return ([], [])


class Reg(Arg):
    '''
    Represents a register parameter to an Insn and describes how the register number is encoded.
    '''
    def __init__(self, bitpatterns):
        assert type(bitpatterns) is list
        self.bit_patterns = list(bitpatterns)
        self.bit_patterns.reverse()
        self.atbit = dict()

        bitoffset = 0
        for bp in self.bit_patterns:
            if bp.byteid in self.atbit:
                self.atbit[bp.byteid].append((bp, bitoffset))
            else:
                self.atbit[bp.byteid] = [(bp, bitoffset)]
            bitoffset += bp.bits_nr

    def getBuilderFor(self, offset):
        if offset in self.atbit:
            pats = self.atbit[offset]
            results = []
            name = self.strName()
            for (pat, bitoffset) in pats:
                results.append(pat.strExtract(name, bitoffset))
            return ' | '.join(results)
        return None

    def maskOut(self, offset):
        mask = 0xff
        try:
            for (pat, bitoffset) in self.atbit[offset]:
                mask = mask & pat.maskOut()
        except KeyError:
            pass
        return mask

    def strGenericName(self):
        return 'r'

    def strType(self):
        return 'int'

    def printDisassemble(self, dataptr, p):
        decoding = []
        bitoffset = 0
        for pat in self.bit_patterns:
            decoding.append('(' + pat.strDecode(dataptr + '[' + str(pat.byteid) + ']') + ('<< %d)' % bitoffset))
            bitoffset += pat.bits_nr
        p('int %s = %s;' % (self.strName(), ' | ' .join(decoding)))
        return (['%s'], ['register_names[' + self.strName() + '].mips'])


class Imm(Arg):
    '''
    Represents an immediate value as parameter.
    '''
    def __init__(self, ctype, cformatstr, bytenr, bytelen):
        self.ctype = ctype
        self.cformatstr = cformatstr
        self.bytenr = bytenr
        self.bytelen = bytelen

    def getExclusiveRegion(self):
        return (self.bytenr, self.bytenr + self.bytelen - 1)

    def strGenericName(self):
        return 'imm'

    def strType(self):
        return self.ctype

    def printCopyToExclusiveRegion(self, p, dataptr):
        p('memcpy(%s + %d, &%s, %d);' % (dataptr, self.bytenr, self.strName(), self.bytelen))

    def printDisassemble(self, dataptr, p):
        p('%s %s;' % (self.ctype, self.strName()))
        p('memcpy(&%s, %s + %d, %d);' % (self.strName(), dataptr, self.bytenr, self.bytelen))
        return ([self.cformatstr], [self.strName()])


def mkp(indent):
    '''Helper for indented printing'''
    def p(s):
        print ('\t' * indent) + s
    return p

class Insn(object):
    emit_prefix = "emit_"

    def __init__(self, name, machine_code, args):
        self.name = name
        self.machine_code = machine_code
        assert type(machine_code) is list
        self.args = args
        assert type(args) is list

        arg_type_counts = {}
        for arg in self.args:
            n = arg.strGenericName()
            if n not in arg_type_counts:
                arg_type_counts[n] = 1
            else:
                arg_type_counts[n] += 1

        arg_type_multiplicity = dict(arg_type_counts)

        # name the arguments
        revargs = list(args)
        revargs.reverse()
        for arg in revargs:
            n = arg.strGenericName()
            if arg_type_multiplicity[n] > 1:
                arg.setName(n + str(arg_type_counts[n]))
                arg_type_counts[n] -= 1
            else:
                arg.setName(n) # only one of these here


    def printHeader(self, trail=';'):
        arglist = []
        for arg in self.args:
            arglist.append(arg.strType() + ' ' + arg.strName())
        print 'void'
        print Insn.emit_prefix + self.name + '(' + ', '.join(["buffer_t *buf"] + arglist) + ')' + trail


    def printGenerator(self):
        self.printHeader(trail='')
        print '{'
        p = mkp(1)
        p('unsigned char *data = buffer_alloc(buf, %d);' % len(self.machine_code))

        # Basic machine code generation: copy from machine code string and or in any suitable arg bits
        offset = 0
        for byte in self.machine_code:
            builders = []
            build_this_byte = True
            for arg in self.args:
                if arg.inExclusiveRegion(offset):
                    build_this_byte = False

                builder = arg.getBuilderFor(offset)
                if builder is not None:
                    builders.append('(' + builder + ')')

            if build_this_byte:
                if len(builders) > 0:
                    builders = [''] + builders # add extra ' | ' to beginning
                p('data[%d] = 0x%02x%s;' % (offset, self.machine_code[offset], ' | '.join(builders)))
            offset += 1

        for arg in self.args:
            if arg.getExclusiveRegion() is not None:
                arg.printCopyToExclusiveRegion(p, 'data')

        print '}'

    def printTryDisassemble(self, data_name, max_len_name):
        checks = []

        offset = 0
        for byte in self.machine_code:
            bitmask = 0xff
            for arg in self.args:
                bitmask = bitmask & arg.maskOut(offset)

            if bitmask != 0:
                if bitmask == 0xff:
                    checks.append('data[%d] == 0x%02x' % (offset, self.machine_code[offset]))
                else:
                    checks.append('(data[%d] & 0x%02x) == 0x%02x' % (offset, bitmask, self.machine_code[offset]))
            offset += 1

        assert len(checks) > 0

        p = mkp(1)
        p(('if (%s >= %d && ' % (max_len_name, len(self.machine_code))) + ' && '.join(checks) + ') {')
        pp = mkp(2)
        
        formats = []
        format_args = []
        for arg in self.args:
            (format_addition, format_args_addition) = arg.printDisassemble('data', pp)
            formats = formats + format_addition
            format_args = format_args + format_args_addition
        if len(formats) == 0:
            pp('fprintf(file, "%s");' % self.name)
        else:
            pp(('fprintf(file, "%s\\t' % self.name) + ', '.join(formats) + '", ' + ', '.join(format_args) + ');');
        pp('return %d;' % len(self.machine_code));
        p('}')
    


def ImmInt(offset):
    return Imm('long', '%ld', offset, 8)

def ImmReal(offset):
    return Imm('double', '%f', offset, 8)


def Name(mips, intel=None):
    '''
    Adjust this if you prefer the Intel asm names
    '''
    return mips


def ArithmeticDestReg(offset):
    return Reg([BitPattern(0, 0, 1), BitPattern(offset, 0, 3)])
def ArithmeticSrcReg(offset):
    return Reg([BitPattern(0, 2, 1), BitPattern(offset, 3, 3)])

def printDisassemblerDoc():
    print '/**'
    print ' * Disassembles a single assembly instruction and prints it to stdout'
    print ' *'
    print ' * @param data: pointer to the instruction to disassemble'
    print ' * @param max_len: max. number of viable bytes in the instruction'
    print ' * @return Number of bytes in the disassembled instruction, or 0 on error'
    print ' */'

def printDisassemblerHeader(trail=';'):
    print 'int'
    print 'disassemble_one(FILE *file, unsigned char *data, int max_len)' + trail

def printDisassembler(instructions):
    printDisassemblerHeader(trail='')
    print '{'
    for insn in instructions:
        insn.printTryDisassemble('data', 'max_len')
    p = mkp(1)
    p('return 0; // failure')
    print '}'


instructions = [
    Insn("add", [0x48, 0x01, 0xc0], [ArithmeticDestReg(2), ArithmeticSrcReg(2)]),
    Insn("sub", [0x48, 0x29, 0xc0], [ArithmeticDestReg(2), ArithmeticSrcReg(2)]),
    Insn(Name(mips="mul", intel="imul"), [0x48, 0x0f, 0xaf], [ArithmeticDestReg(3), ArithmeticSrcReg(3)]),
    Insn(Name(mips="div_a2v0", intel="idiv"), [0x48, 0xf7, 0xf8], [ArithmeticSrcReg(2)]),
    Insn(Name(mips="li", intel="mov_imm"), [0x48, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0], [ArithmeticDestReg(1), ImmInt(2)]),
    Insn(Name(mips="jreturn", intel="ret"), [0xc3], []),
]


def printUsage():
    print 'usage: '
    print '\t' + sys.argv[0] + ' headers'
    print '\t' + sys.argv[0] + ' code'
    

if len(sys.argv) > 1:
    if sys.argv[1] == 'headers':
        for insn in instructions:
            insn.printHeader()
        printDisassemblerDoc()
        printDisassemblerHeader()

    elif sys.argv[1] == 'code':
        for insn in instructions:
            insn.printGenerator()
            print "\n"
        printDisassembler(instructions)

    else:
        printUsage()

else:
    printUsage()
