/**********
Copyright (c) 2018, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/
/*****************************************************************************************
*  GUI Flow :
*
*  By default this example supports 1DDR execution in GUI mode for
*  all the DSAs. To make this example to work with multi DDR DSAs
*  please follow steps mentioned below.
*
*  Note : "bandwidth" in map_connect options below is the kernel name defined in kernel.cl
*
*  ***************************************************************************************
*  DSA  (2DDR):
*
*  1.<SDx Project> > Properties > C/C++ Build > Settings > SDx XOCC Kernel Compiler
*                  > Miscellaneous > Other flags
*  2.In "Other flags" box enter following
*     a. --max_memory_ports all
*     b. --sp bandwidth_1.m_axi_gmem0:bank0
*     c. --sp bandwidth_1.m_axi_gmem1:bank1
*  3.<SDx Project> > Properties > C/C++ Build > Settings > SDx XOCC Kernel Linker
*                  > Miscellaneous > Other flags
*  4.Repeat step 2 above
*
* *****************************************************************************************
*  DSA  (3DDR):
*
*  1.<SDx Project> > Properties > C/C++ Build > Settings > SDx XOCC Kernel Compiler
*                  > Miscellaneous > Other flags
*  2.In "Other flags" box enter following
*     a. --max_memory_ports all
*     b. --sp bandwidth_1.m_axi_gmem0:bank0
*     c. --sp bandwidth_1.m_axi_gmem1:bank1
*     d. --sp bandwidth_1.m_axi_gmem2:bank2
*  3.<SDx Project> > Properties > C/C++ Build > Settings > SDx XOCC Kernel Linker
*                  > Miscellaneous > Other flags
*  4.Repeat step 2 above
*  5.Define NDDR_BANKS 3 in kernel "#define NDDR_BANKS 3" at the top of kernel.cl
*
* *****************************************************************************************
*  DSA  (4DDR):
*
*  1.<SDx Project> > Properties > C/C++ Build > Settings > SDx XOCC Kernel Compiler
*                  > Miscellaneous > Other flags
*  2.In "Other flags" box enter following
*     a. --max_memory_ports all
*     b. --sp bandwidth_1.m_axi_gmem0:bank0
*     c. --sp bandwidth_1.m_axi_gmem1:bank1
*     d. --sp bandwidth_1.m_axi_gmem2:bank2
*     e. --sp bandwidth_1.m_axi_gmem3:bank3
*  3.<SDx Project> > Properties > C/C++ Build > Settings > SDx XOCC Kernel Linker
*                  > Miscellaneous > Other flags
*  4.Repeat step 2 above
*  5.Define NDDR_BANKS 4 in kernel "#define NDDR_BANKS 4" at the top of kernel.cl
*
* *****************************************************************************************
*
*  CLI Flow:
*
*  In CLI flow makefile detects the DDR of a device and based on that
*  automatically it adds all the flags that are necessary. This example can be
*  used similar to other examples in CLI flow, extra setup is not needed.
*
*********************************************************************************************/
#include <xcl2.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "types.h"
#include <string>



string getBinaryFileName(char* xcl_mode) {

  std::string binaryFile = "/home/mkara/tutorial/prop_block/" ;
  if (xcl_mode == NULL)
    binaryFile += "System" ;
  else if (string(xcl_mode) == "hw_emu")
    binaryFile += "Emulation-HW" ;
  else
    binaryFile += "Emulation-SW" ;
  binaryFile += "/binary_container_1.xclbin" ;

  return binaryFile ;
}

typedef vector<ValueT, aligned_allocator<ValueT>> ValueVec;
typedef vector<IndexT, aligned_allocator<IndexT>> IndexVec;
typedef vector<LineT, aligned_allocator<LineT>> LineVector;
typedef vector<outLineT, aligned_allocator<outLineT>> outLineVector;
typedef vector<outLineT, aligned_allocator<outLineT>> lineCountsVector;

