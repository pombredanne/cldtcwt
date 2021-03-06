// Copyright (C) 2013 Timothy Gale
#include "decimateTripleFilterX.h"
#include "util/clUtil.h"
#include <sstream>
#include <string>
#include <iostream>
#include <cassert>

#include "kernel.h"

using namespace DecimateTripleFilterXNS;


cl::Buffer uploadReversedFilter(cl::Context& context,
                                const std::vector<float>& filter)
{
    // Take a vector of filter coefficients, and put them in the 
    // context in reversed order, read only.


    size_t filterLength_ = filter.size();

    // We want the filter to be reversed 
    std::vector<float> reversedFilter(filter.size());

    for (int n = 0; n < filter.size(); ++n) 
        reversedFilter[n] = filter[filterLength_ - n - 1];

    // Upload the filter coefficients
    return cl::Buffer(context,
                      CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                      reversedFilter.size() * sizeof(float),
                      &reversedFilter[0]);
}




DecimateTripleFilterX::DecimateTripleFilterX(cl::Context& context, 
                 const std::vector<cl::Device>& devices,
                 std::vector<float> filter0, bool swapPairOrder0,
                 std::vector<float> filter1, bool swapPairOrder1,
                 std::vector<float> filter2, bool swapPairOrder2)
{
    // Bundle the code up
    cl::Program::Sources source;
    source.push_back(
        std::make_pair(reinterpret_cast<const char*>(kernel_cl), 
                       kernel_cl_len)
    );

    filterLength_ = filter0.size();

    std::ostringstream compilerOptions;
    compilerOptions << "-D WG_W=" << workgroupSize_ << " "
                    << "-D WG_H=" << workgroupSize_ << " "
                    << "-D FILTER_LENGTH=" << filterLength_ << " ";

    if (swapPairOrder0)
        compilerOptions << "-D SWAP_TREE_0 ";

    if (swapPairOrder1)
        compilerOptions << "-D SWAP_TREE_1 ";

    if (swapPairOrder2)
        compilerOptions << "-D SWAP_TREE_2 ";

    // Compile it...
    cl::Program program(context, source);
    try {
        program.build(devices, compilerOptions.str().c_str());
    } catch(cl::Error err) {
	    std::cerr 
		    << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(devices[0])
		    << std::endl;
	    throw;
    } 
        
    // ...and extract the useful part, viz the kernel
    kernel_ = cl::Kernel(program, "decimateTripleFilterX");


    // Put the filters up (see function above)
    filter0_ = uploadReversedFilter(context, filter0);
    filter1_ = uploadReversedFilter(context, filter1);
    filter2_ = uploadReversedFilter(context, filter2);

    // Set that filter for use
    kernel_.setArg(7, filter0_);
    kernel_.setArg(8, filter1_);
    kernel_.setArg(9, filter2_);

    // Make sure the filter is even-length, and all other filters
    // are the same length
    assert((filterLength_ & 1) == 0);
    assert(filterLength_ == filter1.size());
    assert(filterLength_ == filter2.size());

    // Make sure the filter is short enough that we can load
    // all the necessary surrounding data with the kernel (plus
    // an extra if we need an extension)
    assert(filterLength_-1 <= workgroupSize_);

    // Make sure we have enough padding to load the adjacent
    // values without going out of the image to the left/top
    assert(padding_ >= workgroupSize_);
}



void DecimateTripleFilterX::operator() (cl::CommandQueue& cq, 
                 ImageBuffer<cl_float>& input, 
                 ImageBuffer<cl_float>& output,
                 const std::vector<cl::Event>& waitEvents,
                 cl::Event* doneEvent)
{
    // Padding etc.
    cl::NDRange workgroupSize = {workgroupSize_, workgroupSize_};

    cl::NDRange globalSize = {
        roundWGs(output.width(), workgroupSize[0]), 
        roundWGs(output.height(), workgroupSize[1])
    }; 

    // Must have the padding the kernel expects
    assert(input.padding() == padding_);

    // Pad symmetrically if needed
    bool symmetricPadding = output.width() * 2 > input.width();

    // Input and output formats need to be exactly the same
    assert((input.width() + symmetricPadding * 2) == 2*output.width());
    assert(input.height() == output.height());
   
    // Set all the arguments

    // Input
    kernel_.setArg(0, input.buffer());
    kernel_.setArg(1, cl_uint(input.start() - symmetricPadding));
    kernel_.setArg(2, cl_uint(input.stride()));

    // Outputs
    kernel_.setArg(3, output.buffer());
    kernel_.setArg(4, cl_uint(output.start()));
    kernel_.setArg(5, cl_uint(output.stride()));
    kernel_.setArg(6, cl_uint(output.pitch()));

    // Execute
    cq.enqueueNDRangeKernel(kernel_, {0, 0},
                            globalSize, workgroupSize,
                            &waitEvents, doneEvent);
}


