#include <iostream>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <tuple>

#define __CL_ENABLE_EXCEPTIONS
#include "CL/cl.hpp"

#include "util/clUtil.h"

#include "BufferTrials/PadX/padX.h"
#include "BufferTrials/DecimateTripleFilterX/decimateTripleFilterX.h"

#include "../referenceImplementation.h"

// Check that the FilterX kernel actually does what it should

std::tuple<Eigen::ArrayXXf, Eigen::ArrayXXf, Eigen::ArrayXXf> 
    decimateConvolveRowsGPU(const Eigen::ArrayXXf& in, 
            const std::vector<float>& filter0, bool swapOutputs0,
            const std::vector<float>& filter1, bool swapOutputs1,
            const std::vector<float>& filter2, bool swapOutputs2);


// Runs both with the same parameters, and displays output if failure,
// returning true.
bool compareImplementations(const Eigen::ArrayXXf& in, 
            const std::vector<float>& filter0, bool swapOutputs0,
            const std::vector<float>& filter1, bool swapOutputs1,
            const std::vector<float>& filter2, bool swapOutputs2,
                            float tolerance);


int main()
{

    std::vector<float> filter0(14, 0.0);
    for (int n = 0; n < filter0.size(); ++n)
        filter0[n] = n + 1;

    std::vector<float> filter1(14, 0.0);
    for (int n = 0; n < filter1.size(); ++n)
        filter1[n] = n + 1;

    std::vector<float> filter2(14, 0.0);
    filter2[5] = 1.f;

    
    Eigen::ArrayXXf X1(5,16);
    X1.setRandom();

    float eps = 1.e-5;

    if (compareImplementations(X1, filter0, false, 
                                   filter1, false, 
                                   filter2, false, 
                               eps)) {
        std::cerr << "Failed no extension, no swapped outputs" 
                  << std::endl;
        return -1;
    }

    if (compareImplementations(X1, filter0, false, 
                                   filter1, true, 
                                   filter2, true, 
                               eps)) {
        std::cerr << "Failed no extension, swapped output trees" 
                  << std::endl;
        return -1;
    }
    
    Eigen::ArrayXXf X2(5,18);
    X2.setRandom();

    if (compareImplementations(X2, filter0, false, 
                                   filter1, false, 
                                   filter2, false, 
                               eps)) {
        std::cerr << "Failed extension, no swapped outputs" 
                  << std::endl;
        return -1;
    }

    if (compareImplementations(X2, filter0, false, 
                                   filter1, true, 
                                   filter2, true, 
                               eps)) {
        std::cerr << "Failed extension, swapped output trees" 
                  << std::endl;
        return -1;
    }

    // No failures if we reached here
    return 0;
 
}







bool compareImplementations(const Eigen::ArrayXXf& in, 
            const std::vector<float>& filter0, bool swapOutputs0,
            const std::vector<float>& filter1, bool swapOutputs1,
            const std::vector<float>& filter2, bool swapOutputs2,
                            float tolerance)
{
    Eigen::ArrayXXf gpuResults[3];

    // Try with reference and GPU implementations
    std::tie(gpuResults[0], gpuResults[1], gpuResults[2]) 
        = decimateConvolveRowsGPU(in, 
            filter0, swapOutputs0,
            filter1, swapOutputs1,
            filter2, swapOutputs2);

    std::vector<const std::vector<float>*> filters
        = {&filter0, &filter1, &filter2};

    std::vector<bool> swapOutputs
        = {swapOutputs0, swapOutputs1, swapOutputs2};

    for (int n = 0; n < 3; ++n) {
   
        Eigen::ArrayXXf refResult
            = decimateConvolveRows(in, *filters[n], swapOutputs[n]);

        // Check the maximum error is within tolerances
        float biggestDiscrepancy = 
            (refResult - gpuResults[n]).abs().maxCoeff();

        // No problem if within tolerances
        if (biggestDiscrepancy >= tolerance) {

            // Display diagnostics:
            std::cerr << "Output " << n << "\n" 
                      << "Input:\n"
                      << in << "\n\n"
                      << "Should have been:\n"
                      << refResult << "\n\n"
                      << "Was:\n"
                      << gpuResults[n] << std::endl;

            return true;
        }

    }

    return false;
}






