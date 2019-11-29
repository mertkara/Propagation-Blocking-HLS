#include <hls_stream.h>
//#include <ap_int.h>
#include <iostream>
#include <string>
#include <sstream>
#include "types.h"

static void read_to_BRAM(LineT* inp, LineT* bramArray, int lineCnt) {
static int inputStreamStartingIndex = 0;
	mem_rd: for (int i=0; i < lineCnt; ++i) {
#pragma HLS pipeline II=1
		bramArray[i] = inp[inputStreamStartingIndex +i];
	}
	inputStreamStartingIndex += lineCnt;
}

static void read_BRAM_to_FIFOS(LineT* inp, hls::stream<ContribPair> inStream[8], int lineCnt) {

	mem_rd: for (int i=0; i < lineCnt; ++i) {
#pragma HLS pipeline II=1
		LineT line = inp[i];
		for(int j = 0; j < 8; j++)
#pragma HLS unroll factor=8 skip_exit_check
			inStream[j] << line.get(j) ;
	}
}
static void initializeSumArray(ValueT buffer[NUM_OF_PARTITIONS][BUCKET_WIDTH/NUM_OF_PARTITIONS]){
	for(int i = 0; i < NUM_OF_PARTITIONS; i++){
#pragma HLS unroll
		for( int j = 0; j < BUCKET_WIDTH/NUM_OF_PARTITIONS; j++){
#pragma HLS unroll factor 2
			buffer[i][j] = 0;
		}
	}
}

static void read_input_to_fifos(hls::stream<ContribPair>* inStream, hls::stream<ContribPair>* outStream,hls::stream<bool>* validStream, int lineCnt) {
	int i;
	ContribPair pairs[8];
	int remLines[8];
	int highestRemLineCount;
	int highestRemCntIndex;
	bool done = false;
#pragma HLS array_partition variable=pairs complete dim=1
#pragma HLS array_partition variable=remLines complete dim=1
int li;
	init_pairs_warm:for(li = 0; li < 8; li++){
#pragma HLS dependence variable=pairs,inStream intra false
#pragma HLS dependence variable=pairs,inStream inter false
#pragma HLS unroll
		pairs[li] = inStream[li].read();
		remLines[li] = 1;
	}
	while(!done){

/*
		if( (remLines[li%8] < lineCnt+1 )&& (pairs[li%8].indexData%NUM_OF_PARTITIONS == 0 )){
			validStream[0] << true;
			outStream[0] << pairs[li%8];
			if(remLines[li%8] < lineCnt) {
				pairs[li%8] = inStream[li%8].read();
				remLines[li%8]++;
			}else remLines[li%8]+=5;
		}
		if( (remLines[(li+2)%8] < lineCnt+1 )&& (pairs[(li+2)%8].indexData%NUM_OF_PARTITIONS == 1 )){
			validStream[1] << true;
			outStream[1] << pairs[(li+2)%8];
			if(remLines[(li+2)%8] < lineCnt) {
				pairs[(li+2)%8] = inStream[(li+2)%8].read();
				remLines[(li+2)%8]++;
			}else remLines[(li+2)%8]+=5;
		}
		if( (remLines[(li+4)%8] < lineCnt+1 )&& (pairs[(li+4)%8].indexData%NUM_OF_PARTITIONS == 2 )){
			validStream[2] << true;
			outStream[2] << pairs[(li+4)%8];
			if(remLines[(li+4)%8] < lineCnt) {
				pairs[(li+4)%8] = inStream[(li+4)%8].read();
				remLines[(li+4)%8]++;
			}else remLines[(li+4)%8]+=5;
		}
		if( (remLines[(li+6)%8] < lineCnt+1 )&& (pairs[(li+6)%8].indexData%NUM_OF_PARTITIONS == 3 )){
			validStream[3] << true;
			outStream[3] << pairs[(li+6)%8];
			if(remLines[(li+6)%8] < lineCnt) {
				pairs[(li+6)%8] = inStream[(li+6)%8].read();
				remLines[(li+6)%8]++;
			}else remLines[(li+6)%8]+=5;
		}
*/
//#pragma HLS pipeline
	stream_unroll: for(int streamId = 0; streamId < NUM_OF_PARTITIONS; streamId++){
#pragma HLS dependence variable=validStream,outStream,pairs,inStream inter false
#pragma HLS dependence variable=validStream,outStream,pairs,inStream intra false

#pragma HLS unroll
		for(int j = streamId*2; j < 8 + streamId*2; j++ ){

				if( (remLines[j%8] < lineCnt+1 )&& (pairs[j%8].indexData%NUM_OF_PARTITIONS == streamId )){
					validStream[streamId] << true;
					outStream[streamId] << pairs[j%8];
					if(remLines[j%8] < lineCnt) {
						pairs[j%8] = inStream[j%8].read();
					}
					remLines[j%8]++;
				}
			}
	}
	done = ((remLines[0] + remLines[1] +remLines[2]+remLines[3]+remLines[4]+remLines[5]+remLines[6] +remLines[7]) == (lineCnt*8+8));
	}

	finalize: for(int streamId = 0; streamId < NUM_OF_PARTITIONS; streamId++){
#pragma HLS unroll
			validStream[streamId] << false;
		}

}

