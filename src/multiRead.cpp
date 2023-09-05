#include <hls_stream.h>

#include <assert.h>
#include "types.h"

#define CHANNEL_CNT 4
//#define INPUT_LINE_SIZE 16
#define DEPTH 2
#define BUFFER_SIZE 8


class multiRead{

public:
/*	void readDataFromBRAM(outLineT* inpAddr, outLineT* inpVals, hls::stream<ContribPairPacket>* inStreams,int totNumOfChan,int lineCnt){
		//#pragma HLS stream depth=8 variable=inStreams

#pragma HLS array_partition variable=inStreams complete
		//#pragma HLS array_partition variable=midChannels complete
//#pragma HLS array_partition variable=numOfElements complete


		for(int i = 0; i < lineCnt; i++){
#pragma HLS pipeline
			outLineT valLine = inpVals[i];
			outLineT addrLine = inpAddr[i];
			for(int p = 0; p < INPUT_LINE_SIZE; p++){
#pragma HLS unroll
				ContribPairPacket packet;
				packet.indexData = addrLine.get(p);
				packet.valData = valLine.get(p);
				inStreams[p%totNumOfChan].write(packet);
			}
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
	}*/

	void sendPackets(hls::stream<ContribPairPacket> inStreams[CHANNEL_CNT],hls::stream<ContribPairPacket> midChannels[CHANNEL_CNT][CHANNEL_CNT], int bitLevel){

		// 2 regs for each to have a dependency of distance 2 instead of 1, and allow II=1
		ContribPairPacket lastPacket[2][CHANNEL_CNT];
		bool successSent[2][CHANNEL_CNT];
		bool chDone[CHANNEL_CNT];
		bool successRead[2][CHANNEL_CNT];
	//	int processedElements[CHANNEL_CNT]; // this should be fine
		ap_uint<1> polarity = 0;

		//#pragma HLS stream depth=8 variable=inStreams
		//#pragma HLS stream depth=8 variable=midChannels
#pragma HLS array_partition variable=inStreams complete
#pragma HLS array_partition variable=midChannels complete
//#pragma HLS array_partition variable=numOfElements complete
#pragma HLS array_partition variable=successSent complete
#pragma HLS array_partition variable=chDone complete
#pragma HLS array_partition variable=successRead complete
#pragma HLS array_partition variable=lastPacket complete


		//INIT
		bool terminate = false;
		for (int chi=0; chi < CHANNEL_CNT; ++chi) {
#pragma HLS unroll
			successSent[0][chi] = true;
			successSent[1][chi] = true;
			successRead[0][chi] = false;
			successRead[1][chi] = false;
			lastPacket[0][chi].indexData = 0;
			lastPacket[1][chi].indexData = 0;
			lastPacket[0][chi].valData = 0;
			lastPacket[1][chi].valData = 0;
	//		processedElements[chi] = 0;
		//	numOfElements[chi] =  lineCnt;
			chDone[chi] = false;
		}

			while(!terminate){
#pragma HLS pipeline

#pragma HLS dependence variable=chDone,successSent,lastPacket,successRead distance=2 inter true

			// Compute termination condition for the next iteration
			terminate = true ;
			for (int chi=0; chi < CHANNEL_CNT; ++chi) {
#pragma HLS unroll
				terminate = terminate & chDone[chi];
			}


			for(int chi = 0; chi < CHANNEL_CNT; chi++){
#pragma HLS unroll
				//				if(!chDone[chi]){ // ASSUMING IF CHANNEL IS DONE, READ_NB RETURNS EMPTY!
				if(successSent[polarity][chi]){
					successRead[polarity][chi] = inStreams[chi].read_nb(lastPacket[polarity][chi]);
					if(successRead[polarity][chi] ){
						if(lastPacket[polarity][chi].valData == 0) chDone[chi] = true;
						else{ successSent[polarity][chi] = midChannels[lastPacket[polarity][chi].indexData.range((bitLevel+1)*2 - 1,(bitLevel)*2)][chi].write_nb(lastPacket[polarity][chi]);
					//	std::cout << "Send Bucks bitLevel: " << bitLevel << ", index: " <<lastPacket[polarity][chi].indexData << " to " << chi << ", " << i << std::endl;
						}//	if(successSent[polarity][chi]) chDone[chi] = !(numOfElements[chi] > ++processedElements[chi]) ;
					}
				}else{
					successSent[polarity][chi] = midChannels[lastPacket[polarity][chi].indexData.range((bitLevel+1)*2 - 1,(bitLevel)*2)][chi].write_nb(lastPacket[polarity][chi]);
					//if(successSent[polarity][chi]) 	chDone[chi] = !(numOfElements[chi] > ++processedElements[chi]) ;

				}
				//}
			}
			polarity++;
		}

		//Make sure no packet is left, in case last packets were unable to be sent
		bool allSent = false;
		while(!allSent){
			allSent = true;
			for(int chi = 0; chi < CHANNEL_CNT; chi++){
				if(!successSent[0][chi]) successSent[0][chi] = midChannels[lastPacket[0][chi].indexData.range((bitLevel+1)*2 - 1,(bitLevel)*2)][chi].write_nb(lastPacket[0][chi]);
				if(!successSent[1][chi]) successSent[1][chi] = midChannels[lastPacket[1][chi].indexData.range((bitLevel+1)*2 - 1,(bitLevel)*2)][chi].write_nb(lastPacket[1][chi]);
				allSent = allSent & successSent[0][chi] & successSent[1][chi];
			}
		}

		//Send termination
		ContribPairPacket termPacket;
		termPacket.indexData = 0;
		termPacket.valData = 0;
		for(int chi = 0; chi < CHANNEL_CNT; chi++){
#pragma HLS unroll
			for(int chi2 = 0; chi2 < CHANNEL_CNT; chi2++){
#pragma HLS unroll
				 midChannels[chi][chi2].write(termPacket);
			}
		}

	}

