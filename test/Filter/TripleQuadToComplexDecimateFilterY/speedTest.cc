// Copyright (C) 2013 Timothy Gale
#include <iostream>
#include <vector>

#define __CL_ENABLE_EXCEPTIONS
#include "CL/cl.hpp"

#include "util/clUtil.h"

#include <chrono>
typedef std::chrono::duration<double>
    DurationSeconds;

#include "Filter/TripleQuadToComplexDecimateFilterY/tripleQ2cDecimateFilterY.h"
#include "Filter/PadY/padY.h"


#include <sstream>

template <typename T>
T readStr(const char* string)
{
    std::istringstream s(string);

    T result;
    s >> result;
    return result;
}


int main(int argc, const char* argv[])
{
    // Measure the speed of the decimated x-filtering operation on a 720p
    // image with a 14-long filter.  Average over 1000 runs.


    size_t width = 1280, height = 720, len = 14, numIterations = 1000;
    bool pad = true;

    // First and second arguments: width and height
    if (argc > 2) {
        width = readStr<size_t>(argv[1]);
        height = readStr<size_t>(argv[2]);
    }

    // Third argument: filter length
    if (argc > 3) {
        len = readStr<size_t>(argv[3]);
    }

    // Fourth argument: number of iterations
    if (argc > 4) {
        numIterations = readStr<size_t>(argv[4]);
    }

    // Fifth argument: whether to perform padding
    if (argc > 5) {
        pad = readStr<bool>(argv[5]);
    }



    try {

        CLContext context;

        // Ready the command queue on the first device to hand
        cl::CommandQueue cq(context.context, context.devices[0]);

        std::vector<float> filter(len, 0.0);
        TripleQuadToComplexDecimateFilterY 
            filterY(context.context, context.devices, 
                    filter, false,
                    filter, true,
                    filter, true);
        PadY padY(context.context, context.devices);
  
        const size_t padding = 16, alignment = 2*16;

        // Create input and output buffers
        ImageBuffer<cl_float> input(context.context, CL_MEM_READ_WRITE,
                                    width, height, padding, alignment, 3);

        // Allow individual indexing
        ImageBuffer<cl_float> inputs[3]
            = {{input, 0}, {input, 1}, {input, 2}};

        ImageBuffer<Complex<cl_float>> sb(context.context,
                       CL_MEM_READ_WRITE,
                       width / 2, height / 4, 0, 1, 
                       6);

        {
            // Run, timing
            auto start = std::chrono::system_clock::now();

            for (int n = 0; n < numIterations; ++n) {
                if (pad) {
                    padY(cq, inputs[0]);
                    padY(cq, inputs[1]);
                    padY(cq, inputs[2]);
                }
                filterY(cq, input, sb);
            }

            cq.finish();
            auto end = std::chrono::system_clock::now();

            // Work out what the difference between these is
            double t = DurationSeconds(end - start).count();

            std::cout << "TripleQuadToComplexDecimateFilterY: " 
                    << (t / numIterations * 1000) << " ms" << std::endl;
        }
    }
    catch (cl::Error err) {
        std::cerr << "Error: " << err.what() << "(" << err.err() << ")"
                  << std::endl;
    }
                     
    return 0;
}


