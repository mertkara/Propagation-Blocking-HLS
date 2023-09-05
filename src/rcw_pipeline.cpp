#include "multiRead.cpp"

static void initializeSumArray(ValueT buffer[NUM_OF_PARTITIONS][BUCKET_WIDTH/NUM_OF_PARTITIONS]){
	for(int i = 0; i < NUM_OF_PARTITIONS; i++){
#pragma HLS unroll
		for( int j = 0; j < BUCKET_WIDTH/NUM_OF_PARTITIONS; j++){
#pragma HLS unroll factor 2
			buffer[i][j] = 0;
		}
	}
}



static void compute(hls::stream<ContribPairPacket> &inStream, ValueT arr[BUCKET_WIDTH/NUM_OF_PARTITIONS] ){
	#pragma HLS function_instantiate variable=inStream
	IndexT lastAddr = -1;
	ValueT lastVal = 0;
	ContribPairPacket pair;
	IndexT index;
	ValueT val;
	ValueT currVal;
	bool valid;
	bool terminate = false;
	bool readSuccess2;
	int debug = 0;
	comp:while(!terminate){

#pragma HLS dependence variable=arr distance=2 inter true
#pragma HLS pipeline II=1

		readSuccess2 = inStream.read_nb(pair);
		if(readSuccess2){
			if(pair.valData != 0){
				debug++;
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
//	std::cout << "COMP DEBUG: " << debug << std::endl;


}

static void compute_kernel(hls::stream<ContribPairPacket>* inStream, ValueT arr[NUM_OF_PARTITIONS][BUCKET_WIDTH/NUM_OF_PARTITIONS]){
	//#pragma HLS array_partition variable=arr complete dim=1

	comp_unroll_loop: for(int i = 0; i < NUM_OF_PARTITIONS; i++){
		//#pragma HLS array_partition variable=arr cyclic factor=4 dim=1
#pragma HLS dependence variable=arr inter false
#pragma HLS unroll
		compute(inStream[i], arr[i]);
	}
}




class rcw_pipeline{


	void write_output_to_GMEM(ValueT inp[NUM_OF_PARTITIONS][BUCKET_WIDTH/NUM_OF_PARTITIONS], int inpBegin, outLineT *buffer ) {
		static int bucketIDst = 0;
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
			buffer[bucketIDst*BUCKET_WIDTH/16+i] = outBuffer[i];
		}
		bucketIDst++;
		//std::cout<< "write Bucket: "<<bucketID << std::endl;

	}


	void read_Addr_to_BRAM(outLineT* inp, outLineT* bramArray, int lineCnt) {
		static int inputStreamStartingIndexAddr = 0;
		mem_rd: for (int i=0; i < lineCnt; ++i) {
#pragma HLS pipeline II=1
			bramArray[i] = inp[inputStreamStartingIndexAddr + i];
		}
		inputStreamStartingIndexAddr += lineCnt;
	}

	void read_Delta_to_BRAM(outLineT* inp, outLineT* bramArray, int lineCnt) {
		static int inputStreamStartingIndexDelta = 0;
		mem_rd: for (int i=0; i < lineCnt; ++i) {
#pragma HLS pipeline II=1
			bramArray[i] = inp[inputStreamStartingIndexDelta + i];
		}
		inputStreamStartingIndexDelta += lineCnt;
	}


	void readDataFromBRAM(outLineT* inpAddr, outLineT* inpVals, hls::stream<ContribPairPacket>* inStreams,int totNumOfChan,int lineCnt){

#pragma HLS array_partition variable=inStreams complete
		//std::cout << "read from BRAM: " << std::endl;
		for(int i = 0; i < lineCnt; i++){
#pragma HLS pipeline
			outLineT valLine = inpVals[i];
			outLineT addrLine = inpAddr[i];
		//	std::cout << "Line " << i << " of " << lineCnt << ": ";
			for(int p = 0; p < INPUT_LINE_SIZE; p++){
#pragma HLS unroll
				ContribPairPacket packet;
				packet.indexData = addrLine.get(p);
				packet.valData = valLine.get(p);
			//	std::cout << "i: " << packet.indexData << "v: " << packet.valData << ", ";
				if(packet.valData != 0) inStreams[p%totNumOfChan].write(packet);
			}
		//	std::cout << std::endl;
		}

		//send termination signals
		for(int p = 0; p < totNumOfChan; p++){
#pragma HLS unroll
			ContribPairPacket packet;
			packet.indexData = 0;
			packet.valData = 0;
			inStreams[p].write(packet);
		}
		//return;
	}

	void read_and_compute(outLineT* addrBram, outLineT* deltaBram, ValueT outVectorPartitions[NUM_OF_PARTITIONS][BUCKET_WIDTH/NUM_OF_PARTITIONS],int lineCnt){
		hls::stream<ContribPairPacket> inStream[NUM_OF_PARTITIONS];
		hls::stream<ContribPairPacket> outStream[NUM_OF_PARTITIONS];
		multiRead multiReadUnit;

#pragma HLS STREAM variable = inStream depth = 8
#pragma HLS STREAM variable = outStream depth = 8

#pragma HLS array_partition variable=inStream complete
#pragma HLS array_partition variable=outStream complete
#pragma HLS array_partition variable=outVectorPartitions complete dim=1

#pragma HLS dataflow
		readDataFromBRAM(addrBram,deltaBram, inStream, NUM_OF_PARTITIONS, lineCnt);
		multiReadUnit.readandSend16_debug( inStream, outStream );
		compute_kernel(outStream, outVectorPartitions );
	}

	int read_line_counts(outLineT *in, ValueT* array) {

		int sum = 0;
		lineCount_rd:	for (int i = 0; i < NUM_OF_BUCKETS; i++) {
	#pragma HLS PIPELINE II=1
			array[i] = in[i/16].get(i%16);
//			std::cout << "LC Bucket " << i << " out of "<< NUM_OF_BUCKETS <<": "<<array[i] << std::endl ;
			sum += (in[i/16].get(i%16)/INPUT_ARRAY_SIZE );
			if(array[i]%INPUT_ARRAY_SIZE != 0) sum++;
			//std::cout << "SumNext: "<<sum <<std::endl;
		}
		return sum;
	}
public:
	void rcw_run(outLineT* addrInput, outLineT* deltaInput, outLineT* outVec, outLineT* lineCounts){
		static ValueT outputArray[NUM_OF_PARTITIONS][BUCKET_WIDTH/NUM_OF_PARTITIONS];
		static ValueT outputArrSec[NUM_OF_PARTITIONS][BUCKET_WIDTH/NUM_OF_PARTITIONS]; //manual cyclic partition and ping-pong buf
		static ValueT lCounts[NUM_OF_BUCKETS + 1]; //+1 for safety in the pipeline, dummy var
		static outLineT addrBram[INPUT_ARRAY_SIZE];
		static outLineT addrBramSec[INPUT_ARRAY_SIZE];
		static outLineT deltaBram[INPUT_ARRAY_SIZE];
		static outLineT deltaBramSec[INPUT_ARRAY_SIZE];

		//#pragma HLS array_partition variable=addrBram complete dim=1
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

//		initializeSumArray(outputArray);
//		initializeSumArray(outputArrSec);

		shouldCarryOn = (remainingLines > INPUT_ARRAY_SIZE);
		readLines = shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines;
		read_Addr_to_BRAM(addrInput, addrBram, readLines);
		read_Delta_to_BRAM(deltaInput, deltaBram, readLines);
		bucketID = shouldCarryOn ? bucketID:(bucketID + 1);
		remainingLines = shouldCarryOn ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];

		shouldCarryOn2 = (remainingLines > INPUT_ARRAY_SIZE);
		read_Addr_to_BRAM(addrInput, addrBramSec, shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines);
		read_Delta_to_BRAM(deltaInput, deltaBramSec, shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines);
		read_and_compute(addrBram,deltaBram ,outputArray, readLines);
		readLines = shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines;
		bucketID = shouldCarryOn2 ? bucketID:(bucketID + 1);
		remainingLines = shouldCarryOn2 ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];



		for( i = 0; i < totalCounts-2 ; i++){
			//#pragma HLS dependence variable=addrBram inter false

			if(i%2 == 0){
				if(!shouldCarryOn){
					if(outputBucketID == 0){
						shouldCarryOn = (remainingLines > INPUT_ARRAY_SIZE);
						read_and_compute(addrBramSec, deltaBramSec, outputArrSec, readLines);
						read_Addr_to_BRAM(addrInput, addrBram, shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines);
						read_Delta_to_BRAM(deltaInput, deltaBram, shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines);
						write_output_to_GMEM(outputArray, i*BUCKET_WIDTH/16, outVec);
						//	initializeSumArray(outputArray); no need since write_output_to gmem already does this now!
						outputBucketID++;
						readLines = shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines;
						bucketID = shouldCarryOn ? bucketID:(bucketID + 1);
						remainingLines = shouldCarryOn ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];
					}else{
						shouldCarryOn = (remainingLines > INPUT_ARRAY_SIZE);
						read_and_compute(addrBramSec,deltaBramSec, outputArray, readLines);
						read_Addr_to_BRAM(addrInput, addrBram, shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines);
						read_Delta_to_BRAM(deltaInput, deltaBram, shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines);
						write_output_to_GMEM(outputArrSec, i*BUCKET_WIDTH/16, outVec);
						//					initializeSumArray(outputArrSec);
						outputBucketID--;
						readLines = shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines;
						bucketID = shouldCarryOn ? bucketID:(bucketID + 1);
						remainingLines = shouldCarryOn ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];
					}

				}else{
					if(outputBucketID == 0){
						shouldCarryOn = (remainingLines > INPUT_ARRAY_SIZE);
						read_and_compute(addrBramSec,deltaBramSec, outputArray, readLines);
						read_Addr_to_BRAM(addrInput, addrBram, shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines);
						read_Delta_to_BRAM(deltaInput, deltaBram, shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines);
						readLines = shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines;
						bucketID = shouldCarryOn ? bucketID:(bucketID + 1);
						remainingLines = shouldCarryOn ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];
					}else{
						shouldCarryOn = (remainingLines > INPUT_ARRAY_SIZE);
						read_and_compute(addrBramSec,deltaBramSec, outputArrSec, readLines);
						read_Addr_to_BRAM(addrInput, addrBram, shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines);
						read_Delta_to_BRAM(deltaInput, deltaBram, shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines);
						readLines = shouldCarryOn ? INPUT_ARRAY_SIZE: remainingLines;
						bucketID = shouldCarryOn ? bucketID:(bucketID + 1);
						remainingLines = shouldCarryOn ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];
					}
				}
			}else{
				if(!shouldCarryOn2){
					if(outputBucketID == 0){
						shouldCarryOn2 = (remainingLines > INPUT_ARRAY_SIZE);
						read_and_compute(addrBram, deltaBram, outputArrSec, readLines);
						read_Addr_to_BRAM(addrInput, addrBramSec, shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines);
						read_Delta_to_BRAM(deltaInput, deltaBramSec, shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines);
						write_output_to_GMEM(outputArray, i*BUCKET_WIDTH/16, outVec);
						//					initializeSumArray(outputArray);
						outputBucketID++;
						readLines = shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines;
						bucketID = shouldCarryOn2 ? bucketID:(bucketID + 1);
						remainingLines = shouldCarryOn2 ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];

					}else{
						shouldCarryOn2 = (remainingLines > INPUT_ARRAY_SIZE);
						read_and_compute(addrBram,deltaBram, outputArray, readLines);
						read_Addr_to_BRAM(addrInput, addrBramSec, shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines);
						read_Delta_to_BRAM(deltaInput, deltaBramSec, shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines);
						write_output_to_GMEM(outputArrSec, i*BUCKET_WIDTH/16, outVec);
						//					initializeSumArray(outputArrSec);
						outputBucketID--;
						readLines = shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines;
						bucketID = shouldCarryOn2 ? bucketID:(bucketID + 1);
						remainingLines = shouldCarryOn2 ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];
					}
				}else{
					if(outputBucketID == 0){
						shouldCarryOn2 = (remainingLines > INPUT_ARRAY_SIZE);
						read_and_compute(addrBram, deltaBram, outputArray, readLines);
						read_Addr_to_BRAM(addrInput, addrBramSec, shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines);
						read_Delta_to_BRAM(deltaInput, deltaBramSec, shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines);
						readLines = shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines;
						bucketID = shouldCarryOn2 ? bucketID:(bucketID + 1);
						remainingLines = shouldCarryOn2 ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];

					}else{
						shouldCarryOn2 = (remainingLines > INPUT_ARRAY_SIZE);
						read_and_compute(addrBram, deltaBram,outputArrSec, readLines);
						read_Addr_to_BRAM(addrInput, addrBramSec, shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines);
						read_Delta_to_BRAM(deltaInput, deltaBramSec, shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines);
						readLines = shouldCarryOn2 ? INPUT_ARRAY_SIZE: remainingLines;
						bucketID = shouldCarryOn2 ? bucketID:(bucketID + 1);
						remainingLines = shouldCarryOn2 ? (remainingLines - INPUT_ARRAY_SIZE):  (int)lCounts[bucketID];
					}
				}
			}
		}
		//std::cout << i<<"bucket: "<< bucketID << " rmml: "<< remainingLines << ", readLines: "<< readLines<< std::endl;
		if(i%2 == 0){
			if(!shouldCarryOn){
				if(outputBucketID == 0){
					//std::cout << "1" << std::endl;
					read_and_compute(addrBramSec,deltaBramSec, outputArrSec, readLines);
					write_output_to_GMEM(outputArray, i*BUCKET_WIDTH/16, outVec);
					//			initializeSumArray(outputArray);
					outputBucketID++;
				}else{
					//std::cout << "2" << std::endl;
					read_and_compute(addrBramSec,deltaBramSec, outputArray, readLines);
					write_output_to_GMEM(outputArrSec, i*BUCKET_WIDTH/16, outVec);
					//		initializeSumArray(outputArrSec);
					outputBucketID--;
				}
			}else{
				//std::cout << 3+outputBucketID << std::endl;
				if(outputBucketID == 0)
					read_and_compute(addrBramSec,deltaBramSec, outputArray, readLines);
				else
					read_and_compute(addrBramSec,deltaBramSec, outputArrSec, readLines);
			}
			i++;
		}else{
			if(!shouldCarryOn2){
				//std::cout << 5+outputBucketID << std::endl;
				if(outputBucketID == 0){
					read_and_compute(addrBram,deltaBram, outputArrSec, readLines);
					write_output_to_GMEM(outputArray, i*BUCKET_WIDTH/16, outVec);
					//	initializeSumArray(outputArray);
					outputBucketID++;
				}else{
					read_and_compute(addrBram,deltaBram, outputArray, readLines);
					write_output_to_GMEM(outputArrSec, i*BUCKET_WIDTH/16, outVec);
					//initializeSumArray(outputArrSec);
					outputBucketID--;
				}
			}else{
				//std::cout << 7+outputBucketID << std::endl;
				if(outputBucketID == 0)
					read_and_compute(addrBram,deltaBram, outputArray, readLines);
				else
					read_and_compute(addrBram,deltaBram, outputArrSec, readLines);
			}
			i++;
		}
		if(outputBucketID == 0){
			//std::cout << "9" << std::endl;
			write_output_to_GMEM(outputArray, i*BUCKET_WIDTH/16, outVec);
		}else{
			//std::cout << "10" << std::endl;
			write_output_to_GMEM(outputArrSec, i*BUCKET_WIDTH/16, outVec);
		}


	}


};
