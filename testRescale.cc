#include <iostream>
#include <fstream>
#include <vector>
#include <tuple>

#define __CL_ENABLE_EXCEPTIONS
#include "cl.hpp"

#include "filterer.h"
#include "clUtil.h"
#include "dtcwt.h"
#include <iomanip>

#include <ctime>

#include <stdexcept>

#include <highgui.h>


std::tuple<cl::Platform, std::vector<cl::Device>, 
           cl::Context, cl::CommandQueue> 
    initOpenCL();



void displayComplexImage(cl::CommandQueue& cq, cl::Image2D& image)
{
    const size_t width = image.getImageInfo<CL_IMAGE_WIDTH>(),
                height = image.getImageInfo<CL_IMAGE_HEIGHT>();
    float output[height][width][2];
    readImage2D(cq, &output[0][0][0], image);

    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x)
            std::cout << output[y][x][0] 
                      << "+i*" << output[y][x][1]<< "\t";

        std::cout << std::endl;
    }

    std::cout << std::endl;
}


void displayRealImage(cl::CommandQueue& cq, cl::Image2D& image)
{
    const size_t width = image.getImageInfo<CL_IMAGE_WIDTH>(),
                height = image.getImageInfo<CL_IMAGE_HEIGHT>();
    float output[height][width];
    readImage2D(cq, &output[0][0], image);

    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x)
            std::cout << output[y][x] << "\t"; 

        std::cout << std::endl;
    }

    std::cout << std::endl;
}


int main()
{
    try {

        cl::Platform platform;
        std::vector<cl::Device> devices;
        cl::Context context;
        cl::CommandQueue commandQueue; 
        std::tie(platform, devices, context, commandQueue) = initOpenCL();

        //-----------------------------------------------------------------
        // Starting test code
  
        cv::Mat input = cv::Mat::zeros(32, 4, cv::DataType<float>::type);
        input.at<float>(15,3) = 1.0f;
        cl::Image2D inImage = createImage2D(context, input);
        commandQueue.finish();

        Rescale rescale = { context, devices };

        cl::Image2D outImage
            = createImage2D(context,
                    2 * inImage.getImageInfo<CL_IMAGE_WIDTH>(),
                    2 * inImage.getImageInfo<CL_IMAGE_HEIGHT>());

        rescale(commandQueue, inImage, outImage, 2.0f);
        commandQueue.finish();

        displayRealImage(commandQueue, outImage);

    }
    catch (cl::Error err) {
        std::cerr << "Error: " << err.what() << "(" << err.err() << ")"
                  << std::endl;
    }
                     
    return 0;
}


std::tuple<cl::Platform, std::vector<cl::Device>, 
           cl::Context, cl::CommandQueue> 
initOpenCL()
{
    // Get platform, devices, command queue

    // Retrive platform information
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);

    if (platforms.size() == 0)
        throw std::runtime_error("No platforms!");

    std::vector<cl::Device> devices;
    platforms[0].getDevices(CL_DEVICE_TYPE_GPU, &devices);

    // Create a context to work in 
    cl::Context context(devices);

    // Ready the command queue on the first device to hand
    cl::CommandQueue commandQueue(context, devices[0]);

    return std::make_tuple(platforms[0], devices, context, commandQueue);
}