	void recvPackets(hls::stream<ContribPairPacket> midChannels[CHANNEL_CNT][CHANNEL_CNT], hls::stream<ContribPairPacket> outStreams[CHANNEL_CNT]){
#pragma HLS array_partition variable=midChannels complete
#pragma HLS array_partition variable=outStreams complete

		// 2 regs for each to have a dependency of distance 2 instead of 1, and allow II=1
		ContribPairPacket lastPacket[3][CHANNEL_CNT];
		bool successSent[3][CHANNEL_CNT];
		bool chDone[CHANNEL_CNT][CHANNEL_CNT];
		bool successRead[3][CHANNEL_CNT];
		//	int processedElements[CHANNEL_CNT]; // this should be fine
		ap_uint<2> polarity = 0;

//#pragma HLS array_partition variable=outStreams complete
#pragma HLS array_partition variable=midChannels complete dim=0
		//#pragma HLS array_partition variable=numOfElements complete
#pragma HLS array_partition variable=successSent complete dim=0
#pragma HLS array_partition variable=chDone complete dim=0
#pragma HLS array_partition variable=successRead complete dim=0
		//#pragma HLS array_partition variable=processedElements complete
#pragma HLS array_partition variable=lastPacket complete dim=0

		ap_uint<2> prioritizedIndex = 0;
		bool terminate = false;

		for (int chi=0; chi < CHANNEL_CNT; ++chi) {
#pragma HLS unroll
			successSent[0][chi] = true;
			successSent[1][chi] = true;
			successSent[2][chi] = true;
			successRead[0][chi] = false;
			successRead[1][chi] = false;
			successRead[2][chi] = false;
			lastPacket[0][chi].indexData = 0;
			lastPacket[1][chi].indexData = 0;
			lastPacket[2][chi].indexData = 0;
			lastPacket[0][chi].valData = 0;
			lastPacket[1][chi].valData = 0;
			lastPacket[2][chi].valData = 0;
			chDone[chi][0] = false;
			chDone[chi][1] = false;
			chDone[chi][2] = false;
			chDone[chi][3] = false;

			//processedElements[chi] = 0;
			//numOfElements[chi] =  lineCnt;
			//chDone[chi] = false;
		}


		while(!terminate){
#pragma HLS pipeline
#pragma HLS dependence variable=successSent,chDone,lastPacket,successRead distance=3 inter true

			// Compute termination condition for the next iteration
			terminate = true ;
			for (int chi=0; chi < CHANNEL_CNT; ++chi) {
#pragma HLS unroll
				for (int chi2=0; chi2 < CHANNEL_CNT; ++chi2){
#pragma HLS unroll
					terminate = terminate & chDone[chi][chi2];

				}
			}

			ch_loop:	for(int chi = 0; chi < CHANNEL_CNT; chi++){
#pragma HLS unroll
				int readThisIter = -1;
				if(successSent[polarity][chi]){
					if(!midChannels[chi][prioritizedIndex^((ap_uint<2>)(0))].empty()){
						successRead[polarity][chi] = midChannels[chi][prioritizedIndex^((ap_uint<2>)(0))].read_nb(lastPacket[polarity][chi]);
						readThisIter = prioritizedIndex^((ap_uint<2>)(0));
					}else if(!midChannels[chi][prioritizedIndex^((ap_uint<2>)(1))].empty()){
						successRead[polarity][chi] = midChannels[chi][prioritizedIndex^((ap_uint<2>)(1))].read_nb(lastPacket[polarity][chi]);
						readThisIter = prioritizedIndex^((ap_uint<2>)(1));
					}else if(!midChannels[chi][prioritizedIndex^((ap_uint<2>)(2))].empty()){
						successRead[polarity][chi] = midChannels[chi][prioritizedIndex^((ap_uint<2>)(2))].read_nb(lastPacket[polarity][chi]);
						readThisIter = prioritizedIndex^((ap_uint<2>)(2));
					}else if(!midChannels[chi][prioritizedIndex^((ap_uint<2>)(3))].empty()){
						successRead[polarity][chi] = midChannels[chi][prioritizedIndex^((ap_uint<2>)(3))].read_nb(lastPacket[polarity][chi]);
						readThisIter = prioritizedIndex^((ap_uint<2>)(3));
					}//else readThisIter = false;
					if( (readThisIter != -1) && successRead[polarity][chi]){
						if(lastPacket[polarity][chi].valData == 0)
						{	chDone[chi][readThisIter] = true;
						//	std::cout << "CH_DONE: " << readThisIter << std::endl;
						}
						else
							successSent[polarity][chi] = outStreams[chi].write_nb(lastPacket[polarity][chi]);
					}
				}else{
					successSent[polarity][chi] = outStreams[chi].write_nb(lastPacket[polarity][chi]);
				}
			}
			if(polarity == (ap_uint<2>)(2)) polarity = 0; else polarity++;
			prioritizedIndex++;
		}

		//Make sure no packet is left, in case last packets were unable to be sent
		bool allSent = false;
		while(!allSent){
			allSent = true;
			for(int chi = 0; chi < CHANNEL_CNT; chi++){
				if(!successSent[0][chi]) successSent[0][chi] = outStreams[chi].write_nb(lastPacket[0][chi]);
				if(!successSent[1][chi]) successSent[1][chi] = outStreams[chi].write_nb(lastPacket[1][chi]);
				if(!successSent[2][chi]) successSent[2][chi] = outStreams[chi].write_nb(lastPacket[2][chi]);
				allSent = allSent & successSent[0][chi] & successSent[1][chi] & successSent[2][chi];
			}
		}

		//Send termination
		ContribPairPacket termPacket;
		termPacket.indexData = 0;
		termPacket.valData = 0;
		for(int chi = 0; chi < CHANNEL_CNT; chi++){
#pragma HLS unroll
			outStreams[chi].write(termPacket);

		}
	}
	void transposeChannels(hls::stream<ContribPairPacket> inStreams[CHANNEL_CNT][CHANNEL_CNT],hls::stream<ContribPairPacket> outStreams[CHANNEL_CNT][CHANNEL_CNT]){

		bool successSent[2][CHANNEL_CNT][CHANNEL_CNT];
		bool successRead[2][CHANNEL_CNT][CHANNEL_CNT];
		ContribPairPacket lastPacket[2][CHANNEL_CNT][CHANNEL_CNT];
		bool chDone[CHANNEL_CNT][CHANNEL_CNT];
		ap_uint<1> polarity = 0;

#pragma HLS array_partition variable=successSent complete dim=0
#pragma HLS array_partition variable=chDone complete dim=0
#pragma HLS array_partition variable=successRead complete dim=0
#pragma HLS array_partition variable=lastPacket complete dim=0

		//INIT
		bool terminate = false;
		for (int chi=0; chi < CHANNEL_CNT; ++chi) {
#pragma HLS unroll
			for (int j=0; j < CHANNEL_CNT; ++j) {
#pragma HLS unroll
				successSent[0][chi][j] = true;
				successSent[1][chi][j] = true;
				successRead[0][chi][j] = false;
				successRead[1][chi][j] = false;
				lastPacket[0][chi][j].indexData = 0;
				lastPacket[1][chi][j].indexData = 0;
				lastPacket[0][chi][j].valData = 0;
				lastPacket[1][chi][j].valData = 0;
				chDone[chi][j] = false;
			}
		}

		while(!terminate){
#pragma HLS pipeline
#pragma HLS dependence variable=chDone,successSent,lastPacket,successRead distance=2 inter true

			// Compute termination condition for the next iteration
			terminate = true ;
			for (int i=0; i < CHANNEL_CNT; ++i) {
#pragma HLS unroll
				for(int j = 0; j < CHANNEL_CNT; j++){
#pragma HLS unroll
					terminate = terminate & chDone[i][j];
				}
			}

			for(int i = 0; i < CHANNEL_CNT; i++){
#pragma HLS unroll
				for(int j = 0; j < CHANNEL_CNT; j++){
#pragma HLS unroll
					if(successSent[polarity][i][j]){
						successRead[polarity][i][j] = inStreams[i][j].read_nb(lastPacket[polarity][i][j]);
						if( successRead[polarity][i][j] ){
							if( lastPacket[polarity][i][j].valData == 0 ) chDone[i][j] = true;
							else{ successSent[polarity][i][j] = outStreams[j][i].write_nb(lastPacket[polarity][i][j]);
					//		std::cout << "Transpose sending "<< lastPacket[polarity][i][j].indexData << " to " << j << ", " << i << std::endl;

							}
						}
					}else{
						successSent[polarity][i][j] = outStreams[j][i].write_nb(lastPacket[polarity][i][j]);
					}
				}
			}
		}
		//Make sure no packet is left, in case last packets were unable to be sent
		bool allSent = false;
		while(!allSent){
			allSent = true;
			for(int i = 0; i < CHANNEL_CNT; i++){
				for(int j = 0; j < CHANNEL_CNT; j++){
					if(!successSent[0][i][j]) successSent[0][i][j] = outStreams[j][i].write_nb(lastPacket[0][j][i]);
					else if(!successSent[1][i][j]) successSent[1][i][j] = outStreams[j][i].write_nb(lastPacket[1][j][i]);
					allSent = allSent & successSent[0][i][j] & successSent[1][i][j];
			}
		}
		}


		//Send termination
		ContribPairPacket termPacket;
		termPacket.indexData = 0;
		termPacket.valData = 0;
		for(int chi = 0; chi < CHANNEL_CNT; chi++){
#pragma HLS unroll
			for(int chi2 = 0; chi2 < CHANNEL_CNT; chi2++){
#pragma HLS unroll
				outStreams[chi][chi2].write(termPacket);
			}
		}

}
	 void testRead( hls::stream<ContribPairPacket>* outStream){
#pragma HLS array_partition variable=outStream complete
//#pragma HLS array_partition variable=outArr complete

		for(int i = 0; i < 100; i++){
			for(int chi = 0; chi < 16; chi++){
#pragma HLS unroll
				ContribPairPacket p;
				p = outStream[chi].read();
			}
		}
	}

/*	void readAndSend(outLineT* inpVals, outLineT* inpAddr, hls::stream<ContribPairPacket> midChannels[CHANNEL_CNT][CHANNEL_CNT],int lineCnt){
		hls::stream<ContribPairPacket> inStreams[CHANNEL_CNT];
#pragma HLS stream depth=8 variable=inStreams
#pragma HLS array_partition variable=inStreams complete

#pragma HLS array_partition variable=midChannels complete

#pragma HLS dataflow
		readDataFromBRAM(inpVals, inpAddr,inStreams, lineCnt);
		sendPackets(inStreams,midChannels, lineCnt);
	}*/
	void sendAndRecv( hls::stream<ContribPairPacket> inStreams[CHANNEL_CNT], hls::stream<ContribPairPacket> outStreams[CHANNEL_CNT],int bitLevel){
		hls::stream<ContribPairPacket> midChannels[CHANNEL_CNT][CHANNEL_CNT] ;

#pragma HLS array_partition variable=midChannels complete
#pragma HLS stream depth=4 variable=midChannels

#pragma HLS dataflow
		sendPackets(inStreams,midChannels, bitLevel);
		recvPackets(midChannels, outStreams);
	}