static void read_output_to_BRAM(outLineT* inp, int inpBegin, ValueT buffer[NUM_OF_PARTITIONS][BUCKET_WIDTH/NUM_OF_PARTITIONS] ) {

	mem_rd_output: for (int i=0; i < BUCKET_WIDTH/16; ++i) {
#pragma HLS pipeline II=1
		outLineT line = inp[inpBegin+i];
		for(int j = 0; j < 16; j++)
			//#pragma HLS unroll factor=NUM_OF_PARTITIONS*2 skip_exit_check
			buffer[j%NUM_OF_PARTITIONS][(i*16 + j) /NUM_OF_PARTITIONS] = line.get(j) ;
	}
}
static void write_output_to_GMEM(ValueT inp[NUM_OF_PARTITIONS][BUCKET_WIDTH/NUM_OF_PARTITIONS], int inpBegin, outLineT buffer[BUCKET_WIDTH/16] ) {
	static int bucketID = 0;
	outLineT line;
	ValueT valToWrite;
	mem_wr_output: for (int i= 0; i < BUCKET_WIDTH; i++) {
#pragma HLS pipeline II=1

		//		for(int j = 0; j < 16; j++)
		//{
		//#pragma HLS unroll factor=16 skip_exit_check
		valToWrite = inp[i%NUM_OF_PARTITIONS][i/NUM_OF_PARTITIONS];
		line.set(i%16,valToWrite);
		//}
		//std::cout << "index1: " << i%NUM_OF_PARTITIONS <<", index2:"<< i/NUM_OF_PARTITIONS << ", val: " << valToWrite << std::endl;
		if( i%16 == 15) buffer[bucketID*BUCKET_WIDTH/16+i/16] = line ;
	}
	bucketID++;
	//std::cout<< "write: "<<bucketID << std::endl;

}
static int read_line_counts(outLineT *in, ValueT* array) {
	lineCount_rd:
	int sum = 0;
	for (int i = 0; i < NUM_OF_BUCKETS; i++) {
#pragma HLS PIPELINE II=1
		array[i] = in[i/16].get(i%16);
		//std::cout << "Sum: "<<sum ;
		sum += (in[i/16].get(i%16)/INPUT_ARRAY_SIZE );
		if(array[i]%32 != 0) sum++;
		//std::cout << "SumNext: "<<sum <<std::endl;
	}
	return sum;
}
static void compute(hls::stream<ContribPair> &inStream, hls::stream<bool>& validStream ,ValueT arr[BUCKET_WIDTH/NUM_OF_PARTITIONS] ){
#pragma HLS function_instantiate variable=inStream,validStream
	IndexT lastAddr = -1;
	ValueT lastVal = 0;
	ContribPair pair;
	IndexT index;
	ValueT val;
	ValueT currVal;
	bool valid;

	comp:for(valid = validStream.read(); valid != false; valid = validStream.read()){

#pragma HLS dependence variable=arr distance=2 inter true
#pragma HLS pipeline II=1

		//std::cout << "Valid: " << valid << ", i: " << i << std::endl;
		//ContribPair pair;

		pair = inStream.read() ; // read the current address
		index = pair.indexData / NUM_OF_PARTITIONS;     //0 4 8
		//std::cout << "INdex: " << index << ", i: " << pair.indexData << std::endl;
		val = pair.valData;			//1 5 9
		//2 6 10
		if (index == lastAddr) {	//3 7 11
			currVal = lastVal ; // pipeline forwarding
		}
		else {
			currVal = arr[index];
		}

		arr[index] = currVal + val; // update the current value and store it

		lastAddr = index ;
		lastVal = currVal + val ;
	}

}

static void compute_kernel(hls::stream<ContribPair>* inStream,hls::stream<bool>* validStream, ValueT arr[NUM_OF_PARTITIONS][BUCKET_WIDTH/NUM_OF_PARTITIONS]){
//#pragma HLS array_partition variable=arr complete dim=1

	comp_unroll_loop: for(int i = 0; i < 4; i++){
		//#pragma HLS array_partition variable=arr cyclic factor=4 dim=1

#pragma HLS dependence variable=arr inter false
#pragma HLS dependence variable=arr intra false
#pragma HLS unroll
		compute(inStream[i], validStream[i], arr[i]);
	}
}

