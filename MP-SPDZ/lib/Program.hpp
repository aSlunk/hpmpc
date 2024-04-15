#ifndef PROGRAM_H
#define PROGRAM_H

#include <cassert>     // assert()
#include <cstdint>     // int_t
#include <iomanip>     // set precision
#include <iostream>    // default precision
#include <istream>     // for bytecode
#include <random>      // random bit
#include <stack>       // stack for thread local ints (obsolete)
#include <string>      // bytecode path
#include <string_view> // cisc instruction
#include <utility>     // move
#include <vector>      // register

#include "Constants.hpp"
#include "Shares/CIntSet.hpp"
#include "Shares/CInteger.hpp"
#include "Shares/Integer.hpp"
#include "help/Input.hpp"
#include "help/Util.hpp"

#include "../../programs/functions/comp_trunc.hpp"

using std::string;
using std::vector;

namespace IR {

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N>
class Machine;

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N = 64>
class Program {
  public:
#if BITLENGTH == 64
    using BitType = cint;
#else
    using BitType = int_t;
#endif
    using IntType = int_t;
    using ClearIntType = cint;

    static constexpr size_t BIT_LEN = N;

    class Instruction {
      public:
        explicit Instruction(const uint32_t& op, const int& vec);

        void execute(Program& p, Machine<int_t, cint, Share, sint, sbit, BitShare, N>& m,
                     size_t& pc) const;

        const Opcode& get_opcode() const { return op; }

        const size_t& set_immediate(const int64_t& im) {
            n = im;
            return n;
        }
        const int& add_reg(const int& reg) { return regs.emplace_back(reg); }

        bool is_gf2n() const { return (static_cast<unsigned>(op) & 0x100) != 0; }
        Type get_reg_type(const Opcode& op) const;

        inline unsigned get_size() const { return size; }

        string cisc; // for cisc command (LTZ, EQZ, ...)
      private:
        Opcode op;        // opcode
        unsigned size;    // vectorized
        size_t n;         // immediate
        vector<int> regs; // required addresses in order given
    };

  public:
    explicit Program(const string&& path, size_t thread);

    Program(const Program& other) = delete;
    Program(Program&& other) = default;
    Program& operator=(const Program& other) = delete;
    Program& operator=(Program&& other) = default;

    bool
    load_program(Machine<int_t, cint, Share, sint, sbit, BitShare, N>& m); // parse bytecode file
    void setup();                                                          // parse bytecode file
    void run(Machine<int_t, cint, Share, sint, sbit, BitShare, N>& m, const int& arg,
             const int& t_num); // execute all instructions

    inline int get_argument() const { return arg; }
    void set_argument(const int& a) { arg = a; }

    // sint operations
    void popen(const vector<int>& regs, const size_t& size);
    void muls(const vector<int>& regs);
    void mulm(const vector<int>& regs, const size_t& vec);
    void xor_arith(const vector<sint>& x, const vector<sint>& y, vector<sint>& res);
    void dotprods(const vector<int>& regs, const size_t& size); // TODO
    void inputmixed(const vector<int>& regs, const bool from_reg, const size_t& vec);

    void matmulsm(const vector<int>& regs, Machine<int_t, cint, Share, sint, sbit, BitShare, N>& m);
    void matmuls(const vector<int>& regs);
    template <class iterator>
    void matmulsm_prepare(const vector<int>& regs, const int& row_1, const int& j, iterator source1,
                          iterator source2);

    void cisc(const vector<int>& regs, const std::string_view cisc);

    // sbit operations
    void inputbvec(const vector<int>& regs);
    void andrsvec(const vector<int>& regs);

  private:
    int precision;
    const string path;

    size_t thread_id;

    int arg;                  // thread arg
    int thread_num;           // thread num
    vector<Instruction> prog; // all instructions

    unsigned max_reg[REG_TYPES]; // to get required register size for all types

    vector<sint> s_register;                    // secret share
    vector<ClearIntType> c_register;            // clear share
    vector<IntType> i_register;                 // integer
    vector<sbitset_t<N, BitShare>> sb_register; // secret bit
    vector<BitType> cb_register;                // clear bit

    std::stack<IntType> i_stack;

