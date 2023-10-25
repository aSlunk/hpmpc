#pragma once
#include "../../generic_share.hpp"
#define PRE_SHARE OEC_MAL3_Share
template <typename Datatype>
class OEC_MAL3_Share
{
private:
    Datatype r0;
    Datatype r1;
public:
    
OEC_MAL3_Share() {}
OEC_MAL3_Share(Datatype r0, Datatype r1) : r0(r0), r1(r1) {}
OEC_MAL3_Share(Datatype r0) : r0(r0) {}


    

OEC_MAL3_Share public_val(Datatype a)
{
    return OEC_MAL3_Share(SET_ALL_ZERO(),SET_ALL_ZERO());
}

OEC_MAL3_Share Not() const
{
   return *this;
}

template <typename func_add>
OEC_MAL3_Share Add(OEC_MAL3_Share b, func_add ADD) const
{
   return OEC_MAL3_Share(ADD(r0,b.r0),ADD(r1,b.r1));
}



template <typename func_add, typename func_sub, typename func_mul>
    OEC_MAL3_Share prepare_mult(OEC_MAL3_Share b, func_add ADD, func_sub SUB, func_mul MULT) const
{
OEC_MAL3_Share c;
c.r0 = getRandomVal(P_123); // r123_1
c.r1 = ADD(getRandomVal(P_023),getRandomVal(P_013)); // x1 

/* Datatype r124 = getRandomVal(P_013); // used for verification */
/* Datatype r234 = getRandomVal(P_123); // Probably sufficient to only generate with P_2(-> P_3 in paper) -> no because of verification */

/* Datatype o1 = ADD( x1y1, getRandomVal(P_013)); */
Datatype o1 = ADD(c.r1, ADD(MULT(r1,b.r1), getRandomVal(P_013)));

#if PROTOCOL == 11
/* Datatype o4 = ADD(SUB(SUB(x1y1, MULT(a.r0,b.r1)) ,MULT(a.r1,b.r0)),getRandomVal(P_123_2)); // r123_2 */
/* Datatype o4 = ADD(SUB(MULT(a.r1, SUB(b.r0,b.r1)) ,MULT(b.r1,a.r0)),getRandomVal(P_123_2)); // r123_2 */
Datatype o4 = ADD(SUB(MULT(r1, SUB(b.r1,b.r0)) ,MULT(b.r1,r0)),getRandomVal(P_123_2)); // r123_2
#else
Datatype o4 = ADD(SUB(MULT(r1, SUB(b.r1,b.r0)) ,MULT(b.r1,r0)),SUB(getRandomVal(P_123_2),c.r0)); // r123_2
#endif

/* Datatype o4 = ADD( SUB( MULT(a.r0,b.r1) ,MULT(a.r1,b.r0)),getRandomVal(P_123)); */
/* o4 = XOR(o4,o1); //computationally easier to let P_3 do it here instead of P_0 later */
#if PROTOCOL == 12
#if PRE == 1
pre_send_to_live(P_2, o1);
#else
send_to_live(P_2, o1);
#endif
#else
store_compare_view(P_2, o1);
#endif


#if PROTOCOL == 10 || PROTOCOL == 12
#if PRE == 1
pre_send_to_live(P_0, o4);
#else
send_to_live(P_0, o4);
#endif
#elif PROTOCOL == 11
store_compare_view(P_0,o4);
#endif
return c;
}

template <typename func_add, typename func_sub, typename func_mul>
OEC_MAL3_Share prepare_dot(const OEC_MAL3_Share b, func_add ADD, func_sub SUB, func_mul MULT) const
{
OEC_MAL3_Share c;
c.r1 = MULT(r1,b.r1); // store o_1
c.r0 = SUB(MULT(r1, SUB(b.r1,b.r0)) ,MULT(b.r1,r0)); // store o_4
return c;
}
    template <typename func_add, typename func_sub, typename func_trunc>
void mask_and_send_dot_with_trunc(func_add ADD, func_sub SUB, func_trunc TRUNC)
{
r1 = TRUNC(SUB(ADD(getRandomVal(P_013), getRandomVal(P_023)), r1)); // z_0 = [r_0,1,3 + r_0,2,3 - x_0 y_0]^t
#if PROTOCOL == 11
store_compare_view(P_0, ADD(SUB(r0, ADD(getRandomVal(P_012), getRandomVal(P_023))), getRandomVal(P_123)));  // v^3 = .. - r_0,1,2 - r_0,2,3 + r_1,2,3
#else
#if PRE == 1
pre_send_to_live(P_0, ADD(SUB(r0, ADD(getRandomVal(P_012), getRandomVal(P_023))), getRandomVal(P_123));  // m^3 = .. - r_0,1,2 - r_0,2,3 + r_1,2,3
#else
send_to_live(P_0, ADD(SUB(r0, ADD(getRandomVal(P_013), getRandomVal(P_023))), getRandomVal(P_123)));  // m^3 = .. - r_0,1,2 - r_0,2,3 + r_1,2,3
#endif
#endif
store_compare_view(P_2, r1); // compare m^0
}

    template <typename func_add, typename func_sub, typename func_trunc>
void complete_mult_with_trunc(func_add ADD, func_sub SUB, func_trunc TRUNC)
{
}

template <typename func_add, typename func_sub>
void mask_and_send_dot(func_add ADD, func_sub SUB)
{

Datatype rc0 = getRandomVal(P_123); // r123_1
Datatype rc1 = ADD(getRandomVal(P_023),getRandomVal(P_013)); // x0 

Datatype o1 = ADD(rc0, ADD(r1, getRandomVal(P_013)));
#if PROTOCOL == 11
Datatype o4 = ADD(r0,getRandomVal(P_123_2)); // r123_2
#else
Datatype o4 = ADD(r0,SUB(getRandomVal(P_123_2),r0)); // r123_2
#endif

r0 = rc0;
r1 = rc1;

#if PROTOCOL == 12
#if PRE == 1
pre_send_to_live(P_2, o1);
#else
send_to_live(P_2, o1);
#endif
#else
store_compare_view(P_2, o1);
#endif


#if PROTOCOL == 10 || PROTOCOL == 12
#if PRE == 1
pre_send_to_live(P_0, o4);
#else
send_to_live(P_0, o4);
#endif
#elif PROTOCOL == 11
store_compare_view(P_0,o4);
#endif

}



template <typename func_add, typename func_sub>
void complete_mult(func_add ADD, func_sub SUB)
{
}


void prepare_reveal_to_all()
{
#if PRE == 1
    pre_send_to_live(P_0, r0);
#else
    send_to_live(P_0, r0);
#endif
}    


template <typename func_add, typename func_sub>
Datatype complete_Reveal(func_add ADD, func_sub SUB)
{
#if PRE == 0
Datatype result = SUB(receive_from_live(P_0),r0);
store_compare_view(P_123, r1);
store_compare_view(P_0123, result);
return result;
#else
#if PRE == 1 && HAS_POST_PROTOCOL == 1
store_output_share(r0);
store_output_share(r1);
#endif
return r0;
#endif


}



template <int id, typename func_add, typename func_sub>
void prepare_receive_from(func_add ADD, func_sub SUB)
{
if constexpr(id == PSELF)
{
    Datatype v = get_input_live();
    Datatype x_1 = getRandomVal(P_013);
    Datatype x_2 = getRandomVal(P_023);
    Datatype u = getRandomVal(P_123);

    r0 = u;
    r1 = ADD(x_1,x_2);
    Datatype complete_masked = ADD(v, ADD(r1, r0));
    #if PRE == 1
    pre_send_to_live(P_0, complete_masked);
    pre_send_to_live(P_1, complete_masked);
    pre_send_to_live(P_2, complete_masked);
    #else
    send_to_live(P_0, complete_masked);
    send_to_live(P_1, complete_masked);
    send_to_live(P_2, complete_masked);
    #endif
     
}
else if constexpr(id == P_0)
{
    r0 = SET_ALL_ZERO();
    r1 = ADD(getRandomVal(P_013),getRandomVal(P_023));
    
}
else if constexpr(id == P_1)
{
    r0 = getRandomVal(P_123);
    r1 = getRandomVal(P_013);
    
}
else if constexpr(id == P_2)
{
    r0 = getRandomVal(P_123);
    r1 = getRandomVal(P_023);
    
}
}

template <int id, typename func_add, typename func_sub>
void complete_receive_from(func_add ADD, func_sub SUB)
{
}




static void send()
{
    send_live();
}

static void receive()
{
    receive_live();
}

static void communicate()
{
#if PRE == 0
    communicate_live();
#endif
}

static void prepare_A2B_S1(OEC_MAL3_Share in[], OEC_MAL3_Share out[])
{
    for(int i = 0; i < BITLENGTH; i++)
    {
        out[i].r0 = getRandomVal(P_123); // r123
        out[i].r1 = SET_ALL_ZERO(); // set share to 0
    }
}


static void prepare_A2B_S2(OEC_MAL3_Share in[], OEC_MAL3_Share out[])
{
    //convert share x0 to boolean
    Datatype temp[BITLENGTH];
        for (int j = 0; j < BITLENGTH; j++)
        {
            temp[j] = OP_SUB(SET_ALL_ZERO(), in[j].r1); // set share to -x0
        }
    unorthogonalize_arithmetic(temp, (UINT_TYPE*) temp);
    orthogonalize_boolean((UINT_TYPE*) temp, temp);

    for(int i = 0; i < BITLENGTH; i++)
    {
            out[i].r0 = SET_ALL_ZERO(); 
            out[i].r1 = temp[i]; 
            store_compare_view(P_2, FUNC_XOR(temp[i], getRandomVal(P_013))); // compare -x0 xor r0,1 with $P_2
    } 
}

static void complete_A2B_S1(OEC_MAL3_Share out[])
{
}

static void complete_A2B_S2(OEC_MAL3_Share out[])
{

}

void prepare_bit_injection_S1(OEC_MAL3_Share out[])
{
    for(int i = 0; i < BITLENGTH; i++)
    {
        out[i].r0 = getRandomVal(P_123); // r123
        out[i].r1 = SET_ALL_ZERO(); // set share to 0
    }
}

void prepare_bit_injection_S2(OEC_MAL3_Share out[])
{
    Datatype temp[BITLENGTH]{0};
    temp[BITLENGTH - 1] = r1;
    unorthogonalize_boolean(temp,(UINT_TYPE*)temp);
    orthogonalize_arithmetic((UINT_TYPE*) temp,  temp);
    for(int i = 0; i < BITLENGTH; i++)
    {
        out[i].r0 = SET_ALL_ZERO(); //w = 0
        out[i].r1 = OP_SUB(SET_ALL_ZERO(), temp[i]) ; // z_0 = - x_0
        store_compare_view(P_2, OP_ADD(temp[i],getRandomVal(P_013))); //  x_0 + r013
        
    }
}

static void complete_bit_injection_S1(OEC_MAL3_Share out[])
{
    
}

static void complete_bit_injection_S2(OEC_MAL3_Share out[])
{
}

#if MULTI_INPUT == 1

template <typename func_add, typename func_sub, typename func_mul>
    OEC_MAL3_Share prepare_mult3(OEC_MAL3_Share b, OEC_MAL3_Share c, func_add ADD, func_sub SUB, func_mul MULT) const
{
Datatype mxy = SUB(MULT(r1,b.r1),getRandomVal(P_013));
Datatype mxz = SUB(MULT(r1,c.r1),getRandomVal(P_013));
Datatype myz = SUB(MULT(b.r1,c.r1),getRandomVal(P_013));
Datatype mxyz = SUB(MULT(mxy,c.r1),getRandomVal(P_013));
Datatype ax = ADD(c.r0,c.r1);
Datatype by = ADD(b.r0,b.r1);
Datatype cz = ADD(r0,r1);
Datatype m3xy = SUB(MULT(ax,by),getRandomVal(P_123));
Datatype m3xz = SUB(MULT(ax,cz),getRandomVal(P_123));
Datatype m3yz = SUB(MULT(by,cz),getRandomVal(P_123));
Datatype m3xyz = SUB(MULT(MULT(ax,by),cz),getRandomVal(P_123));
#if PROTOCOL == 12
#if PRE == 1
pre_send_to_live(P_0, m3xy);
pre_send_to_live(P_0, m3xz);
pre_send_to_live(P_0, m3yz);
pre_send_to_live(P_0, m3xyz);
pre_send_to_live(P_2, mxy);
pre_send_to_live(P_2, mxz);
pre_send_to_live(P_2, myz);
pre_send_to_live(P_2, mxyz);
#else
send_to_live(P_0, m3xy);
send_to_live(P_0, m3xz);
send_to_live(P_0, m3yz);
send_to_live(P_0, m3xyz);
send_to_live(P_2, mxy);
send_to_live(P_2, mxz);
send_to_live(P_2, myz);
send_to_live(P_2, mxyz);
#endif
#else
send_to_live(P_0, m3xy);
send_to_live(P_0, m3xz);
send_to_live(P_0, m3yz);
send_to_live(P_0, m3xyz);
store_compare_view(P_2, mxy);
store_compare_view(P_2, mxz);
store_compare_view(P_2, myz);
store_compare_view(P_2, mxyz);
#endif
OEC_MAL3_Share d;
d.r0 = getRandomVal(P_123);
d.r1 = ADD(getRandomVal(P_013),getRandomVal(P_023));
return d;
}

template <typename func_add, typename func_sub>
void complete_mult3(func_add ADD, func_sub SUB){
}

template <typename func_add, typename func_sub, typename func_mul>
    OEC_MAL3_Share prepare_mult4(OEC_MAL3_Share b, OEC_MAL3_Share c, OEC_MAL3_Share d, func_add ADD, func_sub SUB, func_mul MULT) const
{
Datatype mxy = SUB(MULT(r1,b.r1),getRandomVal(P_013));
Datatype mxz = SUB(MULT(r1,c.r1),getRandomVal(P_013));
Datatype mxw = SUB(MULT(r1,d.r1),getRandomVal(P_013));
Datatype myz = SUB(MULT(b.r1,c.r1),getRandomVal(P_013));
Datatype myw = SUB(MULT(b.r1,d.r1),getRandomVal(P_013));
Datatype mzw = SUB(MULT(c.r1,d.r1),getRandomVal(P_013));
Datatype mxyz = SUB(MULT(mxy,c.r1),getRandomVal(P_013));
Datatype mxzw = SUB(MULT(mxz,d.r1),getRandomVal(P_013));
Datatype myzw = SUB(MULT(myz,d.r1),getRandomVal(P_013));
Datatype mxyw = SUB(MULT(mxy,d.r1),getRandomVal(P_013));
Datatype mxyzw = SUB(MULT(mxy,mzw),getRandomVal(P_013));
Datatype ax = ADD(r0,r1);
Datatype by = ADD(b.r0,b.r1);
Datatype cz = ADD(c.r0,c.r1);
Datatype dw = ADD(d.r0,d.r1);
Datatype m3xy = SUB(MULT(ax,by),getRandomVal(P_123));
Datatype m3xz = SUB(MULT(ax,cz),getRandomVal(P_123));
Datatype m3xw = SUB(MULT(ax,dw),getRandomVal(P_123));
Datatype m3yz = SUB(MULT(by,cz),getRandomVal(P_123));
Datatype m3yw = SUB(MULT(by,dw),getRandomVal(P_123));
Datatype m3zw = SUB(MULT(cz,dw),getRandomVal(P_123));
Datatype m3xyz = SUB(MULT(m3xy,cz),getRandomVal(P_123));
Datatype m3xyw = SUB(MULT(m3xy,dw),getRandomVal(P_123));
Datatype m3xzw = SUB(MULT(m3xz,dw),getRandomVal(P_123));
Datatype m3yzw = SUB(MULT(m3yz,dw),getRandomVal(P_123));
Datatype m3xyzw = SUB(MULT(m3xy,m3zw),getRandomVal(P_123));
#if PROTOCOL == 12
#if PRE == 1
pre_send_to_live(P_0, m3xy);
pre_send_to_live(P_0, m3xz);
pre_send_to_live(P_0, m3xw);
pre_send_to_live(P_0, m3yz);
pre_send_to_live(P_0, m3yw);
pre_send_to_live(P_0, m3zw);
pre_send_to_live(P_0, m3xyz);
pre_send_to_live(P_0, m3xyw);
pre_send_to_live(P_0, m3xzw);
pre_send_to_live(P_0, m3yzw);
pre_send_to_live(P_0, m3xyzw);
pre_send_to_live(P_2, mxy);
pre_send_to_live(P_2, mxz);
pre_send_to_live(P_2, mxw);
pre_send_to_live(P_2, myz);
pre_send_to_live(P_2, myw);
pre_send_to_live(P_2, mzw);
pre_send_to_live(P_2, mxyz);
pre_send_to_live(P_2, mxyw);
pre_send_to_live(P_2, mxzw);
pre_send_to_live(P_2, myzw);
pre_send_to_live(P_2, mxyzw);
#else
send_to_live(P_0, m3xy);
send_to_live(P_0, m3xz);
send_to_live(P_0, m3xw);
send_to_live(P_0, m3yz);
send_to_live(P_0, m3yw);
send_to_live(P_0, m3zw);
send_to_live(P_0, m3xyz);
send_to_live(P_0, m3xyw);
send_to_live(P_0, m3xzw);
send_to_live(P_0, m3yzw);
send_to_live(P_0, m3xyzw);
send_to_live(P_2, mxy);
send_to_live(P_2, mxz);
send_to_live(P_2, mxw);
send_to_live(P_2, myz);
send_to_live(P_2, myw);
send_to_live(P_2, mzw);
send_to_live(P_2, mxyz);
send_to_live(P_2, mxyw);
send_to_live(P_2, mxzw);
send_to_live(P_2, myzw);
send_to_live(P_2, mxyzw);
#endif
#else
send_to_live(P_0, m3xy);
send_to_live(P_0, m3xz);
send_to_live(P_0, m3xw);
send_to_live(P_0, m3yz);
send_to_live(P_0, m3yw);
send_to_live(P_0, m3zw);
send_to_live(P_0, m3xyz);
send_to_live(P_0, m3xyw);
send_to_live(P_0, m3xzw);
send_to_live(P_0, m3yzw);
send_to_live(P_0, m3xyzw);
store_compare_view(P_2, mxy);
store_compare_view(P_2, mxz);
store_compare_view(P_2, mxw);
store_compare_view(P_2, myz);
store_compare_view(P_2, myw);
store_compare_view(P_2, mzw);
store_compare_view(P_2, mxyz);
store_compare_view(P_2, mxzw);
store_compare_view(P_2, myzw);
store_compare_view(P_2, mxyw);
store_compare_view(P_2, mxyzw);
#endif
OEC_MAL3_Share e;
e.r0 = getRandomVal(P_123);
e.r1 = ADD(getRandomVal(P_013),getRandomVal(P_023));
return e;

}

template <typename func_add, typename func_sub>
void complete_mult4(func_add ADD, func_sub SUB){
}

#endif

};