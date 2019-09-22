#include <hls_stream.h>
//#include <ap_int.h>
#include <iostream>
#include <string>
#include <sstream>
#include "types.h"



const int ArrSize = 8 ;

static void read_to_BRAM(LineT* inp, int inpBegin, ContribPair buffer[ArrSize*8], int lineCnt) {

 mem_rd: for (int i=0; i < lineCnt; ++i) {
#pragma HLS pipeline II=1
	 LineT line = inp[inpBegin+i];
	 for(int j = 0; j < 8; j++)
	#pragma HLS unroll factor=2 skip_exit_check
		 buffer[i*8+j] = line.get(j) ;
  }
}
static void read_output_to_BRAM(outLineT* inp, int inpBegin, ValueT buffer[BUCKET_WIDTH] ) {

	 mem_rd_output: for (int i=0; i < BUCKET_WIDTH/16; ++i) {
	#pragma HLS pipeline II=1
		 outLineT line = inp[inpBegin+i];
		 for(int j = 0; j < 16; j++)
		#pragma HLS unroll factor=2 skip_exit_check
			 buffer[i*16+j] = line.get(j) ;
	  }
}
static void write_output_to_GMEM(ValueT* inp, int inpBegin, outLineT buffer[BUCKET_WIDTH] ) {

 mem_wr_output: for (int i=0; i < BUCKET_WIDTH; i = i + 16) {
#pragma HLS pipeline II=1
	 outLineT line;
	 for(int j = 0; j < 16; j++)
#pragma HLS unroll factor=16 skip_exit_check
		 line.set(j,inp[i+j]);
	buffer[inpBegin+i/16] = line ;
  }
}

static void compute_kernel(ContribPair* inputs, ValueT* arr, int lineCnt ){
	IndexT lastAddr = -1;
	ValueT lastVal = 0;
	ContribPair pair;
	IndexT index;
	ValueT val;
	ValueT currVal;
#pragma HLS inline off

	comp: for(int i = 0; i < lineCnt*8; i++){
#pragma HLS pipeline II=1
#pragma HLS dependence variable=arr distance=2 inter true

			pair = inputs[i] ; // read the current address
			index = pair.indexData;
			val = pair.valData;
			if (index == lastAddr) {
				currVal = lastVal ; // pipeline forwarding
			}
			else {
				currVal = arr[index % BUCKET_WIDTH];
			}

			arr[index % BUCKET_WIDTH] = currVal + val; // update the current value and store it

			lastAddr = index ;
			lastVal = currVal + val ;
		}
	}

extern "C" {

  void top_kernel(LineT* inputVals, LineT* indexVals, outLineT* outputSums, unsigned long blockCnt) {
#pragma HLS INTERFACE m_axi port=inputVals offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=indexVals offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=outputSums offset=slave bundle=gmem2
#pragma HLS INTERFACE s_axilite port=inputVals bundle=control
#pragma HLS INTERFACE s_axilite port=indexVals bundle=control
#pragma HLS INTERFACE s_axilite port=outputSums bundle=control

#pragma HLS INTERFACE s_axilite port=blockCnt bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

	ContribPair inputArray[ArrSize*8];
	ValueT outputArray[BUCKET_WIDTH];
//#pragma HLS array_partition variable=buffers complete dim=1
	for(int i = 0; i < NUM_OF_BUCKETS; i++){
		read_to_BRAM(inputVals, i*8, inputArray, 8);
		read_output_to_BRAM(outputSums, i*BUCKET_WIDTH/16, outputArray);
		compute_kernel(inputArray,outputArray,8);
		write_output_to_GMEM(outputArray, i*BUCKET_WIDTH/16, outputSums);
	}

  }
}