std::tuple<Eigen::ArrayXXf, Eigen::ArrayXXf, Eigen::ArrayXXf> 
    decimateConvolveRowsGPU(const Eigen::ArrayXXf& in, 
            const std::vector<float>& filter0, bool swapOutputs0,
            const std::vector<float>& filter1, bool swapOutputs1,
            const std::vector<float>& filter2, bool swapOutputs2)
{
    typedef
    Eigen::Array<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>
        Array;
        

    // Copy into an array where we set up the backing, so should
    // know the data format!
    std::vector<float> inValues(in.rows() * in.cols());
    Eigen::Map<Array> input(&inValues[0], in.rows(), in.cols());
    input = in;


    const size_t outputWidth = (in.cols() + in.cols() % 4) / 2;
    std::vector<float> outValues0(in.rows() * outputWidth);
    Eigen::Map<Array> output0(&outValues0[0], in.rows(), outputWidth);

    std::vector<float> outValues1(in.rows() * outputWidth);
    Eigen::Map<Array> output1(&outValues1[0], in.rows(), outputWidth);

    std::vector<float> outValues2(in.rows() * outputWidth);
    Eigen::Map<Array> output2(&outValues2[0], in.rows(), outputWidth);

    try {

        CLContext context;

        // Ready the command queue on the first device to hand
        cl::CommandQueue cq(context.context, context.devices[0]);

        PadX padX(context.context, context.devices);
        DecimateTripleFilterX 
            decimateFilterX(context.context, context.devices, 
                            filter0, swapOutputs0,
                            filter1, swapOutputs1,
                            filter2, swapOutputs2);

  
        const size_t width = in.cols(), height = in.rows(),
                     padding = 16, alignment = 32;

        ImageBuffer input(context.context, CL_MEM_READ_WRITE,
                          width, height, padding, alignment); 

        ImageBuffer outputImage0(context.context, CL_MEM_READ_WRITE,
                           outputWidth, height, padding, alignment); 

        ImageBuffer outputImage1(context.context, CL_MEM_READ_WRITE,
                           outputWidth, height, padding, alignment); 

        ImageBuffer outputImage2(context.context, CL_MEM_READ_WRITE,
                           outputWidth, height, padding, alignment); 

        // Upload the data
        cq.enqueueWriteBufferRect(input.buffer(), CL_TRUE,
              makeCLSizeT<3>({sizeof(float) * input.padding(),
                              input.padding(), 0}),
              makeCLSizeT<3>({0,0,0}),
              makeCLSizeT<3>({input.width() * sizeof(float),
                              input.height(), 1}),
              input.stride() * sizeof(float), 0,
              0, 0,
              &inValues[0]);

        // Try the filter
        padX(cq, input);
        decimateFilterX(cq, input, outputImage0, outputImage1, outputImage2);

        // Download the data
        cq.enqueueReadBufferRect(outputImage0.buffer(), CL_TRUE,
              makeCLSizeT<3>({sizeof(float) * outputImage0.padding(),
                             outputImage0.padding(), 0}),
              makeCLSizeT<3>({0,0,0}),
              makeCLSizeT<3>({outputImage0.width() * sizeof(float),
                             outputImage0.height(), 1}),
              outputImage0.stride() * sizeof(float), 0,
              0, 0,
              &outValues0[0]);

        cq.enqueueReadBufferRect(outputImage1.buffer(), CL_TRUE,
              makeCLSizeT<3>({sizeof(float) * outputImage1.padding(),
                             outputImage1.padding(), 0}),
              makeCLSizeT<3>({0,0,0}),
              makeCLSizeT<3>({outputImage1.width() * sizeof(float),
                             outputImage1.height(), 1}),
              outputImage1.stride() * sizeof(float), 0,
              0, 0,
              &outValues1[0]);

        cq.enqueueReadBufferRect(outputImage2.buffer(), CL_TRUE,
              makeCLSizeT<3>({sizeof(float) * outputImage2.padding(),
                             outputImage2.padding(), 0}),
              makeCLSizeT<3>({0,0,0}),
              makeCLSizeT<3>({outputImage2.width() * sizeof(float),
                             outputImage2.height(), 1}),
              outputImage2.stride() * sizeof(float), 0,
              0, 0,
              &outValues2[0]);


    }
    catch (cl::Error err) {
        std::cerr << "Error: " << err.what() << "(" << err.err() << ")"
                  << std::endl;
        throw;
    }

    Array out0 = output0;
    Array out1 = output1;
    Array out2 = output2;

    return std::make_tuple(out0, out1, out2);
}



