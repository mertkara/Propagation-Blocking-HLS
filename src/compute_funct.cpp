//#include <ap_int.h>
#include <iostream>
#include <string>
#include <sstream>
//#include "types.h"
#include <assert.h>
#include "rcw_pipeline.cpp"

/*
static void read_to_BRAM(LineT* inp, LineT* bramArray, int lineCnt) {
	static int inputStreamStartingIndex = 0;
	mem_rd: for (int i=0; i < lineCnt; ++i) {
#pragma HLS pipeline II=1
		bramArray[i] = inp[inputStreamStartingIndex +i];
	}
	inputStreamStartingIndex += lineCnt;
}

static void read_BRAM_to_FIFOS(LineT* inp, hls::stream<ContribPair> inStream[16], hls::stream<bool> inValStream[16], int lineCnt) {
	LineT resLine;
	resLine.data = 0;
	ContribPair p;
	p.valData = 0;
	p.indexData = 0;

	warm_up: 	for(int j = 0; j < 16; j++){
#pragma HLS unroll factor=16
		if(j > 0 && j%4 == 0)
			p.indexData = p.indexData++;
		inValStream[j] << true;
		inStream[j] << p;
		inValStream[j] << true;
		inStream[j] << p;
		p.indexData = p.indexData + 4;
	}


	mem_rd: for (int i=0; i < lineCnt; i = i + 2) {
#pragma HLS pipeline II=1
		LineT line1 = inp[i];
		LineT line2 = ((i+1) < lineCnt) ? inp[i+1] : resLine ;
		for(int j = 0; j < 8; j++){
#pragma HLS unroll factor=8 skip_exit_check
			inStream[j] << line1.get(j);
			inValStream[j] << true;
		}
		for(int j = 0; j < 8; j++){
#pragma HLS unroll factor=8 skip_exit_check
			inStream[8 + j] << line2.get(j);
			inValStream[8 + j] << true;
		}
	}
	for(int j = 0; j < 16; j++){
#pragma HLS unroll factor=16 skip_exit_check
		inValStream[j] << false;
		inStream[j] << p;
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

static void sendPackets(int bitLevel, hls::stream<ContribPair>* inStream,hls::stream<bool>* inValStream,hls::stream<ContribPair> midStream[4][4], hls::stream<bool> midValidStream[4][4]){

	//ap_uint<4> signaltoExitLoop = 0;
	bool signaltoExitLoop[4];

	ContribPair pairsRead[4];
	bool validTokensRead[4];
	bool successWriteContrib[4];
	bool successWriteValid[4];

#pragma HLS array_partition variable=validTokensRead complete
#pragma HLS array_partition variable=successWriteContrib complete
#pragma HLS array_partition variable=pairsRead complete
#pragma HLS array_partition variable=successWriteValid complete
#pragma HLS array_partition variable=signaltoExitLoop complete
	//	ContribPair dummy;
	//dummy.indexData = 0;
	//dummy.valData = 0;

	initVariables: for(int i = 0; i < 4; i++){
#pragma HLS unroll
		signaltoExitLoop[i] = false;
		successWriteContrib[i] = true;
		successWriteValid[i] = true;
	}

//	std::cout << "Came so Far! 111"<< std::endl;

	write_pairs:while(!signaltoExitLoop[0] || !signaltoExitLoop[1] || !signaltoExitLoop[2] || !signaltoExitLoop[3]){
		//#pragma HLS dependence variable=midStream,midValidStream,inStream,inValStream intra false
		//#pragma HLS dependence variable=midStream,midValidStream,inStream,inValStream inter false

		divide_unroll:for(int li = 0; li < 4; li++){
			//#pragma HLS dependence variable=midStream,midValidStream,inStream,inValStream intra false
#pragma HLS dependence variable=midStream,midValidStream,inStream,inValStream,signaltoExitLoop,successWriteValid,successWriteContrib inter false
#pragma HLS unroll
			if(!successWriteContrib[li] || !successWriteValid[li]){
				if(!successWriteContrib[li])
					successWriteContrib[li] = midStream[li][ pairsRead[li].indexData.range((bitLevel+1)*2 - 1,(bitLevel)*2) ].write_nb(pairsRead[li]);
				if(!successWriteValid[li])
					successWriteValid[li] = midValidStream[li][ pairsRead[li].indexData.range((bitLevel+1)*2 - 1,(bitLevel)*2) ].write_nb(validTokensRead[li]);
			}else{
				if(!inValStream[li].empty() && !inStream[li].empty()){
					inStream[li].read_nb(pairsRead[li]);
					inValStream[li].read_nb(validTokensRead[li]);
					if(validTokensRead[li]){
						successWriteContrib[li] = midStream[li][ pairsRead[li].indexData.range((bitLevel+1)*2 - 1,(bitLevel)*2) ].write_nb(pairsRead[li]);
						successWriteValid[li] = midValidStream[li][ pairsRead[li].indexData.range((bitLevel+1)*2 - 1,(bitLevel)*2) ].write_nb(validTokensRead[li]);
					}else{
						signaltoExitLoop[li] = true;
					}
				}
			}
		}
	}
		if(!signaltoExitLoop[li] && !inStream[li].empty() && !inValStream[li].empty()){
				ContribPair pairToWrite;
				assert(inStream[li].read_nb(pairToWrite));
				bool valid;
				assert(inValStream[li].read_nb(valid));
					success = midStream[li][ pairToWrite.indexData.range((bitLevel+1)*2 - 1,(bitLevel)*2) ].write_nb(pairToWrite);
				}while(!success);
				do{
					success = midValidStream[li][ pairToWrite.indexData.range((bitLevel+1)*2 - 1,(bitLevel)*2) ].write_nb(valid);
				}while(!success);
				//}else{
					if(!valid){
					signaltoExitLoop[li] = true;
					for(int i = 1; i < 4; i++){
#pragma HLS dependence variable=midValidStream inter false
						#pragma HLS unroll
						bool success;
						do{success = midValidStream[li][ i ].write_nb(false);}while(!success);
						do{success = midStream[li][ i ].write_nb(pairToWrite);}while(!success);
					}
				}
			}
		}
	}


	finalize: for(int i = 0; i < 4; i++){
#pragma HLS dependence variable=midValidStream inter false
#pragma HLS pipeline
		for(int li = 0; li < 4; li++){
#pragma HLS unroll
#pragma HLS dependence variable=midValidStream inter false
			midStream[li][i] << pairsRead[li];
			midValidStream[li][ i ] << false;
		}


	}
}

static void recievePackets(hls::stream<ContribPair> midStream[4][4], hls::stream<bool> midValidStream[4][4],hls::stream<bool>* validStream,hls::stream<ContribPair>* outStream){
	//#pragma HLS interface ap_ctrl_none port=return

	//	ap_uint<4> signaltoExitLoop = 0;
	ContribPair dummy;
	dummy.indexData = 0;
	dummy.valData = 0;
	bool signaltoExitLoop[4];
	int doneCount[4];
	ap_uint<2> scheduler = 0;
//#pragma HLS array_partition variable=scheduler complete
#pragma HLS array_partition variable=doneCount complete
#pragma HLS array_partition variable=signaltoExitLoop complete

	initVariables: for(int i = 0; i < 4; i++){
#pragma HLS unroll
		doneCount[i] = 0;
		signaltoExitLoop[i] = false;
	}

	stream_unroll: while(!signaltoExitLoop[0] || !signaltoExitLoop[1] || !signaltoExitLoop[2] || !signaltoExitLoop[3]){
		//#pragma HLS loop_merge
		//#pragma HLS dependence variable=midStream,midValidStream,validStream,outStream,doneCount intra false
		//	#pragma HLS dependence variable=midStream,midValidStream,validStream,outStream false//,doneCount,signaltoExitLoop inter false
		//#pragma HLS dependence variable=doneCount inter true

#pragma HLS pipeline
		//To allow unrolled region to run in parallel, it is crucial for it to be just consisting of direct instructions/functions without loops, if the unrolled region has loop, it runs it sequentially.
		for(int streamId = 0; streamId < 4; streamId++){
#pragma HLS dependence variable=midStream,midValidStream,validStream,outStream,doneCount intra false
#pragma HLS dependence variable=midStream,midValidStream,validStream,outStream,doneCount,scheduler inter false
#pragma HLS unroll

			for(int i = 0; i < 4; i++){
//#pragma HLS loop_flatten
				if(!midValidStream[(scheduler + ap_uint<2>(i))%4][streamId].empty()){
					bool val;
					assert(midValidStream[(scheduler + ap_uint<2>(i))%4][streamId].read_nb(val));
					ContribPair p ;
					assert(midStream[(scheduler + ap_uint<2>(i))%4][streamId].read_nb(p));
					if(!val)
						doneCount[streamId]++;
					else{
						assert(outStream[streamId].write_nb(p));
						assert(validStream[streamId].write_nb(true));
						scheduler++;
					}
					break;
				}else if(doneCount[streamId] == 4){
					doneCount[streamId]++;
					signaltoExitLoop[streamId] = true;
					//signaltoExitLoop.set(streamId);
					outStream[streamId].write_nb(dummy);
					validStream[streamId].write_nb(false);
				}
			}

		if(!midValidStream[(scheduler + ap_uint<2>(0))%4][streamId].empty()){
			bool val;
			assert(midValidStream[(scheduler + ap_uint<2>(0))%4][streamId].read_nb(val));
			ContribPair p ;
			assert(midStream[(scheduler + ap_uint<2>(0))%4][streamId].read_nb(p));
			if(!val)
				doneCount[streamId]++;
			else{
				assert(outStream[streamId].write_nb(p));
				assert(validStream[streamId].write_nb(true));
			}
		}
		else if(!midValidStream[(scheduler + ap_uint<2>(1))%4][streamId].empty()){
			bool val;
			assert(midValidStream[(scheduler + ap_uint<2>(1))%4][streamId].read_nb(val));
			ContribPair p ;
			assert(midStream[(scheduler + ap_uint<2>(1))%4][streamId].read_nb(p));
			if(!val)
				doneCount[streamId]++;
			else{
				assert(outStream[streamId].write_nb(p));
				assert(validStream[streamId].write_nb(true));
			}
		}
		else if(!midValidStream[(scheduler + ap_uint<2>(2))%4][streamId].empty()){
			bool val;
			assert(midValidStream[(scheduler + ap_uint<2>(2))%4][streamId].read_nb(val));
			ContribPair p ;
			assert(midStream[(scheduler + ap_uint<2>(2))%4][streamId].read_nb(p));
			if(!val)
				doneCount[streamId]++;
			else{
				assert(outStream[streamId].write_nb(p));
				assert(validStream[streamId].write_nb(true));
			}
		}
		else if(!midValidStream[(scheduler + ap_uint<2>(3))%4][streamId].empty()){
			bool val;
			assert(midValidStream[(scheduler + ap_uint<2>(3))%4][streamId].read_nb(val));
			ContribPair p ;
			assert(midStream[(scheduler + ap_uint<2>(3))%4][streamId].read_nb(p));
			if(!val)
				doneCount[streamId]++;
			else{
				assert(outStream[streamId].write_nb(p));
				assert(validStream[streamId].write_nb(true));
			}
		}
		else if(doneCount[streamId] == 4){
			doneCount[streamId]++;
			signaltoExitLoop[streamId] = true;
			//signaltoExitLoop.set(streamId);
			outStream[streamId].write_nb(dummy);
			validStream[streamId].write_nb(false);
		}

		}
			scheduler++; //causes problems idk
	}
		finalize_valid_signals: for(int i = 0; i < 4; i++){
#pragma HLS dependence variable=validStream inter false
#pragma HLS unroll
		validStream[i].write(false);
	}

}
void transposeSingleChannel(hls::stream<ContribPair> &midStream, hls::stream<bool> &midValidStream,hls::stream<bool> &validStream,hls::stream<ContribPair> &outStream){
	//#pragma HLS inline
#pragma HLS function_instantiate variable=inStream,validStream
	//#pragma HLS dataflow
	bool done = false;
	while(!done){
		//#pragma HLS pipeline
		if(!midValidStream.empty()){
			if(midValidStream.read() == true)
			{
				outStream.write_nb(midStream.read());
				validStream.write_nb(true);
			}else{
				done = true;
				validStream.write_nb(false);
				outStream.write_nb(midStream.read());
			}
		}
	}
}

static void transposeChannels(hls::stream<ContribPair> midStream[4][4], hls::stream<bool> midValidStream[4][4],hls::stream<bool> validStream[4][4],hls::stream<ContribPair> outStream[4][4]){

	for(int i = 0; i < 4 ; i++){
#pragma HLS dependence variable=midStream,midValidStream,validStream,outStream intra false
#pragma HLS dependence variable=midStream,midValidStream,validStream,outStream inter false
#pragma HLS unroll
		for(int j = 0; j < 4 ; j++){
#pragma HLS dependence variable=midStream,midValidStream,validStream,outStream intra false
#pragma HLS dependence variable=midStream,midValidStream,validStream,outStream inter false
#pragma HLS unroll
			transposeSingleChannel(midStream[i][j],midValidStream[i][j],validStream[j][i], outStream[j][i]);
		}
	}
}

static void multiReadChannelPartition(int bitLevel,hls::stream<ContribPair>* inStream, hls::stream<bool>* inValStream,hls::stream<ContribPair>* outStream,hls::stream<bool>* validStream ) {
	//#pragma HLS interface ap_ctrl_none port=return

	hls::stream<ContribPair> midStream[4][4];
	hls::stream<bool> midValidStream[4][4];

#pragma HLS array_partition variable=midStream complete dim=0
#pragma HLS array_partition variable=midValidStream complete dim=0
#pragma HLS STREAM variable = midStream depth = 5
#pragma HLS STREAM variable = midValidStream depth = 5


#pragma HLS dataflow
	sendPackets(bitLevel,inStream,inValStream, midStream, midValidStream);
	recievePackets(midStream, midValidStream, validStream,outStream);

}

static void multiRead16( hls::stream<ContribPair>* inStream, hls::stream<bool>* inValStream,hls::stream<ContribPair>* outStream,hls::stream<bool>* validStream) {
	hls::stream<ContribPair> midStream[4][4];
	hls::stream<bool> midValidStream[4][4];

	hls::stream<ContribPair> midTranspose[4][4];
	hls::stream<bool> midValidTranspose[4][4];

#pragma HLS STREAM variable = midStream depth = 5
#pragma HLS STREAM variable = midValidStream depth = 5
#pragma HLS STREAM variable = midTranspose depth = 5
#pragma HLS STREAM variable = midValidTranspose depth = 5

#pragma HLS array_partition variable=midStream complete
#pragma HLS array_partition variable=midValidStream complete
#pragma HLS array_partition variable=midTranspose complete
#pragma HLS array_partition variable=midValidTranspose complete

	// Arrays of Pointers to access the second dimensions of streams, transpose
	//	hls::stream<ContribPair> midStreamDim2[4][4];
	//	hls::stream<bool> midValidStreamDim2[4][4];
	//#pragma HLS array_partition variable=midStreamDim2 complete
	//#pragma HLS array_partition variable=midValidStreamDim2 complete
	//	for( int i = 0; i < 4; i++){
	//		for( int j = 0; j < 4; j++){
	//			midStreamDim2[i][j] = &(midStream[j][i]);
	//			midValidStreamDim2[i][j] = &(midValidStream[j][i]);
	//		}
	//	}

	//#pragma HLS dependence

#pragma HLS dataflow

	//#pragma HLS interface ap_ctrl_none port=return
	multiReadChannelPartition(1,&inStream[0],  &inValStream[0],  midStream[0],  midValidStream[0] );
	multiReadChannelPartition(1,&inStream[4],  &inValStream[4],  midStream[1],  midValidStream[1] );
	multiReadChannelPartition(1,&inStream[8],  &inValStream[8],  midStream[2],  midValidStream[2] );
	multiReadChannelPartition(1,&inStream[12], &inValStream[12], midStream[3],  midValidStream[3] );
	transposeChannels(midStream, midValidStream,midValidTranspose,midTranspose);
	multiReadChannelPartition(0, midTranspose[0], midValidTranspose[0], &outStream[0]  ,&validStream[0] );
	multiReadChannelPartition(0, midTranspose[1], midValidTranspose[1], &outStream[4]  ,&validStream[4] );
	multiReadChannelPartition(0, midTranspose[2], midValidTranspose[2], &outStream[8]  ,&validStream[8] );
	multiReadChannelPartition(0, midTranspose[3], midValidTranspose[3], &outStream[12] ,&validStream[12] );


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
#pragma HLS dependence variable=inp intra WAR true
		//#pragma HLS pipeline II=1
#pragma HLS unroll factor=16
		//		valToWrite = inp[i%NUM_OF_PARTITIONS][i/NUM_OF_PARTITIONS];
		line.set(i%16,inp[i%NUM_OF_PARTITIONS][i/NUM_OF_PARTITIONS]);
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
	bool terminate = false;
	//valid = validStream.read() ;
	//pair = inStream.read() ;
	bool readSuccess1, readSuccess2;
	comp:while(!terminate){

#pragma HLS dependence variable=arr distance=2 inter true
#pragma HLS pipeline II=1

		//std::cout << "Valid: " << valid << ", i: " << i << std::endl;
		//ContribPair pair;
		readSuccess1 = validStream.read_nb(valid);
		readSuccess2 = inStream.read_nb(pair);
		//		pair = inStream.read() ; // read the current address
		if(readSuccess1 && readSuccess2){
			if(valid){
				index = pair.indexData / NUM_OF_PARTITIONS;     //0 4 8
				currVal = arr[index];
				//std::cout << "INdex: " << index << ", i: " << pair.indexData << std::endl;
				val = pair.valData;			//1 5 9
				//2 6 10
				if (index == lastAddr) {	//3 7 11
					currVal = lastVal ; // pipeline forwarding
				}

				arr[index] = currVal + val; // update the current value and store it

				lastAddr = index ;
				lastVal = currVal + val ;
			}else terminate = true;
		}
		//if(valid){
		//		valid = validStream.read();
		//			pair = inStream.read();
		//}//else
		//{
		//	terminate = true;
		//break;
		//}
	}

}

static void compute_kernel(hls::stream<ContribPair>* inStream,hls::stream<bool>* validStream, ValueT arr[NUM_OF_PARTITIONS][BUCKET_WIDTH/NUM_OF_PARTITIONS]){
	//#pragma HLS array_partition variable=arr complete dim=1

	comp_unroll_loop: for(int i = 0; i < NUM_OF_PARTITIONS; i++){
		//#pragma HLS array_partition variable=arr cyclic factor=4 dim=1

#pragma HLS dependence variable=arr inter false
#pragma HLS dependence variable=arr intra false
#pragma HLS unroll
		compute(inStream[i], validStream[i], arr[i]);
	}
}

static void read_and_compute(LineT* inp, ValueT arr[NUM_OF_PARTITIONS][BUCKET_WIDTH/NUM_OF_PARTITIONS],int lineCnt){
	hls::stream<ContribPair> inStream[16];
	hls::stream<bool> inValStream[16];

	hls::stream<ContribPair> outStream[NUM_OF_PARTITIONS];
	hls::stream<bool> validStream[NUM_OF_PARTITIONS];

#pragma HLS STREAM variable = inStream depth = 1
#pragma HLS STREAM variable = inValStream depth = 1
#pragma HLS STREAM variable = outStream depth = 2
#pragma HLS STREAM variable = validStream depth = 2

#pragma HLS array_partition variable=inStream complete
#pragma HLS array_partition variable=inValStream complete
#pragma HLS array_partition variable=outStream complete
#pragma HLS array_partition variable=validStream complete
#pragma HLS array_partition variable=arr complete dim=1

#pragma HLS dataflow
	read_BRAM_to_FIFOS(inp, inStream, inValStream,lineCnt);
	multiRead16(inStream, inValStream, outStream, validStream);
	compute_kernel(outStream,validStream ,arr );
}

*/


extern "C" {

void top_kernel( outLineT* indexVals, outLineT* inputVals, outLineT* outputSums, outLineT* lineCounts) {
#pragma HLS INTERFACE m_axi port=inputVals offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=lineCounts offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=indexVals offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=outputSums offset=slave bundle=gmem2
#pragma HLS INTERFACE s_axilite port=inputVals bundle=control
#pragma HLS INTERFACE s_axilite port=indexVals bundle=control
#pragma HLS INTERFACE s_axilite port=outputSums bundle=control
#pragma HLS INTERFACE s_axilite port=lineCounts bundle=control

#pragma HLS INTERFACE s_axilite port=return bundle=control


	rcw_pipeline rcw;
	rcw.rcw_run(indexVals, inputVals, outputSums, lineCounts);

	/*ValueT outputArray[NUM_OF_PARTITIONS][BUCKET_WIDTH/NUM_OF_PARTITIONS];
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
*/
}
}
