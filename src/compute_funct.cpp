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

	ContribPair pairs[8];
	int remLines[8];

	bool done = false;
#pragma HLS array_partition variable=pairs complete
#pragma HLS array_partition variable=remLines complete

	init_pairs_warm:for(int li = 0; li < 8; li++){
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
//#pragma HLS pipeline enable_flush
	stream_unroll: for(int streamId = 0; streamId < NUM_OF_PARTITIONS; streamId++){
#pragma HLS dependence variable=validStream,outStream,pairs,inStream inter false
#pragma HLS dependence variable=validStream,outStream,pairs,inStream intra false
//#pragma HLS loop_flatten

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

static void divideInputsOnBits(hls::stream<ContribPair>* inStream,hls::stream<ContribPair> midStream[8][NUM_OF_PARTITIONS], hls::stream<bool> midValidStream[8][NUM_OF_PARTITIONS],int lineCnt){
	write_pairs:for(int i = 0; i < lineCnt; i++){
#pragma HLS dependence variable=midStream,midValidStream,inStream intra false
#pragma HLS dependence variable=midStream,midValidStream,inStream inter false
		divide_unroll:for(int li = 0; li < 8; li++){
#pragma HLS dependence variable=midStream,midValidStream,inStream intra false
#pragma HLS dependence variable=midStream,midValidStream,inStream inter false
#pragma HLS unroll
				ContribPair pairToWrite = inStream[li].read();
				midStream[li][ pairToWrite.indexData%NUM_OF_PARTITIONS ] << pairToWrite;
				midValidStream[li][ pairToWrite.indexData%NUM_OF_PARTITIONS ] << true;
		}
	}
	finalize: for(int i = 0; i < NUM_OF_PARTITIONS; i++){
#pragma HLS unroll
		for(int li = 0; li < 8; li++){
#pragma HLS loop_flatten
		midValidStream[li][ i ] << false;
		}
	}

}

static void sendPairsToPartitions(hls::stream<ContribPair> midStream[8][NUM_OF_PARTITIONS], hls::stream<bool> midValidStream[8][NUM_OF_PARTITIONS],hls::stream<bool>* validStream,hls::stream<ContribPair>* outStream,int lineCnt){
#pragma HLS array_partition variable=midStream complete
#pragma HLS array_partition variable=midValidStream complete
#pragma HLS array_partition variable=validStream complete
#pragma HLS array_partition variable=outStream complete

int doneCount[NUM_OF_PARTITIONS] = {0};
#pragma HLS array_partition variable=doneCount complete

	stream_unroll: while(!((doneCount[0] + doneCount[1]+doneCount[2] + doneCount[3]) == (NUM_OF_PARTITIONS*9))){
//#pragma HLS loop_merge
#pragma HLS dependence variable=midStream,midValidStream,validStream,outStream intra false
#pragma HLS dependence variable=midStream,midValidStream,validStream,outStream,doneCount inter false



		//To allow unrolled region to run in parallel, it is crucial for it to be just consisting of direct instructions/functions without loops, if the unrolled region has loop, it runs it sequentially.
		for(int streamId = 0; streamId < NUM_OF_PARTITIONS; streamId++){
//#pragma HLS pipeline
#pragma HLS unroll
//#pragma HLS dependence variable=midStream,midValidStream,validStream,outStream,doneCount intra false
#pragma HLS dependence variable=midStream,midValidStream,validStream,outStream,doneCount inter false

			if(!midValidStream[0][streamId].empty()){
				if(midValidStream[0][streamId].read() == true){
					outStream[streamId] << midStream[0][streamId].read();
					validStream[streamId] << true;}else doneCount[streamId]++;
			}else if(!midValidStream[1][streamId].empty()){
				if(midValidStream[1][streamId].read() == true){
					outStream[streamId] << midStream[1][streamId].read();
					validStream[streamId] << true;}else doneCount[streamId]++;

			}else if(!midValidStream[2][streamId].empty()){
				if(midValidStream[2][streamId].read() == true){
					outStream[streamId] << midStream[2][streamId].read();
					validStream[streamId] << true;}else doneCount[streamId]++;

			}else if(!midValidStream[3][streamId].empty()){
				if(midValidStream[3][streamId].read() == true){
					outStream[streamId] << midStream[3][streamId].read();
					validStream[streamId] << true;}else doneCount[streamId]++;

			}else if(!midValidStream[4][streamId].empty()){
				if(midValidStream[4][streamId].read() == true){
					outStream[streamId] << midStream[4][streamId].read();
					validStream[streamId] << true;}else doneCount[streamId]++;

			}else if(!midValidStream[5][streamId].empty()){
				if(midValidStream[5][streamId].read() == true){
					outStream[streamId] << midStream[5][streamId].read();
					validStream[streamId] << true;}else doneCount[streamId]++;

			}else if(!midValidStream[6][streamId].empty()){
				if(midValidStream[6][streamId].read() == true){
					outStream[streamId] << midStream[6][streamId].read();
					validStream[streamId] << true;}
				else doneCount[streamId]++;
			}else if(!midValidStream[7][streamId].empty()){
				if(midValidStream[7][streamId].read() == true){
					outStream[streamId] << midStream[7][streamId].read();
					validStream[streamId] << true;
				}else doneCount[streamId]++;
			}else if(doneCount[streamId] == 8){
				validStream[streamId] << false;
				doneCount[streamId]++;
				//exitLoop = true;
			}

		}
	}

}

static void read_input_to_fifos2(hls::stream<ContribPair>* inStream, hls::stream<ContribPair>* outStream,hls::stream<bool>* validStream, int lineCnt) {

	hls::stream<ContribPair> midStream[8][NUM_OF_PARTITIONS];
	hls::stream<bool> midValidStream[8][NUM_OF_PARTITIONS];

#pragma HLS array_partition variable=midStream complete dim=0
#pragma HLS array_partition variable=midValidStream complete dim=0
#pragma HLS STREAM variable = midStream depth = 8
#pragma HLS STREAM variable = midValidStream depth = 8


#pragma HLS dataflow
	divideInputsOnBits(inStream, midStream, midValidStream, lineCnt);
	sendPairsToPartitions(midStream, midValidStream, validStream,outStream,lineCnt);



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
	outLineT outBuffer[BUCKET_WIDTH/16];
//#pragma HLS array_map variable=inp instance=inpComb vertical
//#pragma HLS interface port=inp bram

//#pragma HLS dataflow

	generate_for_burst: for (int i= 0; i < BUCKET_WIDTH; i++){
#pragma HLS dependence variable=inp inter false
#pragma HLS pipeline II=1
		valToWrite = inp[i%NUM_OF_PARTITIONS][i/NUM_OF_PARTITIONS];
		line.set(i%16,valToWrite);
		inp[i%NUM_OF_PARTITIONS][i/NUM_OF_PARTITIONS] = 0;
		if( i%16 == 15) {
#pragma HLS occurrence cycle=16
			outBuffer[i/16] = line;
		}
	}

	mem_wr_output: for (int i= 0; i < BUCKET_WIDTH/16; i++) {
#pragma HLS pipeline II=1
		buffer[bucketID*BUCKET_WIDTH/16+i] = outBuffer[i];
	}
	bucketID++;
	//std::cout<< "write Bucket: "<<bucketID << std::endl;

}
static int read_line_counts(outLineT *in, ValueT* array) {
	lineCount_rd:
	int sum = 0;
	for (int i = 0; i < NUM_OF_BUCKETS; i++) {
#pragma HLS PIPELINE II=1
		array[i] = in[i/16].get(i%16);
		//std::cout << "Sum: "<<sum ;
		sum += (in[i/16].get(i%16)/INPUT_ARRAY_SIZE );
		if(array[i]%INPUT_ARRAY_SIZE != 0) sum++;
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
	read_input_to_fifos2(inStream, outStream, validStream,lineCnt);
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
	ValueT outputArrSec[NUM_OF_PARTITIONS][BUCKET_WIDTH/NUM_OF_PARTITIONS]; //manual cyclic partition and ping-pong buf
	ValueT lCounts[NUM_OF_BUCKETS + 1]; //+1 for safety in the pipeline, dummy var
	LineT inputLines[INPUT_ARRAY_SIZE];
	LineT inputLinesSec[INPUT_ARRAY_SIZE];

//#pragma HLS array_partition variable=inputLines complete dim=1
#pragma HLS array_partition variable=outputArray complete dim=1
#pragma HLS array_partition variable=outputArrSec complete dim=1
//#pragma HLS array_partition variable=outputArray complete dim=2

	ValueT inputStreamStartingIndex = 0;
	int totalCounts;
	totalCounts = read_line_counts(lineCounts, lCounts);

	ap_uint<1> outputBucketID = 0;
	int bucketID = 0;
	bool shouldCarryOn = false;
	int readLines;
	int remainingLines = lCounts[0];
	bool shouldCarryOn2;
	int i;

	std::cout << "init "<< totalCounts << std::endl;

	initializeSumArray(outputArray);
	initializeSumArray(outputArrSec);

	shouldCarryOn = (remainingLines > INPUT_ARRAY_SIZE);
	readLines = shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines;
	read_to_BRAM(inputVals, inputLines, readLines);
	bucketID = shouldCarryOn ? bucketID:++bucketID;
	remainingLines = shouldCarryOn ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];

	shouldCarryOn2 = (remainingLines > INPUT_ARRAY_SIZE);
	read_to_BRAM(inputVals, inputLinesSec, shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines);
	read_and_compute(inputLines, outputArray, readLines);

	readLines = shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines;
	bucketID = shouldCarryOn2 ? bucketID:++bucketID;
	remainingLines = shouldCarryOn2 ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];




	for( i = 0; i < totalCounts-2 ; i++){
//#pragma HLS dependence variable=inputLines inter false

		if(i%2 == 0){
			if(!shouldCarryOn){
				if(outputBucketID == 0){
					shouldCarryOn = (remainingLines > INPUT_ARRAY_SIZE);
					read_and_compute(inputLinesSec, outputArrSec, readLines);
					read_to_BRAM(inputVals, inputLines, shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines);
					write_output_to_GMEM(outputArray, i*BUCKET_WIDTH/16, outputSums);
				//	initializeSumArray(outputArray); no need since write_output_to gmem already does this now!
					outputBucketID++;
					readLines = shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines;
					bucketID = shouldCarryOn ? bucketID:++bucketID;
					remainingLines = shouldCarryOn ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];
				}else{
					shouldCarryOn = (remainingLines > INPUT_ARRAY_SIZE);
					read_and_compute(inputLinesSec, outputArray, readLines);
					read_to_BRAM(inputVals, inputLines, shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines);
					write_output_to_GMEM(outputArrSec, i*BUCKET_WIDTH/16, outputSums);
//					initializeSumArray(outputArrSec);
					outputBucketID--;
					readLines = shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines;
					bucketID = shouldCarryOn ? bucketID:++bucketID;
					remainingLines = shouldCarryOn ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];
				}

			}else{
				if(outputBucketID == 0){
					shouldCarryOn = (remainingLines > INPUT_ARRAY_SIZE);
					read_and_compute(inputLinesSec, outputArray, readLines);
					read_to_BRAM(inputVals, inputLines, shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines);
					readLines = shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines;
					bucketID = shouldCarryOn ? bucketID:++bucketID;
					remainingLines = shouldCarryOn ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];
				}else{
					shouldCarryOn = (remainingLines > INPUT_ARRAY_SIZE);
					read_and_compute(inputLinesSec, outputArrSec, readLines);
					read_to_BRAM(inputVals, inputLines, shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines);
					readLines = shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines;
					bucketID = shouldCarryOn ? bucketID:++bucketID;
					remainingLines = shouldCarryOn ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];
				}
			}
		}else{
			if(!shouldCarryOn2){
				if(outputBucketID == 0){
					shouldCarryOn2 = (remainingLines > INPUT_ARRAY_SIZE);
					read_and_compute(inputLines, outputArrSec, readLines);
					read_to_BRAM(inputVals, inputLinesSec, shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines);
					write_output_to_GMEM(outputArray, i*BUCKET_WIDTH/16, outputSums);
//					initializeSumArray(outputArray);
					outputBucketID++;
					readLines = shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines;
					bucketID = shouldCarryOn2 ? bucketID:++bucketID;
					remainingLines = shouldCarryOn2 ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];

				}else{
					shouldCarryOn2 = (remainingLines > INPUT_ARRAY_SIZE);
					read_and_compute(inputLines, outputArray, readLines);
					read_to_BRAM(inputVals, inputLinesSec, shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines);
					write_output_to_GMEM(outputArrSec, i*BUCKET_WIDTH/16, outputSums);
//					initializeSumArray(outputArrSec);
					outputBucketID--;
					readLines = shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines;
					bucketID = shouldCarryOn2 ? bucketID:++bucketID;
					remainingLines = shouldCarryOn2 ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];
				}
			}else{
				if(outputBucketID == 0){
					shouldCarryOn2 = (remainingLines > INPUT_ARRAY_SIZE);
					read_and_compute(inputLines, outputArray, readLines);
					read_to_BRAM(inputVals, inputLinesSec, shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines);

					readLines = shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines;
					bucketID = shouldCarryOn2 ? bucketID:++bucketID;
					remainingLines = shouldCarryOn2 ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];

				}else{
					shouldCarryOn2 = (remainingLines > INPUT_ARRAY_SIZE);
					read_and_compute(inputLines, outputArrSec, readLines);
					read_to_BRAM(inputVals, inputLinesSec, shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines);
					readLines = shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines;
					bucketID = shouldCarryOn2 ? bucketID:++bucketID;
					remainingLines = shouldCarryOn2 ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];
				}
			}
		}
	}
	//std::cout << i<<"bucket: "<< bucketID << " rmml: "<< remainingLines << ", readLines: "<< readLines<< std::endl;
	if(i%2 == 0){
		if(!shouldCarryOn){
			if(outputBucketID == 0){
				std::cout << "1" << std::endl;
				read_and_compute(inputLinesSec, outputArrSec, readLines);
				write_output_to_GMEM(outputArray, i*BUCKET_WIDTH/16, outputSums);
	//			initializeSumArray(outputArray);
				outputBucketID++;
			}else{
				std::cout << "2" << std::endl;
				read_and_compute(inputLinesSec, outputArray, readLines);
				write_output_to_GMEM(outputArrSec, i*BUCKET_WIDTH/16, outputSums);
		//		initializeSumArray(outputArrSec);
				outputBucketID--;
			}
		}else{
			std::cout << 3+outputBucketID << std::endl;
			if(outputBucketID == 0)
				read_and_compute(inputLinesSec, outputArray, readLines);
			else
				read_and_compute(inputLinesSec, outputArrSec, readLines);
		}
		i++;
	}else{
		if(!shouldCarryOn2){
			std::cout << 5+outputBucketID << std::endl;
			if(outputBucketID == 0){
				read_and_compute(inputLines, outputArrSec, readLines);
				write_output_to_GMEM(outputArray, i*BUCKET_WIDTH/16, outputSums);
			//	initializeSumArray(outputArray);
				outputBucketID++;
			}else{
				read_and_compute(inputLines, outputArray, readLines);
				write_output_to_GMEM(outputArrSec, i*BUCKET_WIDTH/16, outputSums);
				//initializeSumArray(outputArrSec);
				outputBucketID--;
			}
		}else{
			std::cout << 7+outputBucketID << std::endl;
			if(outputBucketID == 0)
				read_and_compute(inputLines, outputArray, readLines);
			else
				read_and_compute(inputLines, outputArrSec, readLines);
		}
		i++;
	}
	if(outputBucketID == 0){
		std::cout << "9" << std::endl;
		write_output_to_GMEM(outputArray, i*BUCKET_WIDTH/16, outputSums);
	}else{
		std::cout << "10" << std::endl;
		write_output_to_GMEM(outputArrSec, i*BUCKET_WIDTH/16, outputSums);
	}

}
}
