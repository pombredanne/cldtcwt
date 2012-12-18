#include <iostream>
#include <fstream>
#include <vector>
#include <stdexcept>

#define __CL_ENABLE_EXCEPTIONS
#include "CL/cl.hpp"

#include "util/clUtil.h"

#include <sys/timeb.h>

#include "filter.h"




int main()
{
    try {

        CLContext context;

        // Ready the command queue on the first device to hand
        cl::CommandQueue cq(context.context, context.devices[0]);

        std::vector<float> filter(13, 0.0);
        FilterX filterX(context.context, context.devices, filter);

        //-----------------------------------------------------------------
        // Starting test code
  
        const size_t width = 1280, height = 720, 
                     padding = 8;
        const size_t stride = width + 2*padding;

        std::vector<float> zeros(stride*(2*padding+height), 0);


        ImageBuffer input = {
            cl::Buffer(context.context,
                       CL_MEM_COPY_HOST_PTR,
                       stride*(2*padding+height)*sizeof(float),
                       &zeros[0]),
            width, padding, stride, height
        };


        ImageBuffer output = {
            cl::Buffer(context.context,
                       CL_MEM_COPY_HOST_PTR,
                       stride*(2*padding+height)*sizeof(float),
                       &zeros[0]),
            width, padding, stride, height
        };



        timeb start, end;
        const int numFrames = 1000;
        ftime(&start);

        for (int n = 0; n < numFrames; ++n) {
            filterX(cq, input, output);
        }

        cq.finish();
        ftime(&end);

        // Work out what the difference between these is
        double t = end.time - start.time 
                 + 0.001 * (end.millitm - start.millitm);

        std::cout << (t / numFrames * 1000) << " ms" << std::endl;

    }
    catch (cl::Error err) {
        std::cerr << "Error: " << err.what() << "(" << err.err() << ")"
                  << std::endl;
    }
                     
    return 0;
}