int main(int argc, char** argv) {

    if(argc != 1) {
        printf("Usage: %s\n", argv[0]);
        return EXIT_FAILURE;
    }
    srand(time(NULL));


    cl_int err;
    std::vector<cl::Device> devices = xcl::get_xil_devices();
    cl::Device device = devices[0];

    OCL_CHECK(err, cl::Context context(device, NULL, NULL, NULL, &err));
    OCL_CHECK(err, cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE, &err));
    OCL_CHECK(err, std::string device_name = device.getInfo<CL_DEVICE_NAME>(&err));
    std::cout << "Found Device=" << device_name.c_str() << std::endl;

    size_t globalbuffersize = 1024*1024*1024;    /* 1GB */

    /* Reducing the data size for emulation mode */
    char *xcl_mode = getenv("XCL_EMULATION_MODE");
    if (xcl_mode != NULL){
        globalbuffersize = 1024 * 1024;  /* 1MB */
    }
    std::cout << "   Global buffer size is: " << globalbuffersize << ", and xcl_mode = "
	      << (xcl_mode ? xcl_mode : "NULL") << std::endl ;

    string binaryFile = getBinaryFileName(xcl_mode) ;
    std::cout << "..... Loading binary file " << binaryFile << std::endl ;

    cl::Program::Binaries bins = xcl::import_binary_file(binaryFile.c_str());
    devices.resize(1);
    OCL_CHECK(err, cl::Program program(context, devices, bins, NULL, &err));

	/* Initialize Buffers */
    LineVector inputVec(100); // -- to do Let 1 MB for test purpose, then 1024*1024*8/512 = 16*1024
    outLineVector sums(NUM_OF_BUCKETS*LINES_PER_BUCKET);
    outLineVector sums_sw(NUM_OF_BUCKETS*LINES_PER_BUCKET);
    lineCountsVector lineCounts(NUM_OF_BUCKETS/16);

    int lastLineIndex = -1;
    int neededVectorSize = 0;
    for(uint i = 0; i < NUM_OF_BUCKETS; i++) { //should work fine ...
    	int j;
    	int numOfTestValues = rand() % (BUCKET_DENSITY_FOR_TESTING*BUCKET_WIDTH);
    	int lineCountPerThisBucket = ceil(numOfTestValues/8.0);
    	neededVectorSize += lineCountPerThisBucket;
    	if(neededVectorSize > inputVec.size())
    		inputVec.resize(neededVectorSize);
    	if(i%16 == 0){
    		outLineT t;
    		t.data = 0;
    		lineCounts[i/16] = t;
    	}
//    	std::cout << lineCountPerThisBucket << " lp " <<std::endl;
    	lineCounts[i/16].set(i%16, lineCountPerThisBucket);
    	int base = i*LINES_PER_BUCKET;
    	//fill input stream vector and sw sim results
    	for(j = 0; j < numOfTestValues; j++ ){
    		if (j%8 == 0) lastLineIndex++;
    		ContribPair inp;
    		inp.indexData = rand() % (BUCKET_WIDTH) ;
    		inp.valData = (rand() % 10) + 1;
    		inputVec[lastLineIndex].set(j%8, inp);

    		ValueT prev = sums_sw[ base + inp.indexData/16].get(inp.indexData%16);
    		sums_sw[base + inp.indexData/16].set(inp.indexData%16,inp.valData + prev);
    		//std::cout << "Sums_sw: " << sums_sw[base + inp.indexData/16].get(inp.indexData%16) << std::endl;
    	}
    	//fill last line of input vec with zeros if there are remaining empty spaces
    	if(numOfTestValues%8 != 0 )
    		while(j%8 !=  0){
    			ContribPair inp;
    			inp.valData = 0;
    			inp.indexData = 0;
    			inputVec[ lastLineIndex].set(j%8, inp);
    			j++;
    		}
    }
    std::cout << "Input vector size is " << neededVectorSize << ", needed memory size is: " << neededVectorSize*64 << std::endl ;
    globalbuffersize = neededVectorSize*64;

    short ddr_banks = NDDR_BANKS;
    std::cout << "Global Buffer size is now: " << globalbuffersize <<std::endl ;

    /* Index for the ddr pointer array: 4=4, 3=3, 2=2, 1=2 */
    char num_buffers = ddr_banks;
    if(ddr_banks==1)
        num_buffers = ddr_banks + (ddr_banks % 2);

    /* buffer[0] is input0
     * buffer[1] is output0
     * buffer[2] is input1
     * buffer[3] is output1 */
    cl::Buffer *buffer[num_buffers];
    cl::Buffer *lineCountBuffer;
    cl_mem_ext_ptr_t ext_buffer[num_buffers];
    cl_mem_ext_ptr_t ext_buffer_lc;
    #if NDDR_BANKS > 1
        unsigned xcl_bank[4] = {
            XCL_MEM_DDR_BANK0,
            XCL_MEM_DDR_BANK1,
            XCL_MEM_DDR_BANK2,
            XCL_MEM_DDR_BANK3
        };

        for (int i = 0; i < ddr_banks; i++) {
            ext_buffer[i].flags = xcl_bank[i];
            ext_buffer[i].obj = NULL;
            ext_buffer[i].param = 0;

	        buffer[i] = new cl::Buffer(context,
		                           CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                    			   globalbuffersize,
            		    		   &ext_buffer[i],
			                	   &err);

            if(err != CL_SUCCESS) {
                printf("Error: Failed to allocate buffer in DDR bank %zu\n", globalbuffersize);
                return EXIT_FAILURE;
            }
        } /* End for (i < ddr_banks) */
        ext_buffer_lc.flags = xcl_bank[0];
        ext_buffer_lc.obj = NULL;
        ext_buffer_lc.param = 0;

        OCL_CHECK(err, lineCountBuffer = new cl::Buffer(context,
                			            	CL_MEM_READ_ONLY| CL_MEM_EXT_PTR_XILINX,
                            				lineCounts.size()*512,
											&ext_buffer_lc,
                            				&err));

    #else
	     OCL_CHECK(err, buffer[0] = new cl::Buffer(context,
         				            CL_MEM_READ_WRITE,
                    				globalbuffersize,
        			            	NULL,
                    				&err));
    	 OCL_CHECK(err, buffer[1] = new cl::Buffer(context,
        			            	CL_MEM_READ_WRITE,
                    				globalbuffersize,
        			            	NULL,
                    				&err));
    #endif
    cl_ulong num_blocks = globalbuffersize /64;
    double dbytes = globalbuffersize;
    double dmbytes = dbytes / (((double)1024) * ((double)1024));
    printf("Starting kernel to read/write %.0lf MB bytes from/to global memory... \n", dmbytes);

    /* Write input buffer */
    outLineT *map_lcount_buffer;
    OCL_CHECK(err, map_lcount_buffer = (outLineT*) q.enqueueMapBuffer(*(lineCountBuffer),
							                                 CL_FALSE,
                            							     CL_MAP_WRITE_INVALIDATE_REGION,
                            							     0,
															 lineCounts.size()*512,
                            							     NULL,
							                                 NULL,
                            							     &err));
    OCL_CHECK(err, err = q.finish());
    for(size_t i = 0; i< lineCounts.size(); i++) {
            map_lcount_buffer[i] = lineCounts[i];
            //if(i<30)
            	//std::cout << "i: " << i <<", line count: " <<  lineCounts[i] << std::endl;
        }
        OCL_CHECK(err, err = q.enqueueUnmapMemObject(*(buffer[0]),
                    				  map_lcount_buffer));

        OCL_CHECK(err, err = q.finish());

    /* Map input buffer for PCIe write */
    LineT *map_input_buffer0;
    OCL_CHECK(err, map_input_buffer0 = (LineT*) q.enqueueMapBuffer(*(buffer[0]),
							                                 CL_FALSE,
                            							     CL_MAP_WRITE_INVALIDATE_REGION,
                            							     0,
                               							     globalbuffersize,
                            							     NULL,
							                                 NULL,
                            							     &err));
    OCL_CHECK(err, err = q.finish());
    std::cout << "..... Came SO FAR" << std::endl ;

    /* prepare data to be written to the device */
    for(size_t i = 0; i< inputVec.size(); i++) {
        map_input_buffer0[i] = inputVec[i];
    }
    OCL_CHECK(err, err = q.enqueueUnmapMemObject(*(buffer[0]),
                				  map_input_buffer0));

    OCL_CHECK(err, err = q.finish());


    #if NDDR_BANKS > 3
        unsigned char *map_input_buffer1;
     	OCL_CHECK(err, map_input_buffer1 = (unsigned char *) q.enqueueMapBuffer(*(buffer[2]),
	                                							 CL_FALSE,
                                								 CL_MAP_WRITE_INVALIDATE_REGION,
                                    							 0,
                                								 globalbuffersize,
                                								 NULL,
                                								 NULL,
                                								 &err));
    	OCL_CHECK(err, err = q.finish());

        /* Prepare data to be written to the device */
        for(size_t i = 0; i < globalbuffersize; i++) {
            map_input_buffer1[i] = index_host[i];
        }

	    OCL_CHECK(err, err = q.enqueueUnmapMemObject(*(buffer[2]),
		                		      map_input_buffer1));
        OCL_CHECK(err, err = q.finish());
    #endif

    /* Set the kernel arguments */
    OCL_CHECK(err, cl::Kernel krnl_global_bandwidth(program, "top_kernel", &err));
    int arg_index = 0;
    int buffer_index = 0;

    OCL_CHECK(err, err = krnl_global_bandwidth.setArg(arg_index++, *(buffer[buffer_index++])));
    OCL_CHECK(err, err = krnl_global_bandwidth.setArg(arg_index++, *(buffer[buffer_index++])));
    #if NDDR_BANKS == 3
       OCL_CHECK(err, err = krnl_global_bandwidth.setArg(arg_index++, *(buffer[buffer_index++])));
       OCL_CHECK(err, err = krnl_global_bandwidth.setArg(arg_index++, *lineCountBuffer ));
    #elif NDDR_BANKS > 3
       OCL_CHECK(err, err = krnl_global_bandwidth.setArg(arg_index++, *(buffer[buffer_index++])));
       OCL_CHECK(err, err = krnl_global_bandwidth.setArg(arg_index++, *(buffer[buffer_index++])));
    #endif
