#include <hls_stream.h>
#include <ap_int.h>
#include <iostream>
#include <string>
#include <sstream>

#define BURST_LINE_CNT 32
#define VECTOR_SIZE 16

using std::cout ;
using std::endl ;


typedef Type512 LineT ;
typedef ap_uint<32> IndexT ;
typedef ap_uint<32> ValueT ;
// The following class is to get bit counts of types in compile time
template <typename VT>
class BitCnt {

public:
  static short get() {
#pragma HLS INLINE
    return 64 ; // default -- not to be used
  }
} ;

template<>
class BitCnt<ap_uint<32> > {

public:
  static inline short get() {
#pragma HLS INLINE
    return 32 ;
  }
} ;

template<>
class BitCnt<ap_uint<16> > {

public:
  static inline short get() {
#pragma HLS INLINE
    return 16 ;
  }
} ;


struct Type512 {
  ap_uint<512> data ;
  ValueT get(int idx) {
    // HLS INLINE pragma is important. The tool does not recognize the C++ "inline" keyword.
#pragma HLS INLINE
    return data((idx+1)*BitCnt<ValueT>::get()-1,idx*BitCnt<ValueT>::get()) ;
  }

  void set(int idx, ValueT val) {
    // HLS INLINE pragma is important. The tool does not recognize the C++ "inline" keyword.
#pragma HLS INLINE
    data((idx+1)*BitCnt<ValueT>::get()-1, idx*BitCnt<ValueT>::get()) = val ;
  }

  string str() {
    std::ostringstream oss ;
    for (int vi=0; vi < VECTOR_SIZE; ++vi)
      oss << get(vi) << " " ;
    return oss.str() ;
  }

} ;