static void read_and_compute(LineT* inp, ValueT arr[NUM_OF_PARTITIONS][BUCKET_WIDTH/NUM_OF_PARTITIONS],int lineCnt){
	hls::stream<ContribPair> inStream[8];
	hls::stream<ContribPair> outStream[4];
	hls::stream<bool> validStream[4];

#pragma HLS STREAM variable = inStream depth = 32
#pragma HLS STREAM variable = outStream depth = 32
#pragma HLS STREAM variable = validStream depth = 32

#pragma HLS array_partition variable=inStream complete
#pragma HLS array_partition variable=outStream complete
#pragma HLS array_partition variable=validStream complete
#pragma HLS array_partition variable=arr complete dim=1

#pragma HLS dataflow
	read_BRAM_to_FIFOS(inp, inStream, lineCnt);
	read_input_to_fifos(inStream, outStream, validStream,lineCnt);
	compute_kernel(outStream,validStream ,arr );
}
extern "C" {

void top_kernel(LineT* inputVals, LineT* indexVals, outLineT* outputSums, outLineT* lineCounts) {
#pragma HLS INTERFACE m_axi port=inputVals offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=lineCounts offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=indexVals offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=outputSums offset=slave bundle=gmem2
#pragma HLS INTERFACE s_axilite port=inputVals bundle=control
#pragma HLS INTERFACE s_axilite port=indexVals bundle=control
#pragma HLS INTERFACE s_axilite port=outputSums bundle=control

#pragma HLS INTERFACE s_axilite port=return bundle=control


	ValueT outputArray[NUM_OF_PARTITIONS][BUCKET_WIDTH/NUM_OF_PARTITIONS];
	ValueT outputArray2[NUM_OF_PARTITIONS][BUCKET_WIDTH/NUM_OF_PARTITIONS]; //manual cyclic partition and ping-pong buf
	ValueT lCounts[NUM_OF_BUCKETS];
	LineT inputLines[INPUT_ARRAY_SIZE];
	LineT inputLines2[INPUT_ARRAY_SIZE];


#pragma HLS array_partition variable=outputArray complete dim=1
#pragma HLS array_partition variable=outputArray2 complete dim=1
//#pragma HLS array_partition variable=outputArray complete dim=2

	ValueT inputStreamStartingIndex = 0;
	int totalCounts;
	totalCounts = read_line_counts(lineCounts, lCounts);

	bool shouldCarryOn = false;
	int remainingLines = lCounts[0];
	std::cout << "init"<<totalCounts << std::endl;
	initializeSumArray(outputArray);
	shouldCarryOn = (remainingLines > INPUT_ARRAY_SIZE);
	int readLines = shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines;
	read_to_BRAM(inputVals, inputLines, readLines);


	remainingLines = shouldCarryOn ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[1];

	int bucketID = 0;
	bucketID = shouldCarryOn ? bucketID:++bucketID;
	bool shouldCarryOn2;
	int i;
	for( i = 0; i < totalCounts-1 ; i++){
		if(i%2 == 0){
			shouldCarryOn2 = (remainingLines > INPUT_ARRAY_SIZE);
			read_to_BRAM(inputVals, inputLines2, shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines);
			read_and_compute(inputLines, outputArray, readLines);

			readLines = shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines;
			bucketID = shouldCarryOn2 ? bucketID:++bucketID;
			remainingLines = shouldCarryOn2 ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];


			if(!shouldCarryOn){
				write_output_to_GMEM(outputArray, i*BUCKET_WIDTH/16, outputSums);
				initializeSumArray(outputArray);
			}
		}else{
			shouldCarryOn = (remainingLines > INPUT_ARRAY_SIZE);
			read_to_BRAM(inputVals, inputLines, shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines);
			read_and_compute(inputLines2, outputArray, readLines);

			readLines = shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines;
			bucketID = shouldCarryOn ? bucketID:++bucketID;
			remainingLines = shouldCarryOn ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];
			//std::cout << "bucket: "<< bucketID << " rmml: "<< remainingLines << ", readLines: "<< readLines<< std::endl;
			if(!shouldCarryOn2){
				write_output_to_GMEM(outputArray, i*BUCKET_WIDTH/16, outputSums);
				initializeSumArray(outputArray);
			}
		}
	}
	//std::cout << i<<"bucket: "<< bucketID << " rmml: "<< remainingLines << ", readLines: "<< readLines<< std::endl;
	if(i%2==0)
		read_and_compute(inputLines, outputArray, readLines);
	else
		read_and_compute(inputLines2, outputArray, readLines);
	write_output_to_GMEM(outputArray, i*BUCKET_WIDTH/16, outputSums);

}
}