	/*void readandSend_debug(outLineT* inpVals, outLineT* inpAddr, int lineCnt){

		hls::stream<ContribPairPacket> inStreams[CHANNEL_CNT];
		hls::stream<bool> syncChannel[3];
		hls::stream<ContribPairPacket> midChannels[CHANNEL_CNT][CHANNEL_CNT] ;
		hls::stream<ContribPairPacket> outS[CHANNEL_CNT];
		ContribPairPacket outArr[4];

#pragma HLS stream depth=8 variable=inStreams
#pragma HLS stream depth=1 variable=syncChannel
#pragma HLS stream depth=8 variable=midChannels
#pragma HLS stream depth=8 variable=outS

#pragma HLS array_partition variable=inStreams complete
#pragma HLS array_partition variable=midChannels complete
#pragma HLS array_partition variable=outS complete
#pragma HLS array_partition variable=syncChannel complete

#pragma HLS dataflow
		readDataFromBRAM(inpVals, inpAddr,inStreams, lineCnt, syncChannel[0]);
		sendPackets(inStreams,midChannels, lineCnt, syncChannel[0],syncChannel[1]);

		//readAndSend(inpVals, inpAddr, midChannels,lineCnt);
		//rcvAndSend(midChannels);
		recvPackets(midChannels,outS,syncChannel[1],syncChannel[2]);
		testRead(outS,syncChannel[2]);
	}*/

