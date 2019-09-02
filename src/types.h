#include <ap_int.h>
#include <iostream>
#include <string>
#include <vector>


using std::cout;
using std::vector;
using std::endl;
using std::string ;
//PARAM DEFINITIONS
#define NDDR_BANKS 3
#define OUTPUT_VECTOR_SIZE 1024*8 // 1 MB
#define BUCKET_WIDTH 64 //64 now for test purposes //512 // KB--> 1024*512/32 = 131072 values picked as best in the paper
#define MAX_BUCKET_INDEX_VAL_SIZE BUCKET_WIDTH*BUCKET_WIDTH // not likely since sparcity

#define NUM_OF_BUCKETS OUTPUT_VECTOR_SIZE/BUCKET_WIDTH // assume divisible for now

//bucket densities for testing
#define EMPTY_BUCKET 0
#define LOW_DENSE_BUCKET 1
#define MEDIUM_DENSE_BUCKET 2
#define FULL_BUCKET 3
#define NUM_OF_TEST_SIZE 2048

#define BURST_LINE_CNT 32
#define VECTOR_SIZE 8
typedef ap_uint<32> IndexT ;
typedef ap_uint<32> ValueT ;

typedef struct ContribPair {
	IndexT indexData;
	ValueT valData;
} ContribPair;


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
class BitCnt<ContribPair> {

public:
  static inline short get() {
#pragma HLS INLINE
    return 64 ;
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
	ap_uint<512> data;
  ContribPair get(int idx) {
    // HLS INLINE pragma is important. The tool does not recognize the C++ "inline" keyword.
#pragma HLS INLINE
	  ContribPair res;
	  ap_uint<64> pair = data((idx+1)*64-1,idx*64);
	  res.indexData = pair(31,0);
	  res.valData = pair(63,32);
	  return res;
  }

  void set(int idx, ContribPair val) {
    // HLS INLINE pragma is important. The tool does not recognize the C++ "inline" keyword.
#pragma HLS INLINE
	  ap_uint<64> pair;
	  pair(31,0) = val.indexData ;
	  pair(63,32) = val.valData ;
	  data((idx+1)*64-1,idx*64) = pair;

  }

/*  string str() {
    std::ostringstream oss ;
    for (int vi=0; vi < VECTOR_SIZE; ++vi)
      oss << get(vi) << " " ;
    return oss.str() ;
  }*/

} ;

//TYPE DEFINITIONS AND STRUCTS
typedef Type512 LineT ;




/*
struct Type512_output {
  ap_uint<512> data ;
  ValueT get(int idx) {
    // HLS INLINE pragma is important. The tool does not recognize the C++ "inline" keyword.
#pragma HLS INLINE
    return data((idx+1)*BitCnt<ContribPair>::get()-1,idx*BitCnt<ContribPair>::get()) ;
  }

  void set(int idx, ContribPair val) {
    // HLS INLINE pragma is important. The tool does not recognize the C++ "inline" keyword.
#pragma HLS INLINE
    data((idx+1)*BitCnt<ContribPair>::get()-1, idx*BitCnt<ContribPair>::get()) = val ;
  }

  string str() {
    std::ostringstream oss ;
    for (int vi=0; vi < VECTOR_SIZE; ++vi)
      oss << get(vi) << " " ;
    return oss.str() ;
  }

} ;
*/