    void update_max_reg(const Type& reg, const unsigned& sreg, const Opcode& op);
};

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N>
bool Program<int_t, cint, Share, sint, sbit, BitShare, N>::load_program(
    Machine<int_t, cint, Share, sint, sbit, BitShare, N>& m) {
    std::ifstream fd(path, std::ios::in);
    if (fd.fail()) {
        log(Level::WARNING, "couldn't open file: ", path);
        return false;
    }

    while (true) {
        uint64_t num = read_long(fd);
        if (fd.fail())
            break;
        int cur = 0x3ff & num;
        size_t vec = num >> 10; // 1st 22bits for vectorized command

        auto& inst = prog.emplace_back(cur, vec == 0 ? 1 : vec);

        switch (inst.get_opcode()) {
        // sreg + im(32)
        case Opcode::LDSI:
        case Opcode::JMPNZ:
        case Opcode::JMPEQZ:
        case Opcode::LDI:
        case Opcode::LDINT:
        case Opcode::RANDOMS:
        case Opcode::COND_PRINT_STRB:
        case Opcode::PRINTREG:
        case Opcode::PRINTREGB:
        case Opcode::PRINT4COND: {
            unsigned sreg = inst.add_reg(read_int(fd));
            inst.set_immediate(read_int(fd));

            update_max_reg(inst.get_reg_type(inst.get_opcode()), sreg + inst.get_size(),
                           inst.get_opcode());
            break;
        }
        // sreg + mem_addr(64)
        case Opcode::LDMS:
        case Opcode::LDMC:
        case Opcode::STMC:
        case Opcode::LDMCB:
        case Opcode::STMCB:
        case Opcode::STMS:
        case Opcode::STMINT:
        case Opcode::LDMSB:
        case Opcode::STMSB:
        case Opcode::GLDMC:
        case Opcode::GLDMS:
        case Opcode::LDMINT: {
            unsigned sreg = inst.add_reg(read_int(fd));          // dest
            size_t mem_addr = inst.set_immediate(read_long(fd)); // source

            update_max_reg(inst.get_reg_type(inst.get_opcode()), sreg + inst.get_size(),
                           inst.get_opcode());
            m.update_max_mem(inst.get_reg_type(inst.get_opcode()), mem_addr + inst.get_size());
            break;
        }
        // sreg + im(32) + im(32)
        case Opcode::LDBITS: {
            unsigned sreg = inst.add_reg(read_int(fd));
            unsigned bits = inst.add_reg(read_int(fd));

            inst.set_immediate(read_int(fd));

            update_max_reg(inst.get_reg_type(inst.get_opcode()), sreg + div_ceil(bits, BIT_LEN),
                           inst.get_opcode());
            break;
        }
        case Opcode::XORS:
        case Opcode::ANDS:
        case Opcode::MULS: {
            unsigned args = read_int(fd);

            assert(args % 2 == 0);

            for (size_t i = 1; i < args; i += 4) {
                int size = inst.add_reg(read_int(fd)); // vector size

                unsigned sreg = inst.add_reg(read_int(fd)); // destination

                size = inst.get_opcode() == Opcode::MULS ? size : div_ceil(size, BIT_LEN);
                update_max_reg(inst.get_reg_type(inst.get_opcode()), sreg + size,
                               inst.get_opcode());

                sreg = inst.add_reg(read_int(fd)); // factor 1
                update_max_reg(inst.get_reg_type(inst.get_opcode()), sreg + size,
                               inst.get_opcode());

                sreg = inst.add_reg(read_int(fd)); // factor 2
                update_max_reg(inst.get_reg_type(inst.get_opcode()), sreg + size,
                               inst.get_opcode());
            }
            break;
        }
        // im(32) + sreg + sreg
        case Opcode::NOTS: {
            size_t bits = inst.set_immediate(read_int(fd));

            unsigned sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::SBIT, sreg + div_ceil(bits, BIT_LEN), inst.get_opcode());

            sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::SBIT, sreg + div_ceil(bits, BIT_LEN), inst.get_opcode());
            break;
        }
        case Opcode::CONVCBIT2S: {
            size_t bits = inst.set_immediate(read_int(fd));

            unsigned sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::SBIT, sreg + div_ceil(bits, BIT_LEN), inst.get_opcode());

            sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::CBIT, sreg + div_ceil(bits, BIT_LEN), inst.get_opcode());
            break;
        }
        case Opcode::OPEN: {
            uint32_t num = inst.set_immediate(read_int(fd));
            read_int(fd); // check after opening (idk)

            for (size_t i = 1; i < num; i += 2) {
                unsigned creg = inst.add_reg(read_int(fd));
                unsigned sreg = inst.add_reg(read_int(fd));

                update_max_reg(Type::CINT, creg + inst.get_size(), inst.get_opcode());
                update_max_reg(Type::SINT, sreg + inst.get_size(), inst.get_opcode());
            }
            break;
        }
        case Opcode::REVEAL: {
            uint32_t num = read_int(fd);

            for (size_t i = 0; i < num; i += 3) {
                unsigned num = inst.add_reg(read_int(fd));

                unsigned creg = inst.add_reg(read_int(fd)); // des
                unsigned sreg = inst.add_reg(read_int(fd)); // source

                update_max_reg(Type::CBIT, creg + div_ceil(num, BIT_LEN), inst.get_opcode());
                update_max_reg(Type::SBIT, sreg + div_ceil(num, sizeof(sint) * 8),
                               inst.get_opcode());
            }
            break;
        }
        // im(32)
        case Opcode::PRINT4:
        case Opcode::JMP:
        case Opcode::ACTIVE:
        case Opcode::START:
        case Opcode::STOP:
        case Opcode::PRINT_CHR:
        case Opcode::PRINT_FLOAT_PREC:
        case Opcode::JOIN_TAPE:
            inst.set_immediate(read_int(fd));
            break;
        case Opcode::REQBL: // requirement for modulus prime calculus
                            // min bit length
        {
            int ring = read_int(fd);
            if (ring > 0) {
                log(Level::ERROR, "compiled for fields not rings");
            } else if (-ring != BITLENGTH) {
                log(Level::ERROR, ring, ": compiled for rings 2^", BITLENGTH);
                exit(EXIT_FAILURE);
            }
            break;
        }
        case Opcode::PRINT_FLOAT_PLAIN: {
            assert(inst.get_size() == 1);
            int reg = inst.add_reg(read_int(fd)); // significant
            update_max_reg(Type::CINT, reg + inst.get_size(), inst.get_opcode());

            reg = inst.add_reg(read_int(fd)); // exponent
            update_max_reg(Type::CINT, reg + inst.get_size(), inst.get_opcode());

            reg = inst.add_reg(read_int(fd)); // zero bit (zero if == 1)
            update_max_reg(Type::CINT, reg + inst.get_size(), inst.get_opcode());

            reg = inst.add_reg(read_int(fd)); // sign bit (neg if == 1)
            update_max_reg(Type::CINT, reg + inst.get_size(), inst.get_opcode());

            reg = inst.add_reg(read_int(fd)); // NaN (reg num if zero)
            update_max_reg(Type::CINT, reg + inst.get_size(), inst.get_opcode());
            break;
        }
        case Opcode::FLOATOUTPUT:
            inst.set_immediate(read_int(fd));
            inst.add_reg(read_int(fd)); // significant
            inst.add_reg(read_int(fd)); // exponent
            inst.add_reg(read_int(fd)); // zero bit
            inst.add_reg(read_int(fd)); // sign bit
            break;
        // reg
        case Opcode::PRINT_REG_PLAIN:
        case Opcode::PRINT_INT:
        case Opcode::BIT:
        case Opcode::JMPI:
        case Opcode::CRASH:
        case Opcode::NPLAYERS:
        case Opcode::PUBINPUT:
        case Opcode::THRESHOLD:
        case Opcode::PLAYERID:
        case Opcode::PUSHINT:
        case Opcode::POPINT:
        case Opcode::LDTN:
        case Opcode::STARG:
        case Opcode::LDARG: {
            unsigned reg = inst.add_reg(read_int(fd));
            update_max_reg(inst.get_reg_type(inst.get_opcode()), reg + inst.get_size(),
                           inst.get_opcode());
            break;
        }
        // im(32) + reg
        case Opcode::INTOUTPUT: {
            unsigned im = inst.set_immediate(read_int(fd));
            unsigned reg = inst.add_reg(read_int(fd));

            update_max_reg(inst.get_reg_type(inst.get_opcode()), reg + inst.get_size(),
                           inst.get_opcode());
            break;
        }
        case Opcode::PRINT_REG_SIGNED: {
            unsigned im = inst.set_immediate(read_int(fd));
            unsigned cbit = inst.add_reg(read_int(fd));

            update_max_reg(Type::CBIT, cbit + div_ceil(im, BIT_LEN), inst.get_opcode());
            break;
        }
        case Opcode::BITDECINT: {
            unsigned args = read_int(fd);
            unsigned source = inst.add_reg(read_int(fd)); // source

            update_max_reg(Type::SINT, source + inst.get_size(), inst.get_opcode());

            for (size_t i = 1; i < args; i++) {
                unsigned sreg = inst.add_reg(read_int(fd));

                update_max_reg(Type::INT, sreg + inst.get_size(), inst.get_opcode());
            }

            break;
        }
        case Opcode::CONCATS: {
            unsigned args = read_int(fd);
            unsigned dest = inst.add_reg(read_int(fd)); // dest

            update_max_reg(Type::SINT, dest + 1, inst.get_opcode());

            for (size_t i = 1; i < args; i += 2) {
                unsigned off = inst.add_reg(read_int(fd)); // offset
                unsigned sreg = inst.add_reg(read_int(fd));

                dest += off;

                update_max_reg(Type::SINT, sreg + off, inst.get_opcode());
                update_max_reg(Type::SINT, dest, inst.get_opcode());
            }

            break;
        }
        case Opcode::TRANSPOSE: {
            unsigned num = read_int(fd);
            unsigned outs = inst.set_immediate(read_int(fd));

            unsigned sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::SBIT, sreg + div_ceil(num - 1 - outs, BIT_LEN), inst.get_opcode());

            for (size_t i = 2; i < num; ++i) {
                sreg = inst.add_reg(read_int(fd));
                update_max_reg(Type::SBIT, sreg + div_ceil(outs, BIT_LEN), inst.get_opcode());
            }
            break;
        }
        case Opcode::PICKS: {
            unsigned dest = inst.add_reg(read_int(fd));
            unsigned source = inst.add_reg(read_int(fd));
            unsigned off = inst.add_reg(read_int(fd));

            int step(inst.set_immediate(read_int(fd)));

            update_max_reg(Type::SINT, dest + inst.get_size(), inst.get_opcode());
            update_max_reg(Type::SINT, source + off + step * vec + 1, inst.get_opcode());
            break;
        }
        case Opcode::USE:
        case Opcode::USE_INP: {
            inst.add_reg(read_int(fd));
            inst.add_reg(read_int(fd));
            inst.set_immediate(read_long(fd));
            break;
        }
        case Opcode::INPUTMIXEDREG:
        case Opcode::INPUTMIXED: {
            unsigned num = read_int(fd);
            for (size_t i = 0; i < num; ++i) {
                uint32_t cur = inst.add_reg(read_int(fd));
                if (cur == 2) {
                    log(Level::ERROR, "INPUTMIXED: only int/fix is supported");
                }

                uint32_t dest = inst.add_reg(read_int(fd));

                if (cur == 1) {                 // fix-point
                    inst.add_reg(read_int(fd)); // precision
                    i++;
                }

                update_max_reg(Type::SINT, dest + inst.get_size(), inst.get_opcode());
                inst.add_reg(read_int(fd)); // input PLAYER (regint for ...REG)

                i += 2;
            }
            break;
        }
        case Opcode::INPUTBVEC: {
            unsigned num = read_int(fd);

            for (size_t i = 1; i < num; i += 3) {
                unsigned bits = inst.add_reg(read_int(fd)) - 3;
                inst.add_reg(read_int(fd)); // 2^n
                inst.add_reg(read_int(fd)); // player id

                i += bits;

                for (unsigned j = 0; j < bits; ++j) {
                    unsigned sreg = inst.add_reg(read_int(fd));
                    update_max_reg(Type::SBIT, sreg + 1, inst.get_opcode());
                }
            }

            break;
        }
        // sreg + sreg + im(32)
        case Opcode::MULSI:
        case Opcode::SUBSI:
        case Opcode::SHRCI:
        case Opcode::SHLCI:
        case Opcode::ADDCI:
        case Opcode::SUBCI:
        case Opcode::MODCI:
        case Opcode::DIVCI:
        case Opcode::ADDSI:
        case Opcode::SUBSFI:
        case Opcode::SUBCFI:
        case Opcode::XORCI:
        case Opcode::ORCI:
        case Opcode::ANDCI:
        case Opcode::NOTC:
        case Opcode::ADDCBI:
        case Opcode::MULCBI:
        case Opcode::XORCBI:
        case Opcode::SHRCBI:
        case Opcode::SHLCBI:
        case Opcode::MULCI: {
            unsigned sreg = inst.add_reg(read_int(fd));
            update_max_reg(inst.get_reg_type(inst.get_opcode()), sreg + inst.get_size(),
                           inst.get_opcode());

            sreg = inst.add_reg(read_int(fd));
            update_max_reg(inst.get_reg_type(inst.get_opcode()), sreg + inst.get_size(),
                           inst.get_opcode());

            inst.set_immediate(int(read_int(fd)));
            break;
        }
        case Opcode::SUBMR: {
            unsigned sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::SINT, sreg + inst.get_size(), inst.get_opcode());

            sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::CINT, sreg + inst.get_size(), inst.get_opcode());

            sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::SINT, sreg + inst.get_size(), inst.get_opcode());
            break;
        }
        // sreg + sreg
        case Opcode::SHUFFLE:
        case Opcode::MOVC:
        case Opcode::MOVINT:
        case Opcode::EQZC:
        case Opcode::LTZC:
        case Opcode::RAND:
        case Opcode::PREFIXSUMS:
        case Opcode::MOVS: {
            unsigned sreg = inst.add_reg(read_int(fd));
            update_max_reg(inst.get_reg_type(inst.get_opcode()), sreg + inst.get_size(),
                           inst.get_opcode());

            sreg = inst.add_reg(read_int(fd));
            update_max_reg(inst.get_reg_type(inst.get_opcode()), sreg + inst.get_size(),
                           inst.get_opcode());
            break;
        }
        case Opcode::CONVINT: {
            unsigned sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::CINT, sreg + inst.get_size(), inst.get_opcode());

            sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::INT, sreg + inst.get_size(), inst.get_opcode());
            break;
        }
        case Opcode::STMSI:
        case Opcode::LDMSI: {
            unsigned sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::SINT, sreg + inst.get_size(), inst.get_opcode());

            sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::INT, sreg + inst.get_size(), inst.get_opcode());
            break;
        }
        case Opcode::STMSBI:
        case Opcode::LDMSBI: {
            unsigned sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::SBIT, sreg + inst.get_size(), inst.get_opcode());

            sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::INT, sreg + inst.get_size(), inst.get_opcode());
            break;
        }
        case Opcode::STMINTI:
        case Opcode::LDMINTI: {
            unsigned sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::INT, sreg + inst.get_size(), inst.get_opcode());

            sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::INT, sreg + inst.get_size(), inst.get_opcode());
            break;
        }
        case Opcode::STMCI:
        case Opcode::LDMCI: {
            unsigned sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::CINT, sreg + inst.get_size(), inst.get_opcode());

            sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::INT, sreg + inst.get_size(), inst.get_opcode());
            break;
        }
        // sreg + sreg + sreg
        case Opcode::XORCB:
            inst.set_immediate(read_int(fd)); // + BIT_LEN
        case Opcode::ADDCB:
        case Opcode::ADDS:
        case Opcode::SUBC:
        case Opcode::ADDC:
        case Opcode::FLOORDIVC:
        case Opcode::DIVC:
        case Opcode::MODC:
        case Opcode::MULC:
        case Opcode::ORC:
        case Opcode::ANDC:
        case Opcode::XORC:
        case Opcode::SHLC:
        case Opcode::SHRC:
        case Opcode::EQC:
        case Opcode::LTC:
        case Opcode::GTC:
        case Opcode::SUBINT:
        case Opcode::ADDINT:
        case Opcode::MULINT:
        case Opcode::DIVINT:
        case Opcode::PRINT_COND_PLAIN:
        case Opcode::SUBS: {
            unsigned sreg = inst.add_reg(read_int(fd));
            update_max_reg(inst.get_reg_type(inst.get_opcode()), sreg + inst.get_size(),
                           inst.get_opcode());

            sreg = inst.add_reg(read_int(fd));
            update_max_reg(inst.get_reg_type(inst.get_opcode()), sreg + inst.get_size(),
                           inst.get_opcode());

            sreg = inst.add_reg(read_int(fd));
            update_max_reg(inst.get_reg_type(inst.get_opcode()), sreg + inst.get_size(),
                           inst.get_opcode());
            break;
        }
        case Opcode::MULM:
        case Opcode::SUBML:
        case Opcode::ADDM: {
            unsigned sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::SINT, sreg + inst.get_size(),
                           inst.get_opcode()); // dest

            sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::SINT, sreg + inst.get_size(),
                           inst.get_opcode()); // sum1

            sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::CINT, sreg + inst.get_size(),
                           inst.get_opcode()); // sum2
            break;
        }
        case Opcode::ANDM: {
            inst.add_reg(read_int(fd)); // bits

            unsigned sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::SBIT, sreg + inst.get_size(),
                           inst.get_opcode()); // dest

            sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::SBIT, sreg + inst.get_size(),
                           inst.get_opcode()); // sum1

            sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::CBIT, sreg + inst.get_size(),
                           inst.get_opcode()); // sum2
            break;
        }
        case Opcode::CONVMODP: {
            unsigned sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::INT, sreg + inst.get_size(),
                           inst.get_opcode()); // dest

            sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::CINT, sreg + inst.get_size(),
                           inst.get_opcode()); // sum1

            inst.set_immediate(read_int(fd));
            break;
        }
        // im(32) + sreg + sreg
        case Opcode::NOTCB:
        case Opcode::MOVSB: {
            unsigned num = inst.set_immediate(read_int(fd));

            unsigned sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::SBIT, sreg + div_ceil(num, BIT_LEN), inst.get_opcode());

            sreg = inst.add_reg(read_int(fd));
            update_max_reg(Type::SBIT, sreg + div_ceil(num, BIT_LEN), inst.get_opcode());
            break;
        }
        case Opcode::BITDECS:
        case Opcode::BITCOMS: {
            unsigned num = read_int(fd);
            unsigned bit = inst.add_reg(read_int(fd));
            update_max_reg(Type::SBIT, bit + div_ceil(num, BIT_LEN), inst.get_opcode());

            for (size_t i = 1; i < num; ++i) {
                bit = inst.add_reg(read_int(fd));
                update_max_reg(Type::SBIT, bit + 1, inst.get_opcode());
            }
            break;
        }
        case Opcode::CONVCINT: {
            unsigned dest = inst.add_reg(read_int(fd));
            inst.add_reg(read_int(fd)); // source
            update_max_reg(Type::CBIT, dest + 1, inst.get_opcode());
            break;
        }
        case Opcode::CONVSINT: {
            unsigned bits = inst.add_reg(read_int(fd));
            unsigned dest = inst.add_reg(read_int(fd));
            inst.add_reg(read_int(fd)); // source
            update_max_reg(Type::SBIT, dest + div_ceil(bits, BIT_LEN), inst.get_opcode());
            break;
        }
        case Opcode::CONVCINTVEC: {
            unsigned bits = read_int(fd) - 1;
            inst.add_reg(read_int(fd)); // source

            for (size_t i = 0; i < bits; ++i) {
                unsigned dest = inst.add_reg(read_int(fd));
                update_max_reg(Type::SBIT, dest + div_ceil(inst.get_size(), BIT_LEN),
                               inst.get_opcode());
            }
            break;
        }
        case Opcode::ANDRSVEC: {
            unsigned num = read_int(fd);

            for (size_t i = 0; i < num;) {
                unsigned one_op = inst.add_reg(read_int(fd));
                unsigned vec = inst.add_reg(read_int(fd)); // vector size

                for (size_t j = 2; j < one_op; ++j) {
                    unsigned reg = inst.add_reg(read_int(fd));

                    update_max_reg(Type::SBIT, reg + div_ceil(vec, BIT_LEN), inst.get_opcode());
                }

                i += one_op;
            }
            break;
        }
        case Opcode::ANDRS: {
            unsigned args = read_int(fd);
            for (size_t i = 0; i < args; i += 4) {
                unsigned vec = inst.add_reg(read_int(fd));
                unsigned dest = inst.add_reg(read_int(fd));
                update_max_reg(Type::SBIT, dest + div_ceil(vec, BIT_LEN), inst.get_opcode());
                inst.add_reg(read_int(fd)); // source
                inst.add_reg(read_int(fd)); // const factor
            }
            break;
        }
        case Opcode::MULRS: {
            unsigned args = read_int(fd);
            for (size_t i = 0; i < args; i += 4) {
                unsigned vec = inst.add_reg(read_int(fd));
                unsigned dest = inst.add_reg(read_int(fd));
                update_max_reg(Type::SINT, dest + vec, inst.get_opcode());
                inst.add_reg(read_int(fd)); // source
                inst.add_reg(read_int(fd)); // const factor
            }
            break;
        }
        case Opcode::MATMULSM: {
            unsigned dest = inst.add_reg(read_int(fd));

            inst.add_reg(read_int(fd)); // factor 1
            inst.add_reg(read_int(fd)); // factor 2

            int rows = inst.add_reg(read_int(fd));
            inst.add_reg(read_int(fd)); // cols/rows of 1st/2nd factor
            int cols = inst.add_reg(read_int(fd));

            update_max_reg(Type::SINT, dest + rows * cols, inst.get_opcode());

            for (size_t i = 0; i < 6u; ++i) {
                inst.add_reg(read_int(fd));
            }

            break;
        }
        case Opcode::MATMULS: {
            unsigned args = read_int(fd);
            assert(args % 6 == 0);
            for (size_t i = 0; i < args; ++i) {
                unsigned dest = inst.add_reg(read_int(fd)); // (sint)

                inst.add_reg(read_int(fd)); // factor 1 (sint)
                inst.add_reg(read_int(fd)); // factor 2 (sint)

                int rows = inst.add_reg(read_int(fd));
                inst.add_reg(read_int(fd)); // cols/rows of 1st/2nd factor
                int cols = inst.add_reg(read_int(fd));

                update_max_reg(Type::SINT, dest + rows * cols, inst.get_opcode());
            }

            break;
        }
        case Opcode::USE_MATMUL: {
            inst.add_reg(read_int(fd));
            inst.add_reg(read_int(fd));
            inst.add_reg(read_int(fd));
            inst.set_immediate(read_long(fd));
            break;
        }
        case Opcode::INCINT: {
            unsigned dest = inst.add_reg(read_int(fd));

            inst.add_reg(read_int(fd)); // base
            inst.add_reg(read_int(fd)); // increment
            inst.add_reg(read_int(fd)); // repeat
            inst.add_reg(read_int(fd)); // wrap

            update_max_reg(Type::INT, dest + inst.get_size(), inst.get_opcode());
            break;
        }
        case Opcode::TRUNC_PR: {
            unsigned args = read_int(fd);

            for (size_t i = 0; i < args; i += 4) {
                unsigned dest = inst.add_reg(read_int(fd));
                unsigned source = inst.add_reg(read_int(fd));

                inst.add_reg(read_int(fd)); // bits to use
                inst.add_reg(read_int(fd)); // bits to truncate

                update_max_reg(Type::SINT, dest + inst.get_size(), inst.get_opcode());
            }
            break;
        }
        case Opcode::DOTPRODS: {
            unsigned args = read_int(fd);
            for (size_t i = 0; i < args; ++i) {
                unsigned len = inst.add_reg(read_int(fd)) - 2;

                unsigned dest = inst.add_reg(read_int(fd));
                update_max_reg(Type::SINT, dest + inst.get_size(), inst.get_opcode());

                for (size_t j = 0; j < len; ++j)
                    inst.add_reg(read_int(fd));

                i += len + 1;
            }
            break;
        }
        case Opcode::SPLIT: {
            unsigned args = read_int(fd);

            inst.add_reg(read_int(fd)); // num player
            inst.add_reg(read_int(fd)); // source

            for (size_t i = 2; i < args; ++i) {
                unsigned dest = inst.add_reg(read_int(fd));
                update_max_reg(Type::SBIT, dest + div_ceil(vec, BIT_LEN), inst.get_opcode());
            }
            break;
        }
        case Opcode::DABIT: {
            unsigned dest = inst.add_reg(read_int(fd));
            update_max_reg(Type::SINT, dest + inst.get_size(), inst.get_opcode());
            dest = inst.add_reg(read_int(fd));
            update_max_reg(Type::SBIT, dest + div_ceil(vec, BIT_LEN), inst.get_opcode());
            break;
        }
        case Opcode::CONVCBITVEC: {
            unsigned bits = inst.set_immediate(read_int(fd));

            unsigned dest = inst.add_reg(read_int(fd));
            update_max_reg(Type::SINT, dest + bits, inst.get_opcode());
            inst.add_reg(read_int(fd)); // source
            break;
        }
        case Opcode::CISC: {
            unsigned args = read_int(fd);
            char op[16];
            fd.read(op, 16);
            inst.cisc = string(op, 16);

            if (strncmp(op, "LTZ", 3) == 0 or strncmp(op, "EQZ", 3) == 0) {
                for (size_t i = 0; i < args - 1; i += 6) {
                    unsigned size = inst.add_reg(read_int(fd)); // arguments
                    unsigned vec = inst.add_reg(read_int(fd));
                    assert(size == 6);
                    unsigned dest = inst.add_reg(read_int(fd));
                    inst.add_reg(read_int(fd)); // result
                    inst.add_reg(read_int(fd)); // bit_length
                    inst.add_reg(read_int(fd)); // ignore
                    update_max_reg(Type::SINT, dest + vec, inst.get_opcode());
                }
            } else {
                for (size_t i = 0; i < args - 1; ++i) {
                    unsigned cur = inst.add_reg(read_int(fd));
                }
            }
            break;
        }
        case Opcode::TIME:
            break;
        case Opcode::RUN_TAPE: {
            unsigned args = read_int(fd);

            for (size_t i = 0; i < args; i += 3) {
                inst.add_reg(read_int(fd));
                inst.add_reg(read_int(fd));
                inst.add_reg(read_int(fd));
            }
            break;
        }
        default:
            log(Level::WARNING, "unknown operation");
            log(Level::WARNING, "read: ", cur);
            log(Level::WARNING, "vec: ", vec);
            fd.close();
            return false;
        }
    }
    fd.close();
    return true;
}

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N>
Type Program<int_t, cint, Share, sint, sbit, BitShare, N>::Instruction::get_reg_type(
    const Opcode& op) const {
    switch (op) {
    case Opcode::LDBITS:
    case Opcode::BITCOMS:
    case Opcode::BITDECS:
    case Opcode::XORS:
    case Opcode::ANDS:
    case Opcode::NOTS:
    case Opcode::LDMSB:
    case Opcode::STMSB:
    case Opcode::MOVSB:
    case Opcode::TRANSPOSE:
    case Opcode::INPUTBVEC:
        return Type::SBIT;
    case Opcode::PRINT_REG_SIGNED:
    case Opcode::LDMCB:
    case Opcode::STMCB:
    case Opcode::NOTCB:
    case Opcode::XORCB:
    case Opcode::ADDCB:
    case Opcode::ADDCBI:
    case Opcode::MULCBI:
    case Opcode::SHRCBI:
    case Opcode::SHLCBI:
    case Opcode::XORCBI:
    case Opcode::COND_PRINT_STRB:
    case Opcode::PRINTREGB:
        return Type::CBIT;
    case Opcode::LDMINT:
    case Opcode::LDARG:
    case Opcode::STARG:
    case Opcode::JMPI:
    case Opcode::CRASH:
    case Opcode::BITDECINT:
    case Opcode::LDINT:
    case Opcode::STMINT:
    case Opcode::LTC:
    case Opcode::GTC:
    case Opcode::SUBINT:
    case Opcode::ADDINT:
    case Opcode::MULINT:
    case Opcode::DIVINT:
    case Opcode::JMPEQZ:
    case Opcode::JMPNZ:
    case Opcode::INCINT:
    case Opcode::EQC:
    case Opcode::EQZC:
    case Opcode::LTZC:
    case Opcode::RAND:
    case Opcode::MOVINT:
    case Opcode::PRINT_INT:
    case Opcode::SHUFFLE:
    case Opcode::NPLAYERS:
    case Opcode::THRESHOLD:
    case Opcode::PLAYERID:
    case Opcode::INTOUTPUT:
    case Opcode::PUSHINT:
    case Opcode::POPINT:
    case Opcode::LDTN:
        return Type::INT;
    case Opcode::LDI:
    case Opcode::LDMC:
    case Opcode::STMC:
    case Opcode::SHRCI:
    case Opcode::SHLCI:
    case Opcode::MULCI:
    case Opcode::ADDCI:
    case Opcode::SUBCI:
    case Opcode::DIVCI:
    case Opcode::MODCI:
    case Opcode::PRINT_REG_PLAIN:
    case Opcode::PRINT_COND_PLAIN:
    case Opcode::PRINT4COND:
    case Opcode::PRINT_FLOAT_PLAIN:
    case Opcode::ADDC:
    case Opcode::SUBC:
    case Opcode::MULC:
    case Opcode::DIVC:
    case Opcode::MODC:
    case Opcode::FLOORDIVC:
    case Opcode::ORC:
    case Opcode::ANDC:
    case Opcode::XORC:
    case Opcode::NOTC:
    case Opcode::SHLC:
    case Opcode::SHRC:
    case Opcode::SUBCFI:
    case Opcode::MOVC:
    case Opcode::XORCI:
    case Opcode::ORCI:
    case Opcode::ANDCI:
    case Opcode::PRINTREG:
    case Opcode::PUBINPUT:
        return Type::CINT;
    case Opcode::LDMS:
    case Opcode::INPUTMIXED:
    case Opcode::INPUTMIXEDREG:
    case Opcode::LDSI:
    case Opcode::STMS:
    case Opcode::MULS:
    case Opcode::MULSI:
    case Opcode::SUBSI:
    case Opcode::ADDS:
    case Opcode::SUBS:
    case Opcode::CONCATS:
    case Opcode::MOVS:
    case Opcode::BIT:
    case Opcode::SUBSFI:
    case Opcode::DOTPRODS:
    case Opcode::ADDSI:
    case Opcode::RANDOMS:
    case Opcode::PREFIXSUMS:
        return Type::SINT;
    default:
        break;
    }

    if (is_gf2n()) {
        unsigned code = static_cast<unsigned>(op);
        code -= 0x100;
        if (get_reg_type(static_cast<Opcode>(code)) == Type::CINT)
            return Type::CGF2N;
        else
            return Type::SGF2N;
    }

    return Type::SINT;
}

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N>
Program<int_t, cint, Share, sint, sbit, BitShare, N>::Instruction::Instruction(const uint32_t& opc,
                                                                               const int& vec)
    : size(vec) {
    // only if opc is known
    if (opc == 0x01 || opc == 0x02 || opc == 0x03 || opc == 0x12 || opc == 0x1b || opc == 0x90 ||
        opc == 0x96 || opc == 0x21f || opc == 0xb || opc == 0x91 || opc == 0x92 || opc == 0x98 ||
        opc == 0x2d || opc == 0x11 || opc == 0x13 || opc == 0x2e || opc == 0x249 || opc == 0x04 ||
        opc == 0x05 || opc == 0x06 || opc == 0xd2 || opc == 0xcb || opc == 0xcc || opc == 0xcd ||
        opc == 0xce || opc == 0xcf || opc == 0x08 || opc == 0xa || opc == 0x51 || opc == 0xe2 ||
        opc == 0xe6 || opc == 0xe7 || opc == 0x58 || opc == 0xc || opc == 0xc0 || opc == 0xb2 ||
        opc == 0xc1 || opc == 0x99 || opc == 0xe3 || opc == 0x17 || opc == 0x18 || opc == 0x21 ||
        opc == 0x24 || opc == 0x26 || opc == 0x103 || opc == 0xe4 || opc == 0x104 || opc == 0xa5 ||
        opc == 0xa6 || opc == 0xa7 || opc == 0x23 || opc == 0x29 || opc == 0x31 || opc == 0x14 ||
        opc == 0x22 || opc == 0x224 || opc == 0x32 || opc == 0x20 || opc == 0x33 || opc == 0x231 ||
        opc == 0x15 || opc == 0x2a || opc == 0x2c || opc == 0x27 || opc == 0x28 || opc == 0x25 ||
        opc == 0x82 || opc == 0x16 || opc == 0x35 || opc == 0x83 || opc == 0xb3 || opc == 0xb4 ||
        opc == 0xb5 || opc == 0xbf || opc == 0xb6 || opc == 0x30 || opc == 0x34 || opc == 0xe1 ||
        opc == 0xca || opc == 0x9a || opc == 0xf2 || opc == 0xf3 || opc == 0x2f || opc == 0x20a ||
        opc == 0x205 || opc == 0x37 || opc == 0x97 || opc == 0x217 || opc == 0x218 || opc == 0xb1 ||
        opc == 0x203 || opc == 0x202 || opc == 0x204 || opc == 0x3b || opc == 0x00 || opc == 0x73 ||
        opc == 0x74 || opc == 0x75 || opc == 0x76 || opc == 0x20e || opc == 0x244 || opc == 0x72 ||
        opc == 0x9f || opc == 0x80 || opc == 0x36 || opc == 0x2b || opc == 0x214 || opc == 0x24a ||
        opc == 0x5b || opc == 0x248 || opc == 0x1f || opc == 0x71 || opc == 0xe0 || opc == 0x81 ||
        opc == 0x21e || opc == 0x240 || opc == 0x241 || opc == 0x200 || opc == 0x212 ||
        opc == 0x242 || opc == 0x09 || opc == 0x219 || opc == 0x21d || opc == 0x21a ||
        opc == 0x221 || opc == 0xa9 || opc == 0x70 || opc == 0x19 || opc == 0x243 || opc == 0x247 ||
        opc == 0xaa || opc == 0xab || opc == 0x20c || opc == 0xbc || opc == 0x21b || opc == 0x210 ||
        opc == 0x20b || opc == 0xa8 || opc == 0x94 || opc == 0x1a || opc == 0x20f || opc == 0x213 ||
        opc == 0x220 || opc == 0xd1 || opc == 0x21c || opc == 0x07 || opc == 0x10 || opc == 0xe9 ||
        opc == 0x95 || opc == 0x9c || opc == 0x9d || opc == 0x9e || opc == 0x93 || opc == 0x9b) {
        op = static_cast<Opcode>(opc);
    } else {
        op = Opcode::NONE;
        n = opc;
    }
}

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N>
Program<int_t, cint, Share, sint, sbit, BitShare, N>::Program(const string&& path, size_t thread)
    : precision(FRACTIONAL), path(std::move(path)), thread_id(thread), max_reg() {}

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N>
void Program<int_t, cint, Share, sint, sbit, BitShare, N>::update_max_reg(const Type& reg,
                                                                          const unsigned& sreg,
                                                                          const Opcode& op
                                                                          [[maybe_unused]]) {
    if (max_reg[static_cast<unsigned>(reg)] < sreg)
        max_reg[static_cast<unsigned>(reg)] = sreg;
}

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N>
void Program<int_t, cint, Share, sint, sbit, BitShare, N>::setup() {
    // prepare register
    s_register.resize(max_reg[static_cast<unsigned>(Type::SINT)]);
    c_register.resize(max_reg[static_cast<unsigned>(Type::CINT)]);
    i_register.resize(max_reg[static_cast<unsigned>(Type::INT)]);
    cb_register.resize(max_reg[static_cast<unsigned>(Type::CBIT)]);
    sb_register.resize(max_reg[static_cast<unsigned>(Type::SBIT)]);
}

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N>
void Program<int_t, cint, Share, sint, sbit, BitShare, N>::run(
    Machine<int_t, cint, Share, sint, sbit, BitShare, N>& m, const int& argi,
    const int& thread_number) {
    arg = argi;
    thread_num = thread_number;

    for (size_t pc = 0; pc < prog.size(); ++pc)
        prog[pc].execute(*this, m, pc);
}

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N>
void Program<int_t, cint, Share, sint, sbit, BitShare, N>::Instruction::execute(
    Program<int_t, cint, Share, sint, sbit, BitShare, N>& p,
    Machine<int_t, cint, Share, sint, sbit, BitShare, N>& m, size_t& pc) const {
    for (size_t vec = 0; vec < size; ++vec) {
        switch (op) {
        case Opcode::PREFIXSUMS: {
            sint s(0);
            for (size_t vec = 0; vec < size; ++vec) {
                s += p.s_register[regs[1] + vec];
                p.s_register[regs[0] + vec] = s;
            }
            return;
        }
        case Opcode::LDARG:
            p.i_register[regs[0]] = p.get_argument();
            break;
        case Opcode::STARG:
            p.set_argument(p.i_register[regs[0]].get());
            break;
        case Opcode::LDMC:
            p.c_register[regs[0] + vec] = m.c_mem[n + vec];
            break;
        case Opcode::STMC:
            m.c_mem[n + vec] = p.c_register[regs[0] + vec];
            break;
        case Opcode::LDMCB:
            p.cb_register[regs[0] + vec] = m.cb_mem[long(n) + vec];
            break;
        case Opcode::STMCB:
            m.cb_mem[long(n) + vec] = p.cb_register[regs[0] + vec];
            break;
        case Opcode::LDMS:
            p.s_register[regs[0] + vec] = m.s_mem[n + vec];
            break;
        case Opcode::LDMSI:
            p.s_register[regs[0] + vec] = m.s_mem[p.i_register[regs[1] + vec].get()];
            break;
        case Opcode::STMSI:
            if (p.i_register[regs[1] + vec].get() + 1 > m.s_mem.size())
                m.s_mem.resize(p.i_register[regs[1] + vec].get() + 1);
            m.s_mem[p.i_register[regs[1] + vec].get()] = p.s_register[regs[0] + vec];
            break;
        case Opcode::LDMCI:
            p.c_register[regs[0] + vec] = m.c_mem[p.i_register[regs[1] + vec].get()];
            break;
        case Opcode::STMCI:
            if (p.i_register[regs[1] + vec].get() + 1 > m.c_mem.size())
                m.c_mem.resize(p.i_register[regs[1] + vec].get() + 1);
            m.c_mem[p.i_register[regs[1] + vec].get()] = p.c_register[regs[0] + vec];
            break;
        case Opcode::STMSBI:
            if (p.i_register[regs[1]].get() + get_size() > m.sb_mem.size())
                m.sb_mem.resize(p.i_register[regs[1]].get() + get_size());

            for (size_t i = 0; i < get_size(); ++i)
                m.sb_mem[p.i_register[regs[1]].get() + i] = p.sb_register[regs[0] + i];
            return;
        case Opcode::LDMSBI:
            p.sb_register[regs[0] + vec] = m.sb_mem[p.i_register[regs[1]].get() + vec];
            break;
        case Opcode::STMINTI:
            if (p.i_register[regs[1] + vec].get() + 1 > m.ci_mem.size())
                m.ci_mem.resize(p.i_register[regs[1] + vec].get() + 1);
            m.ci_mem[p.i_register[regs[1] + vec].get()] = p.i_register[regs[0] + vec];
            break;
        case Opcode::LDMINTI:
            p.i_register[regs[0] + vec] = m.ci_mem[p.i_register[regs[1] + vec].get()];
            break;
        case Opcode::LDSI:
            p.s_register[regs[0] + vec] = int(n);
            break;
        case Opcode::LDI:
            p.c_register[regs[0] + vec] = PROMOTE(int(n));
            break;
        case Opcode::LDINT:
            p.i_register[regs[0] + vec] = int(n);
            break;
        case Opcode::INCINT: {
            auto dest = p.i_register.begin() + regs[0];
            auto base = p.i_register[regs[1]];
            IntType cur = base;

            const IntType inc = int64_t(regs[2]);
            int repeat = regs[3];
            int wrap = regs[4];

            for (size_t i = 0; i < size; ++i) {
                *(dest + i) = cur;

                repeat--;
                if (repeat == 0) {
                    repeat = regs[3];

                    wrap--;
                    if (wrap == 0) {
                        cur = base;
                        wrap = regs[4];
                    } else {
                        cur = cur + inc;
                    }
                }
            }
            return;
        }
        case Opcode::MATMULSM:
            p.matmulsm(regs, m);
            return;
        case Opcode::MATMULS:
            p.matmuls(regs);
            return;
        case Opcode::DOTPRODS:
            p.dotprods(regs, get_size());
            return;
        case Opcode::USE: // for statistics
        case Opcode::USE_INP:
        case Opcode::USE_MATMUL:
            // std::cerr << "DEBUG: USE_* " << regs[0] + vec << "," <<
            // regs[1] + vec << "," << n << "\n";
            break;
        case Opcode::ADDC:
            p.c_register[regs[0] + vec] = p.c_register[regs[1] + vec] + p.c_register[regs[2] + vec];
            break;
        case Opcode::SUBC:
            p.c_register[regs[0] + vec] = p.c_register[regs[1] + vec] - p.c_register[regs[2] + vec];
            break;
        case Opcode::MULC:
            p.c_register[regs[0] + vec] = p.c_register[regs[1] + vec] * p.c_register[regs[2] + vec];
            break;
        case Opcode::ORC:
            p.c_register[regs[0] + vec] = p.c_register[regs[1] + vec] | p.c_register[regs[2] + vec];
            break;
        case Opcode::ANDC:
            p.c_register[regs[0] + vec] = p.c_register[regs[1] + vec] & p.c_register[regs[2] + vec];
            break;
        case Opcode::XORC:
            p.c_register[regs[0] + vec] = p.c_register[regs[1] + vec] ^ p.c_register[regs[2] + vec];
            break;
        case Opcode::ORCI:
            p.c_register[regs[0] + vec] = p.c_register[regs[1] + vec] | INT_TYPE(int(n));
            break;
        case Opcode::XORCI:
            p.c_register[regs[0] + vec] = p.c_register[regs[1] + vec] ^ INT_TYPE(int(n));
            break;
        case Opcode::NOTC:
            p.c_register[regs[0] + vec] =
                (~p.c_register[regs[1] + vec]) + INT_TYPE((1lu << int(n)));
            break;
        case Opcode::ANDCI:
            p.c_register[regs[0] + vec] = p.c_register[regs[1] + vec] & INT_TYPE(int(n));
            break;
        case Opcode::DIVC:
        case Opcode::FLOORDIVC:
            p.c_register[regs[0] + vec] = p.c_register[regs[1] + vec] / p.c_register[regs[2] + vec];
            break;
        case Opcode::MODC:
            p.c_register[regs[0] + vec] = p.c_register[regs[1] + vec] % p.c_register[regs[2] + vec];
            break;
        case Opcode::SHLC:
            p.c_register[regs[0] + vec] = p.c_register[regs[1] + vec]
                                          << p.c_register[regs[2] + vec];
            break;
        case Opcode::SHRC:
            p.c_register[regs[0] + vec] =
                p.c_register[regs[1] + vec] >> p.c_register[regs[2] + vec];
            break;
        case Opcode::ADDS:
            p.s_register[regs[0] + vec] = p.s_register[regs[1] + vec] + p.s_register[regs[2] + vec];
            break;
        case Opcode::SUBS:
            p.s_register[regs[0] + vec] = p.s_register[regs[1] + vec] - p.s_register[regs[2] + vec];
            break;
        case Opcode::MULS:
            p.muls(regs);
            return;
        case Opcode::MULSI:
            p.s_register[regs[0] + vec] = p.s_register[regs[1] + vec].mult_public(int(n));
            break;
        case Opcode::SUBSI:
            p.s_register[regs[0] + vec] = p.s_register[regs[1] + vec] - sint(int(n));
            break;
        case Opcode::MULCI:
            p.c_register[regs[0] + vec] = p.c_register[regs[1] + vec] * INT_TYPE(int(n));
            break;
        case Opcode::EQC:
            p.i_register[regs[0] + vec] =
                p.i_register[regs[1] + vec] == p.i_register[regs[2] + vec];
            break;
        case Opcode::EQZC:
            p.i_register[regs[0] + vec] = p.i_register[regs[1] + vec] == 0;
            break;
        case Opcode::LTZC:
            p.i_register[regs[0] + vec] = p.i_register[regs[1] + vec] < 0;
            break;
        case Opcode::RAND: {
            long rand = m.get_random();
            rand = p.i_register[regs[1] + vec].get() >= 64
                       ? rand
                       : rand % (1 << p.i_register[regs[1] + vec].get());
            p.i_register[regs[0] + vec] = rand;
            break;
        }
        case Opcode::ADDCI:
            p.c_register[regs[0] + vec] = (p.c_register[regs[1] + vec]) + INT_TYPE(int(n));
            break;
        case Opcode::SUBCI:
            p.c_register[regs[0] + vec] = (p.c_register[regs[1] + vec]) - INT_TYPE(int(n));
            break;
        case Opcode::DIVCI:
            p.c_register[regs[0] + vec] = (p.c_register[regs[1] + vec]) / INT_TYPE(int(n));
            break;
        case Opcode::MODCI:
            p.c_register[regs[0] + vec] = (p.c_register[regs[1] + vec]) % INT_TYPE(int(n));
            break;
        case Opcode::ADDSI:
            p.s_register[regs[0] + vec] = p.s_register[regs[1] + vec] + sint(int(n));
            break;
        case Opcode::SUBSFI:
            p.s_register[regs[0] + vec] = sint(int(n)) - (p.s_register[regs[1] + vec]);
            break;
        case Opcode::SUBCFI:
            p.c_register[regs[0] + vec] =
                ClearIntType(INT_TYPE(int(n))) - (p.c_register[regs[1] + vec]);
            break;
        case Opcode::SHRCI:
            p.c_register[regs[0] + vec] = p.c_register[regs[1] + vec] >> INT_TYPE(int(n));
            break;
        case Opcode::SHLCI:
            p.c_register[regs[0] + vec] = p.c_register[regs[1] + vec] << INT_TYPE(int(n));
            break;
        case Opcode::LTC:
            p.i_register[regs[0] + vec] = p.i_register[regs[1] + vec] < p.i_register[regs[2] + vec];
            break;
        case Opcode::GTC:
            p.i_register[regs[0] + vec] = p.i_register[regs[1] + vec] > p.i_register[regs[2] + vec];
            break;
        case Opcode::SUBINT:
            p.i_register[regs[0] + vec] = p.i_register[regs[1] + vec] - p.i_register[regs[2] + vec];
            break;
        case Opcode::ADDINT:
            p.i_register[regs[0] + vec] = p.i_register[regs[1] + vec] + p.i_register[regs[2] + vec];
            break;
        case Opcode::MULINT:
            p.i_register[regs[0] + vec] = p.i_register[regs[1] + vec] * p.i_register[regs[2] + vec];
            break;
        case Opcode::DIVINT:
            p.i_register[regs[0] + vec] = p.i_register[regs[1] + vec] / p.i_register[regs[2] + vec];
            break;
        case Opcode::MULM:
            p.mulm(regs, get_size());
            return;
        case Opcode::ANDM:
            for (int i = 0; i < regs[0]; ++i) {
                p.sb_register[regs[1] + i / BIT_LEN][i % BIT_LEN] =
                    p.sb_register[regs[2] + i / BIT_LEN][i % BIT_LEN].and_public(
                        ((p.cb_register[regs[3] + i / BIT_LEN] >> int64_t(i % BIT_LEN)) &
                         int64_t(1))
                            .get_type());
            }
            return;
        case Opcode::SUBML:
            p.s_register[regs[0] + vec] =
                p.s_register[regs[1] + vec] - p.s_register[regs[1] + vec].get_share_from_public_dat(
                                                  p.c_register[regs[2] + vec].get_type());
            break;
        case Opcode::SUBMR:
            p.s_register[regs[0] + vec] = p.s_register[regs[2] + vec].get_share_from_public_dat(
                                              p.c_register[regs[1] + vec].get_type()) -
                                          p.s_register[regs[2] + vec];
            break;
        case Opcode::ADDM:
            p.s_register[regs[0] + vec] =
                p.s_register[regs[1] + vec] + p.s_register[regs[1] + vec].get_share_from_public_dat(
                                                  p.c_register[regs[2] + vec].get_type());
            break;
        case Opcode::OPEN:
            p.popen(regs, get_size());
            return;
        case Opcode::TRUNC_PR: {
            assert(regs.size() % 4 == 0);
            vector<sint> input;
            vector<int> idxs;
            for (size_t i = 0; i < regs.size(); i += 4) {
                assert(regs[i + 3] == FRACTIONAL and "FRACTIONAL not set correctly");

                for (size_t j = 0; j < get_size(); ++j)
                    input.push_back(p.s_register[regs[i + 1] + j]);
                idxs.push_back(regs[i]); // dest
            }
            sint* res = new sint[input.size()];
            trunc_pr<sint>(input.data(), res, input.size());

            for (size_t i = 0; i < idxs.size(); ++i) {
                for (size_t j = 0; j < get_size(); ++j)
                    p.s_register[idxs[i] + j] = res[i * get_size() + j];
            }

            delete[] res;
            return;
        }
        case Opcode::STMS:
            m.s_mem[n + vec] = p.s_register[regs[0] + vec];
            break;
        case Opcode::PRINT4:
            m.get_out() << string((char*)&n, 4);
            break;
        case Opcode::PRINTREG: {
            const auto& reg = p.c_register[regs[0] + vec].get_all();
#if DATTYPE == BITLENGTH
            m.get_out() << "Reg[" << regs[0] + vec << "] = " << reg[0] << string((char*)&n, 4)
                        << "\n";
#else
            m.get_out() << "Reg[" << regs[0] + vec << "] = (" << reg[0];

            for (size_t i = 1; i < reg.size(); ++i)
                m.get_out() << ", " << reg[i];
            m.get_out() << ")" << string((char*)&n, 4) << "\n";
#endif
            break;
        }
        case Opcode::PRINTREGB: {
            m.get_out() << "reg[" << regs[0] << "] = 0x";
            for (int i = get_size() - 1; i >= 0; --i) {
                const auto& reg = p.cb_register[regs[0] + i].get(); // TODO: support for SIMD
                m.get_out() << std::hex << reg << std::dec;
            }
            m.get_out() << " # " << string((char*)&n, 4) << "\n";
            return;
        }
        case Opcode::PRINT4COND:
            if (p.c_register[regs[0]].get() != 0)
                m.get_out() << string((char*)&n, 4);
            break;
        case Opcode::COND_PRINT_STRB:
            if (p.cb_register[regs[0]].get() != 0)
                m.get_out() << string((char*)&n, 4);
            break;
        case Opcode::PRINT_CHR: {
            m.get_out() << static_cast<char>(n);
            break;
        }
        case Opcode::PRINT_INT: {
#if BITLENGTH == DATTYPE
            m.get_out() << p.i_register[regs[0]].get();
#else
            const auto& vec = p.i_register[regs[0]].get_all();
            m.get_out() << "(" << vec[0];
            for (size_t i = 1; i < vec.size(); ++i)
                m.get_out() << ", " << vec[i];
            m.get_out() << ")";
#endif
            return;
        }
        case Opcode::PRINT_REG_PLAIN: {
            if (size > 1)
                m.get_out() << "[";

            const auto& reg = p.c_register[regs[0]].get_all();
#if DATTYPE == BITLENGTH
            m.get_out() << reg[0];
#else
            m.get_out() << "(" << reg[0];
            for (size_t j = 1; j < reg.size(); ++j)
                m.get_out() << ", " << reg[j];
            m.get_out() << ")";
#endif
            for (size_t i = 1; i < size; ++i) {
                const auto& reg = p.c_register[regs[0] + i].get_all();
#if DATTYPE == BITLENGTH
                m.get_out() << ", " << reg[0];
#else
                m.get_out() << ", (" << reg[0];
                for (size_t j = 1; j < reg.size(); ++j)
                    m.get_out() << ", " << reg[j];
                m.get_out() << ")";
#endif
            }
            if (size > 1)
                m.get_out() << "]";
            return;
        }
        case Opcode::PRINT_COND_PLAIN:
            if (p.c_register[regs[0]].get() != 0) {
                m.get_out() << (p.c_register[regs[1]] << p.c_register[regs[2]]).get();
            }
            break;
        case Opcode::INPUTMIXED: // fine
            p.inputmixed(regs, false, get_size());
            return;
        case Opcode::INPUTMIXEDREG:
            p.inputmixed(regs, true, get_size());
            return;
        case Opcode::PUBINPUT:
            if (!m.public_input.is_open())
                m.public_input.open_input_file(INPUT_PATH + "PUB-INPUT");

            p.c_register[regs[0] + vec] = PROMOTE(m.public_input.template next<int>(
                [](const std::string& s) -> int { return std::stoi(s.c_str(), nullptr, 10); }));
            break;
        case Opcode::INTOUTPUT:
            if (int(n) == -1 or int(n) == PARTY)
                m.get_out() << "Output: " << p.i_register[regs[0] + vec].get() << "\n";
            break;
        case Opcode::FLOATOUTPUT:
            if (int(n) == -1 or int(n) == PARTY) {
                const auto& sigs = p.c_register[regs[0] + vec].get_all();
                const auto& exp = p.c_register[regs[1] + vec].get_all();
                const auto& zero = p.c_register[regs[2] + vec].get_all();
                const auto& sign = p.c_register[regs[3] + vec].get_all();

                for (size_t i = 0; i < SIZE_VEC; ++i) {
                    double res = 0;
                    if (zero[i] != 1) {
                        res = sigs[i] * powf(2, exp[i]);
                        if (sign[i] == 1)
                            res *= -1;
                    }
                    m.get_out() << "Output: " << res << "\n";
                }
            }
            break;
        case Opcode::CONCATS: {
            auto dest = p.s_register.begin() + regs[0];

            for (size_t i = 1; i < regs.size(); i += 2) {
                auto source = p.s_register.begin() + regs[i + 1];

                for (int j = 0; j < regs[i]; ++j)
                    *dest++ = *source++;
            }
            return;
        }
        case Opcode::LDBITS: // fine
            if (regs[1] >= int(BIT_LEN))
                log(Level::ERROR, "public val too long :c");

            for (int j = 0; j < regs[1]; j++) {
                p.sb_register[regs[0]][j] = BitShare(((n >> j) & 1) == 1 ? PROMOTE(1) : ZERO);
            }

            return;
        case Opcode::STMSB:
            m.sb_mem[n + vec] = p.sb_register[regs[0] + vec];
            break;
        case Opcode::LDMSB:
            p.sb_register[regs[0] + vec] = m.sb_mem[n + vec];
            break;
        case Opcode::XORS:
            for (size_t i = 0; i < regs.size(); i += 4) {
                for (int j = 0; j < regs[i]; ++j) {
                    p.sb_register[regs[i + 1] + j / BIT_LEN][j % BIT_LEN] =
                        p.sb_register[regs[i + 2] + j / BIT_LEN][j % BIT_LEN] ^
                        p.sb_register[regs[i + 3] + j / BIT_LEN][j % BIT_LEN];
                }
            }
            return;
        case Opcode::ANDS:
            for (size_t i = 0; i < regs.size(); i += 4) {
                for (int j = 0; j < regs[i]; ++j) {
                    p.sb_register[regs[i + 1] + j / BIT_LEN][j % BIT_LEN] =
                        p.sb_register[regs[i + 2] + j / BIT_LEN][j % BIT_LEN] &
                        p.sb_register[regs[i + 3] + j / BIT_LEN][j % BIT_LEN];
                }
            }

            Share::communicate();

            for (size_t i = 0; i < regs.size(); i += 4) {
                for (int j = 0; j < regs[i]; ++j) {
                    p.sb_register[regs[i + 1] + j / BIT_LEN][j % BIT_LEN].complete_and();
                }
            }
            return;
        case Opcode::NOTS:
            for (int i = 0; i < int(n); ++i)
                p.sb_register[regs[0] + i / BIT_LEN][i % BIT_LEN] =
                    ~(p.sb_register[regs[1] + i / BIT_LEN][i % BIT_LEN]);
            return;
        case Opcode::NOTCB: {
            long cur = n;
            for (size_t i = 0; i < div_ceil(n, BIT_LEN); ++i) {
                long bits = std::min(cur, long(BIT_LEN));
                auto num = ~p.cb_register[regs[1] + i];
                p.cb_register[regs[0] + i] =
                    bits == BIT_LEN ? num : num & int64_t((1lu << bits) - 1lu);
                cur -= BIT_LEN;
            }
            return;
        }
        case Opcode::XORCB: {
            long cur = n;
            for (size_t i = 0; i < div_ceil(n, BIT_LEN); ++i) {
                long bits = std::min(cur, long(BIT_LEN));
                auto num = p.cb_register[regs[1] + i] ^ p.cb_register[regs[2] + i];
                p.cb_register[regs[0] + i] =
                    bits == BIT_LEN ? num : num & int64_t((1lu << bits) - 1lu);
                cur -= BIT_LEN;
            }
            return;
        }
        case Opcode::ADDCB:
            p.cb_register[regs[0] + vec] =
                p.cb_register[regs[1] + vec] + p.cb_register[regs[2] + vec];
            return;
        case Opcode::ADDCBI:
            p.cb_register[regs[0] + vec] = p.cb_register[regs[1] + vec] + int64_t(int(n));
            break;
        case Opcode::MULCBI:
            p.cb_register[regs[0] + vec] = p.cb_register[regs[1] + vec] * int64_t(int(n));
            break;
        case Opcode::XORCBI:
            p.cb_register[regs[0] + vec] = p.cb_register[regs[1] + vec] ^ int64_t(int(n));
            break;
        case Opcode::SHRCBI:
            p.cb_register[regs[0] + vec] = p.cb_register[regs[1] + vec] >> int64_t(int(n));
            break;
        case Opcode::SHLCBI:
            p.cb_register[regs[0] + vec] = p.cb_register[regs[1] + vec] << int64_t(int(n));
            break;
        case Opcode::REVEAL:
            for (size_t i = 0; i < regs.size(); i += 3) {
                for (int j = 0; j < regs[i]; ++j) {
                    p.sb_register[regs[i + 2] + j / BIT_LEN][j % BIT_LEN].prepare_reveal_to_all();
                }
            }

            Share::communicate();

            for (size_t i = 0; i < regs.size(); i += 3) {
                for (int j = 0; j < div_ceil(regs[i], BIT_LEN); ++j)
                    p.cb_register[regs[i + 1] + j] = ZERO;
                for (int j = 0; j < regs[i]; ++j) {
                    p.cb_register[regs[i + 1] + j / BIT_LEN] |=
                        (BitType(p.sb_register[regs[i + 2] + j / BIT_LEN][j % BIT_LEN]
                             .complete_reveal_to_all()) &
                         1u)
                        << int64_t(j % BIT_LEN);
                }
            }
            break;
        case Opcode::PRINT_REG_SIGNED: {
            const auto& nums = p.cb_register[regs[0]].get_all();
            m.get_out() << "(";
            int64_t reg_val = nums[0];
            // long cur = 0;
            assert(n <= BIT_LEN);

            unsigned n_shift = 0;
            if (n > 1)
                n_shift = sizeof(int64_t) * 8 - n;
            if (n_shift > 63)
                n_shift = 0;

            m.get_out() << (reg_val << n_shift >> n_shift); // signed shift
            for (size_t i = 1; i < nums.size(); ++i) {
                int64_t reg_val = nums[i];

                unsigned n_shift = 0;
                if (n > 1)
                    n_shift = sizeof(int64_t) * 8 - n;
                if (n_shift > 63)
                    n_shift = 0;

                m.get_out() << ", " << (reg_val << n_shift >> n_shift);
            }
            m.get_out() << ")";
            break;
        }
        case Opcode::PRINT_FLOAT_PREC:
            p.precision = int(n);
            return;
        case Opcode::PRINT_FLOAT_PLAIN: {
            if (size > 1)
                m.get_out() << "[";

            for (size_t vec = 0; vec < size; ++vec) {
                if (size > 1 and vec != 0)
                    m.get_out() << ", ";
                const auto& sigs = p.c_register[regs[0] + vec].get_all();
                const auto& exp = p.c_register[regs[1] + vec].get_all();
                const auto& zero = p.c_register[regs[2] + vec].get_all();
                const auto& sign = p.c_register[regs[3] + vec].get_all();
                const auto& nan = p.c_register[regs[4] + vec].get_all();

#if BITLENGTH != DATTYPE
                m.get_out() << "(";
#endif

                if (nan[0]) {
                    m.get_out() << "NaN";
                    return;
                } else if (zero[0]) {
                    m.get_out() << "0";
                    return;
                }

                double res = sigs[0] * powf(2, exp[0]) * (sign[0] == 1 ? -1.f : 1.f);
                m.get_out() << std::setprecision(p.precision) << res;

                for (size_t i = 1; i < SIZE_VEC; ++i) {
                    if (nan[i]) {
                        m.get_out() << "NaN";
                        return;
                    } else if (zero[i]) {
                        m.get_out() << "0";
                        return;
                    }

                    double res = sigs[i] * powf(2, exp[i]) * (sign[i] == 1 ? -1.f : 1.f);
                    m.get_out() << ", " << res;
                }

#if BITLENGTH != DATTYPE
                m.get_out() << ")";
#endif
            }
            if (size > 1)
                m.get_out() << "]";
            return;
        }
        case Opcode::BITCOMS: {
            size_t dest = regs[0];
            for (size_t i = 0; i < BIT_LEN; i++) {
                p.sb_register[dest][i] = ZERO;
            }

            if (BIT_LEN <= regs.size() - 2) {
                log(Level::WARNING, "Note: bits should be compiled with BITLENGTH = 64");
            }
            for (size_t i = 1; i < regs.size(); i++) {
                p.sb_register[dest][i - 1] = p.sb_register[regs[i]][0];
            }
            break;
        }
        case Opcode::BITDECS: {
            size_t source = regs[0];

            if (BIT_LEN <= regs.size() - 2) {
                log(Level::WARNING, "Note: bits should be compiled with BITLENGTH = 64");
            }
            for (size_t i = 1; i < regs.size(); i++) {
                p.sb_register[regs[i]] = p.sb_register[regs[i]] ^ p.sb_register[regs[i]];
                p.sb_register[regs[i]][0] = p.sb_register[source][i - 1];
            }
            break;
        }
        case Opcode::TRANSPOSE: {
            size_t outs = n;
            size_t bits = regs.size() - outs;
            for (size_t i = 0; i < outs; ++i) {
                for (size_t j = 0; j < bits; ++j)
                    p.sb_register[regs[i] + j / BIT_LEN][j % BIT_LEN] =
                        p.sb_register[regs[outs + j] + i / BIT_LEN][i % BIT_LEN];
            }
            return;
        }
        case Opcode::PICKS:
            p.s_register[regs[0] + vec] = p.s_register[regs[1] + regs[2] + int(n) * vec];
            break;
        case Opcode::MOVS:
            p.s_register[regs[0] + vec] = p.s_register[regs[1] + vec];
            break;
        case Opcode::MOVC:
            p.c_register[regs[0] + vec] = p.c_register[regs[1] + vec];
            break;
        case Opcode::MOVINT:
            p.i_register[regs[0] + vec] = p.i_register[regs[1] + vec];
            break;
        case Opcode::CONVINT:
            p.c_register[regs[0] + vec] = p.i_register[regs[1] + vec];
            break;
        case Opcode::MOVSB:
            for (size_t i = 0; i < div_ceil(n, BIT_LEN); ++i)
                p.sb_register[i + regs[0]] = p.sb_register[i + regs[1]];
            break;
        case Opcode::INPUTBVEC:
            p.inputbvec(regs);
            return;
        case Opcode::SHUFFLE:
            for (size_t i = 0; i < size; ++i)
                p.i_register[regs[0] + i] = p.i_register[regs[1] + i];

            for (size_t i = 0; i < size; ++i) {
                size_t j = m.get_random() % (size - i); // should be shared random value
                std::swap(p.i_register[regs[0] + i], p.i_register[regs[0] + i + j]);
            }
            return;
        case Opcode::JMP:
            pc += (signed int)n;
            break;
        case Opcode::JMPI:
            pc += (signed int)p.i_register[regs[0] + vec].get();
            break;
        case Opcode::JMPNZ:
            if (p.i_register[regs[0]].get() != 0)
                pc += (signed int)n;
            return;
        case Opcode::JMPEQZ:
            if (p.i_register[regs[0]].get() == 0)
                pc += (signed int)n;
            return;
        case Opcode::BIT: {
            vector<sint> bits1;
            vector<sint> bits2;
            vector<sint> bits3;
            bits1.resize(size);
            bits2.resize(size);
            bits3.resize(size);

            for (size_t vec = 0; vec < size; ++vec) { // every party creates <size> random bits
                DATATYPE bit = PROMOTE(m.get_random_diff() & 1);

                bits1[vec].template prepare_receive_from<P_0>(bit);
                bits2[vec].template prepare_receive_from<P_1>(bit);
                bits3[vec].template prepare_receive_from<P_2>(bit);
            }

            Share::communicate();

            for (size_t vec = 0; vec < size; ++vec) {
                bits1[vec].template complete_receive_from<P_0>();
                bits2[vec].template complete_receive_from<P_1>();
                bits3[vec].template complete_receive_from<P_2>();
            }
            vector<sint> res;
            res.resize(size);
            p.xor_arith(bits1, bits2, res);
            p.xor_arith(res, bits3, res);

            for (size_t vec = 0; vec < size; ++vec) {
                p.s_register[regs[0] + vec] = res[vec];
            }
            return;
        }
        case Opcode::DABIT: { // TODO
            unsigned bit = m.get_random() & 1;
            p.s_register[regs[0] + vec] = bit;
            p.sb_register[regs[1] + vec / BIT_LEN][vec % BIT_LEN] = BitShare(PROMOTE(bit));
            break;
        }
        case Opcode::CONVCBITVEC:
            for (size_t i = 0; i < n; ++i) {
                IntType res(vector<int64_t>(0));
                const auto& nums = (p.cb_register[regs[1] + i / BIT_LEN]).get_all();
                for (auto& ele : nums)
                    res.add((ele >> INT_TYPE(i % BIT_LEN)) & INT_TYPE(1));
                p.i_register[regs[0] + i] = res;
            }
            break;
        case Opcode::CONVSINT: {
            for (int i = 0; i < regs[0]; ++i) {
                p.sb_register[regs[1] + i / BIT_LEN][i % BIT_LEN] =
                    BitShare(((p.i_register[regs[2] + i / 64] >> int64_t(i % 64)) & 1l).get_type());
            }
            return;
        }
        case Opcode::CONVCINT: {
            p.cb_register[regs[0] + vec] = p.i_register[regs[1] + vec];
            break;
        }
        case Opcode::CONVCINTVEC: {
            for (size_t i = 0; i < get_size(); ++i) {
                auto source = p.c_register[regs[0] + i];
                for (size_t j = 1; j < regs.size(); ++j) {
                    if (i % BIT_LEN == 0)
                        p.cb_register[regs[j] + i / BIT_LEN] = ZERO;
                    p.cb_register[regs[j] + i / BIT_LEN] ^=
                        (((source >> INT_TYPE(j - 1)) & INT_TYPE(1)) << INT_TYPE(i % BIT_LEN))
                            .get_type();
                }
            }
            return;
        }
        case Opcode::CONVCBIT2S:
            for (int i = 0; i < int(n); ++i) {
                p.sb_register[regs[0] + i / BIT_LEN][i % BIT_LEN] =
                    ((p.cb_register[regs[1] + i / BIT_LEN] >> int64_t(i % BIT_LEN)) & int64_t(1))
                        .get_type();
            }
            return;
        case Opcode::CONVMODP:
            if (n == 0) { // unsigned conversion
                IntType tmp(vector<int64_t>(0l));
                const auto& nums = p.c_register[regs[1] + vec].get_all();

                for (unsigned long ele : nums)
                    tmp.add(ele);

                p.i_register[regs[0] + vec] = tmp;
            } else if (n <= 64) {
                auto dest = p.i_register.begin() + regs[0] + vec;
                auto x = p.c_register[regs[1] + vec];
                if (n == 1) {
                    IntType tmp(vector<int64_t>(0l));
                    const auto& vec = x.get_all();

                    for (int64_t ele : vec)
                        tmp.add(ele & 1);

                    *dest = tmp;
                } else if (n == 64) {
                    IntType tmp(vector<int64_t>(0l));
                    const auto& vec = x.get_all();

                    for (int64_t ele : vec)
                        tmp.add(ele);

                    *dest = tmp;
                } else {
                    IntType tmp(vector<int64_t>(0l));
                    const auto& vec = x.get_all();

                    for (INT_TYPE ele : vec) {
                        auto a = std::abs(ele);
                        a &= INT_TYPE(~(uint64_t(-1) << (n - 1) << 1));
                        if (ele < 0)
                            a = -a;
                        tmp.add(a);
                    }

                    *dest = tmp;
                }
            } else {
                log(Level::WARNING, "CONVMODP with bit size > 64 is not possible");
            }
            break;
        case Opcode::BITDECINT: {
            const auto x = p.i_register[regs[0] + vec];

            for (size_t i = 1; i < regs.size(); ++i) {
                p.i_register[regs[i] + vec] = (x >> int64_t(i - 1)) & 1l;
            }
            break;
        }
        case Opcode::ANDRSVEC:
            p.andrsvec(regs);
            return;
        case Opcode::ANDRS:
            for (size_t i = 0; i < regs.size(); i += 4) {
                int vec_size = regs[i];
                for (int j = 0; j < vec_size; ++j) {
                    p.sb_register[regs[i + 1] + j / BIT_LEN][j % BIT_LEN] =
                        p.sb_register[regs[i + 2] + j / BIT_LEN][j % BIT_LEN] &
                        p.sb_register[regs[i + 3]][0];
                }
            }

            Share::communicate();

            for (size_t i = 0; i < regs.size(); i += 4) {
                int vec_size = regs[i];
                for (int j = 0; j < vec_size; ++j) {
                    p.sb_register[regs[i + 1] + j / BIT_LEN][j % BIT_LEN].complete_and();
                }
            }
            return;
        case Opcode::MULRS:
            for (size_t i = 0; i < regs.size(); i += 4) {
                int vec_size = regs[i];
                for (int j = 0; j < vec_size; ++j) {
                    p.s_register[regs[i + 1] + j] =
                        p.s_register[regs[i + 2] + j].prepare_mult(p.s_register[regs[i + 3]]);
                }
            }

            Share::communicate();

            for (size_t i = 0; i < regs.size(); i += 4) {
                int vec_size = regs[i];
                for (int j = 0; j < vec_size; ++j) {
                    p.s_register[regs[i + 1] + j].complete_mult_without_trunc();
                }
            }
            return;
        case Opcode::LDMINT:
            p.i_register[regs[0] + vec] = m.ci_mem[n + vec];
            break;
        case Opcode::STMINT:
            m.ci_mem[n + vec] = p.i_register[regs[0] + vec];
            break;
        case Opcode::CRASH:
            if (p.i_register[regs[0]].get() != 0)
                log(Level::ERROR, "CRASH");
            break;
        case Opcode::ACTIVE:
        case Opcode::REQBL:
            // std::cerr << "DEBUG: ACTIVE " << n << "\n";
            break;
        case Opcode::GLDMC:
            // std::cerr << "DEBUG: GLDMC\n";
            break;
        case Opcode::GLDMS:
            // std::cerr << "DEBUG: GLDMS\n";
            break;
        case Opcode::NPLAYERS:
            p.i_register[regs[0] + vec] = num_players;
            break;
        case Opcode::THRESHOLD:
            p.i_register[regs[0] + vec] = 0;
            break;
        case Opcode::PLAYERID:
            p.i_register[regs[0] + vec] = PARTY;
            break;
        case Opcode::PUSHINT:
            p.i_stack.push(p.i_register[regs[0] + vec]);
            break;
        case Opcode::POPINT:
            p.i_register[regs[0] + vec] = p.i_stack.top();
            p.i_stack.pop();
            break;
        case Opcode::START:
            m.start(n);
            return;
        case Opcode::STOP:
            m.stop(n);
            return;
        case Opcode::TIME:
            m.time();
            return;
        case Opcode::CISC: {
            p.cisc(regs, cisc);
            return;
        }
        case Opcode::RUN_TAPE:
            // for (size_t i = 0; i < regs.size(); i += 3)
            //     m.run_tape(regs[i + 1], regs[i + 2], regs[i]);
            assert(regs.size() == 3); // start only one thread
            assert(regs[1] !=
                   0); // no copy needed (test) since only the main thread(0) has important data
            assert(regs[0] == 1); // assume it's the only thread running
            m.run_tape_no_thread(regs[1], regs[2]);
            return;
        case Opcode::JOIN_TAPE:
            // m.join_tape(int(n));
            assert(n == 1); // multithreading not supported
            return;
        case Opcode::LDTN:
            p.i_register[regs[0] + vec] = p.thread_num;
            break;
        case Opcode::NONE:
            log(Level::WARNING, "unknown opcode: ", n);
            break;
        default:
            log(Level::WARNING, "not implemented: ", static_cast<unsigned>(op));
        }
    }
}

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N>
void Program<int_t, cint, Share, sint, sbit, BitShare, N>::inputbvec(const vector<int>& regs) {
    for (size_t i = 0; i < regs.size(); i += 3) {
        unsigned bits = regs[i] - 3;
        assert(bits == BITLENGTH and "BITLENGTH must equal -B <int>");

        for (size_t j = 0; j < bits; ++j) {
            auto input = get_next_bit(regs[i + 2]);
            if (j >= BIT_LEN)
                log(Level::ERROR, "input is too big");
            switch (regs[i + 2]) {
            case 0:
                sb_register[regs[i + 3 + j]][0].template prepare_receive_from<P_0>(input);
                break;
            case 1:
                sb_register[regs[i + 3 + j]][0].template prepare_receive_from<P_1>(input);
                break;
            case 2:
                sb_register[regs[i + 3 + j]][0].template prepare_receive_from<P_2>(input);
                break;
            }
        }
        i += bits;
    }

    Share::communicate();

    for (size_t i = 0; i < regs.size(); i += 3) {
        unsigned bits = regs[i] - 3;
        for (size_t j = 0; j < bits; ++j) {
            switch (regs[i + 2]) {
            case 0:
                sb_register[regs[i + 3 + j]][0].template complete_receive_from<P_0>();
                break;
            case 1:
                sb_register[regs[i + 3 + j]][0].template complete_receive_from<P_1>();
                break;
            case 2:
                sb_register[regs[i + 3 + j]][0].template complete_receive_from<P_2>();
                break;
            }
        }
        i += bits;
    }
}

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N>
void Program<int_t, cint, Share, sint, sbit, BitShare, N>::inputmixed(const vector<int>& regs,
                                                                      bool from_reg,
                                                                      const size_t& vec) {
    for (size_t i = 0; i < regs.size(); i += 3) {
        for (size_t offset = 0; offset < vec; ++offset) {
            alignas(DATATYPE) UINT_TYPE input[DATTYPE / BITLENGTH];
            int dest = regs[i + 1] + offset;

            int player = 0;

            switch (regs[i]) {
            case 0: { // int
                player = from_reg ? i_register[regs[i + 2]].get() : regs[i + 2];

                auto res = next_input<SIZE_VEC>(player, thread_id);
                for (size_t j = 0; j < SIZE_VEC; ++j)
                    input[j] = res[j];

                break;
            }
            case 1: { // fix
                player = from_reg ? i_register[regs[i + 3]].get() : regs[i + 3];

                auto tmp = next_input_f<SIZE_VEC>(player, thread_id);
                for (size_t j = 0; j < SIZE_VEC; ++j)
                    input[j] = (static_cast<INT_TYPE>(tmp[j] * (1u << regs[i + 2])));
                break;
            }
            case 2: // float
                break;
            }

            DATATYPE in;
            orthogonalize_arithmetic(input, &in, 1);

            switch (player) {
            case 0:
                s_register[dest].template prepare_receive_from<P_0>(in);
                break;
            case 1:
                s_register[dest].template prepare_receive_from<P_1>(in);
                break;
            case 2:
                s_register[dest].template prepare_receive_from<P_2>(in);
                break;
            }
        }
        switch (regs[i]) {
        case 0:
        case 2:
            break;
        case 1:
            i += 1;
            break;
        default:
            log(Level::ERROR, "inputmixed: unknown type");
        }
    }

    Share::communicate();

    for (size_t i = 0; i < regs.size(); i += 3) {
        for (size_t offset = 0; offset < vec; ++offset) {
            int dest = regs[i + 1] + offset;

            int player = 0;
            switch (regs[i]) {
            case 1:
                player = from_reg ? i_register[regs[i + 3]].get() : regs[i + 3];
                break;
            case 0:
            case 2:
                player = from_reg ? i_register[regs[i + 2]].get() : regs[i + 2];
                break;
            }

            switch (player) {
            case 0:
                s_register[dest].template complete_receive_from<P_0>();
                break;
            case 1:
                s_register[dest].template complete_receive_from<P_1>();
                break;
            case 2:
                s_register[dest].template complete_receive_from<P_2>();
                break;
            }
        }
        switch (regs[i]) {
        case 0:
        case 2:
            break;
        case 1:
            i += 1;
            break;
        default:
            log(Level::ERROR, "inputmixed: unknown type");
        }
    }
}

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N>
void Program<int_t, cint, Share, sint, sbit, BitShare, N>::popen(const vector<int>& regs,
                                                                 const size_t& size) {
    for (size_t i = 0; i < regs.size(); i += 2) {
        for (size_t vec = 0; vec < size; ++vec) {
            s_register[regs[i + 1] + vec].prepare_reveal_to_all();
        }
    }

    Share::communicate();

    for (size_t i = 0; i < regs.size(); i += 2)
        for (size_t vec = 0; vec < size; ++vec) {
            std::vector<UINT_TYPE> res;
            res.resize(DATTYPE / BITLENGTH); // TODO
            s_register[regs[i + 1] + vec].complete_reveal_to_all(res.data());
            c_register[regs[i] + vec] = res;
        }
}

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N>
void Program<int_t, cint, Share, sint, sbit, BitShare, N>::mulm(const vector<int>& regs,
                                                                const size_t& size) {
    for (size_t vec = 0; vec < size; ++vec)
        s_register[regs[0] + vec] =
            s_register[regs[1] + vec].mult_public_dat(c_register[regs[2] + vec].get_type());
}

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N>
void Program<int_t, cint, Share, sint, sbit, BitShare, N>::muls(const vector<int>& regs) {
    for (size_t i = 0; i < regs.size(); i += 4) {
        for (int j = 0; j < regs[i]; j++) {
            s_register[regs[i + 1] + j] =
                (s_register[regs[i + 2] + j].prepare_mult(s_register[regs[i + 3] + j]));
        }
    }

    sint::communicate();

    for (size_t i = 0; i < regs.size(); i += 4) {
        for (int j = 0; j < regs[i]; j++) {
            s_register[regs[i + 1] + j].complete_mult_without_trunc();
        }
    }
}

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N>
void Program<int_t, cint, Share, sint, sbit, BitShare, N>::andrsvec(const vector<int>& regs) {
    auto it = regs.begin();
    while (it < regs.end()) {
        int total = *it++;
        int n_args = (total - 3) / 2;
        int vec = *it++;

        for (int i = 0; i < vec; ++i) {
            for (int j = 0; j < n_args; ++j) {
                sb_register[*(it + j) + i / BIT_LEN][i % BIT_LEN] =
                    sb_register[*(it + n_args) + i / BIT_LEN][i % BIT_LEN] &
                    sb_register[*(it + n_args + 1 + j) + i / BIT_LEN][i % BIT_LEN];
            }
        }
        it += total - 2;
    }

    Share::communicate();

    it = regs.begin();
    while (it < regs.end()) {
        int total = *it++;
        int n_args = (total - 3) / 2;
        int vec = *it++;

        for (int i = 0; i < vec; ++i) {
            for (int j = 0; j < n_args; ++j) {
                sb_register[*(it + j) + i / BIT_LEN][i % BIT_LEN].complete_and();
            }
        }
        it += total - 2;
    }
}

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N>
void Program<int_t, cint, Share, sint, sbit, BitShare, N>::matmulsm(
    const vector<int>& regs, Machine<int_t, cint, Share, sint, sbit, BitShare, N>& m) {
    auto res = s_register.begin() + regs[0];
    auto source1 = m.s_mem.begin() + i_register[regs[1]].get();
    auto source2 = m.s_mem.begin() + i_register[regs[2]].get();

    const int& rows = regs[3]; // for 1st but also final
    const int& cols = regs[5]; // for 2nd but also final

    for (int i = 0; i < rows; ++i) {
        auto row_1 = i_register[regs[6] + i].get(); // rows to use what ever that means

        for (int j = 0; j < cols; ++j) {
            matmulsm_prepare(regs, row_1, j, source1, source2); // calculate dotprod
        }
    }

    Share::communicate();

    for (int i = 0; i < rows; ++i) {
        auto row_1 = i_register[regs[6] + i].get();
        for (int j = 0; j < cols; ++j) {
            (res + row_1 * cols + j)->complete_mult_without_trunc();
        }
    }
}

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N>
template <class iterator>
void Program<int_t, cint, Share, sint, sbit, BitShare, N>::matmulsm_prepare(
    const vector<int>& regs, const int& row_1, const int& j, iterator source1, iterator source2) {
    auto col_2 = i_register[regs[9] + j].get(); // column of 2nd factor

    s_register[regs[0] + row_1 * regs[5] + j] = 0;
    for (int k = 0; k < regs[4]; ++k) {             // length of dot_prod
        auto col_1 = i_register[regs[7] + k].get(); // column of first factor
        auto row_2 = i_register[regs[8] + k].get(); // row of 2nd factor

        iterator cur_1 = source1 + row_1 * regs[10] + col_1;
        iterator cur_2 = source2 + row_2 * regs[11] + col_2;

        s_register[regs[0] + row_1 * regs[5] + j] += cur_1->prepare_dot(*cur_2);
    }
    s_register[regs[0] + row_1 * regs[5] + j].mask_and_send_dot_without_trunc();
}

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N>
void Program<int_t, cint, Share, sint, sbit, BitShare, N>::matmuls(const vector<int>& regs) {
    assert(regs.size() % 6 == 0);
    for (size_t vec = 0; vec < regs.size(); vec += 6) {
        const int& rows1 = regs[vec + 3];
        const int& cr = regs[vec + 4];
        const int& cols2 = regs[vec + 5];
        for (int i = 0; i < rows1; ++i) {     // rows
            for (int j = 0; j < cols2; ++j) { // cols
                s_register[regs[vec] + i * rows1 + j] = 0;
                for (int k = 0; k < cr; ++k) // cols/rows
                    s_register[regs[vec] + i * rows1 + j] +=
                        s_register[regs[vec + 1] + i * cr + k].prepare_dot(
                            s_register[regs[vec + 2] + k * cols2 + j]);
                s_register[regs[vec] + i * rows1 + j].mask_and_send_dot_without_trunc();
            }
        }
    }

    Share::communicate();

    for (size_t vec = 0; vec < regs.size(); vec += 6) {
        const int& rows = regs[vec + 3];
        const int& cols = regs[vec + 5];
        for (int i = 0; i < rows; ++i) {   // rows
            for (int j = 0; j < cols; ++j) // cols
                s_register[regs[vec] + i * rows + j].complete_mult_without_trunc();
        }
    }
}

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N>
void Program<int_t, cint, Share, sint, sbit, BitShare, N>::dotprods(const vector<int>& regs,
                                                                    const size_t& size) {
    for (size_t vec = 0; vec < size; ++vec) {
        for (auto it = regs.begin(); it != regs.end();) {
            auto next = it + *it;
            int dest = *(it + 1);
            it += 2;
            s_register[dest] = 0;

            while (it != next) {
                s_register[dest] +=
                    s_register[*(it++) + vec].prepare_dot(s_register[*(it++) + vec]);
            }
            s_register[dest].mask_and_send_dot_without_trunc();
        }
    }

    Share::communicate();

    for (size_t vec = 0; vec < size; ++vec) {
        for (auto it = regs.begin(); it != regs.end();) {
            auto next = it + *it;
            it++;
            s_register[*it + vec].complete_mult_without_trunc();
            it = next;
        }
    }
}

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N>
void Program<int_t, cint, Share, sint, sbit, BitShare, N>::cisc(const vector<int>& regs,
                                                                const std::string_view cisc) {
    if (cisc.starts_with("LTZ")) {
        vector<sint> op(0);
        vector<int> ires(0);
        op.reserve(regs.size() / 6);
        ires.reserve(regs.size() / 6);

        assert(regs.size() % 6 == 0);

        for (size_t i = 0; i < regs.size(); i += 6) {
            for (size_t vec = 0; vec < regs[i + 1]; ++vec) {
                op.push_back(s_register[regs[i + 3] + vec]); // operant
                ires.push_back(regs[i + 2] + vec);           // dest
            }
        }

        sint* res = new sint[op.size()];
        pack_additive<0, BITLENGTH>(op.data(), res, op.size(), LTZ<0, BITLENGTH, Share, DATATYPE>);

        for (size_t i = 0; i < ires.size(); ++i)
            s_register[ires[i]] = res[i];
        delete[] res;
    } else if (cisc.starts_with("EQZ")) {
        vector<sint> op(0);
        vector<int> ires(0);
        op.reserve(regs.size() / 6);
        ires.reserve(regs.size() / 6);

        assert(regs.size() % 6 == 0);

        for (size_t i = 0; i < regs.size(); i += 6) {
            for (size_t vec = 0; vec < regs[i + 1]; ++vec) {
                op.push_back(s_register[regs[i + 3] + vec]);
                ires.push_back(regs[i + 2] + vec);
            }
        }

        sint* res = new sint[op.size()];
        pack_additive<0, BITLENGTH>(op.data(), res, op.size(), EQZ<0, BITLENGTH, Share, DATATYPE>);

        for (size_t i = 0; i < ires.size(); ++i)
            s_register[ires[i]] = res[i];
        delete[] res;
    }
}

template <class int_t, class cint, class Share, class sint, template <int, class> class sbit,
          class BitShare, int N>
void Program<int_t, cint, Share, sint, sbit, BitShare, N>::xor_arith(const vector<sint>& x,
                                                                     const vector<sint>& y,
                                                                     vector<sint>& res) {
    vector<sint> tmp;
    tmp.resize(x.size());

    for (size_t i = 0; i < x.size(); ++i) {
        tmp[i] = x[i].prepare_mult(y[i]);
    }

    Share::communicate();

    for (size_t i = 0; i < x.size(); ++i) {
        tmp[i].complete_mult_without_trunc();
        res[i] = x[i] + y[i] - tmp[i].mult_public(2);
    }
}

} // namespace IR

#endif