	void readandSend16_debug(hls::stream<ContribPairPacket> *inStreams,hls::stream<ContribPairPacket> *outS){

		//	hls::stream<ContribPairPacket> inStreams[CHANNEL_CNT][CHANNEL_CNT];
			hls::stream<ContribPairPacket> transStreams[CHANNEL_CNT][CHANNEL_CNT];
			hls::stream<ContribPairPacket> midChannels[CHANNEL_CNT][CHANNEL_CNT] ;
//			hls::stream<ContribPairPacket> outS[CHANNEL_CNT];

	#pragma HLS stream depth=4 variable=inStreams
	#pragma HLS stream depth=4 variable=midChannels
	#pragma HLS stream depth=4 variable=transStreams
//	#pragma HLS stream depth=8 variable=outS

//	#pragma HLS array_partition variable=inStreams complete
	#pragma HLS array_partition variable=midChannels complete
	#pragma HLS array_partition variable=transStreams complete
//	#pragma HLS array_partition variable=outS complete

	#pragma HLS dataflow
			//readDataFromBRAM(inpVals, inpAddr,inStreams, lineCnt);
			sendAndRecv( &inStreams[0], midChannels[0], 1);
			sendAndRecv( &inStreams[4], midChannels[1], 1);
			sendAndRecv( &inStreams[8], midChannels[2], 1);
			sendAndRecv( &inStreams[12], midChannels[3], 1);
			transposeChannels( midChannels,transStreams );
			sendAndRecv( transStreams[0], &outS[0], 0);
			sendAndRecv( transStreams[1], &outS[4], 0);
			sendAndRecv( transStreams[2], &outS[8], 0);
			sendAndRecv( transStreams[3], &outS[12], 0);

		}
};
/*
class testMultiRead{
	outLineT inputIndexes[100];
	outLineT inputValues[100];
	multiRead toTest;
public:
	bool init(){
		for( int i = 0; i < 100; i++){
			outLineT oL;
			outLineT inL;
			for(int j = 0; j < 16; j++){
				inL.set(j,(IndexT)i);
				oL.set(j,(ValueT)j+1);
			}
			inputIndexes[i] = inL;
			inputValues[i] = oL;
		}


		return true;
	}
	void test(){
		hls::stream<ContribPairPacket> inStreams[16];
		hls::stream<ContribPairPacket> outStreams[16];

#pragma HLS stream depth=8 variable=inStreams
#pragma HLS stream depth=8 variable=outStreams
#pragma HLS array_partition variable=inStreams complete
#pragma HLS array_partition variable=outStreams complete

#pragma HLS dataflow
		toTest.readDataFromBRAM(inputIndexes, inputValues, inStreams,16, 100);
		toTest.readandSend16_debug(inStreams, outStreams);
		toTest.testRead(outStreams);

		//toTest.readandSend_debug(inputValues, inputIndexes, 100);
	}

};*/