//    OCL_CHECK(err, err = krnl_global_bandwidth.setArg(arg_index++, num_blocks));
    printf("Kernel args set...");

    unsigned long nsduration;
    cl::Event event;

    /* Execute Kernel */
    OCL_CHECK(err, err = q.enqueueTask(krnl_global_bandwidth, NULL, &event));
    OCL_CHECK(err, err = event.wait());
    nsduration = OCL_CHECK(err, event.getProfilingInfo<CL_PROFILING_COMMAND_END>(&err)) - OCL_CHECK(err, event.getProfilingInfo<CL_PROFILING_COMMAND_START>(&err));

    /* Copy results back from OpenCL buffer */
    outLineT *map_output_buffer0;

    OCL_CHECK(err, map_output_buffer0 = (outLineT *) q.enqueueMapBuffer(*(buffer[2]),
                            							      CL_FALSE,
                            							      CL_MAP_READ,
                            							      0,
                            							      globalbuffersize,
                            							      NULL,
                            							      NULL,
                            							      &err));
    OCL_CHECK(err, err = q.finish());

    std::cout << "Kernel Duration..." << nsduration << " ns" <<std::endl;
int numOfmismatch = 0;
    /* Check the results of output0 */
    for (int i = 0; i < OUTPUT_VECTOR_SIZE/16; i++) {
    	for(int j = 0; j <16;j++)
    		if (map_output_buffer0[i].get(j) != sums_sw[i].get(j) )//(input_host[i] + 1) % 256)
    	  {
    			numOfmismatch++;
    	  	  std::cout << "ERROR : kernel0 failed to copy line "<< i <<" data" << j <<"; should be "<<  sums_sw[i].get(j) << "but hw res is " <<map_output_buffer0[i].get(j)<< std::endl ;

        }
    }
    if(numOfmismatch > 0){
    	std::cout << "ERROR : NUM OF MISMATCH = "<< numOfmismatch << std::endl ;
    	return EXIT_FAILURE;
    }
    #if NDDR_BANKS > 3
        unsigned char *map_output_buffer1;
  	    OCL_CHECK(err, map_output_buffer1 = (unsigned char *) q.enqueueMapBuffer(*(buffer[3]),
                                								  CL_FALSE,
                                								  CL_MAP_READ,
                                								  0,
                                								  globalbuffersize,
                                								  NULL,
                                								  NULL,
                                								  &err));
        OCL_CHECK(err, err = q.finish());

        /* Check the results of output1 */
        for (size_t i = 0; i < globalbuffersize; i++) {
	  if (map_output_buffer1[i] != (input_host[i] + 1) % 256) {
                printf("ERROR : kernel1 failed to copy entry %zu input %i output %i\n", i, input_host[i], map_output_buffer1[i]);
                return EXIT_FAILURE;
            }
        }
    #endif

    #if NDDR_BANKS > 1
    for(int i = 0; i < ddr_banks; i++)
     {
	delete(buffer[i]);
     }
    #else
	delete(buffer[0]);
	delete(buffer[1]);
    #endif

    /* Profiling information */
    double dnsduration = ((double)nsduration);
    double dsduration = dnsduration / ((double) 1000000000);
    double pairsPerSec = (neededVectorSize/dsduration);
    double bpersec = (dbytes/dsduration);
    double mbpersec = bpersec / ((double) 1024*1024 ) * ddr_banks;

    printf("Kernel completed read/write %.0lf MB bytes from/to global memory.\n", dmbytes);
    printf("Execution time = %f (sec) \n", dsduration);
    printf("Pair per sec = %f \n", pairsPerSec);
    printf("Concurrent Read and Write Throughput = %f (MB/sec) \n", mbpersec);

    printf("TEST PASSED\n");
    return EXIT_SUCCESS;
}
