#include <iostream>
#include <fstream>
#include <vector>

#define __CL_ENABLE_EXCEPTIONS
#include "cl.hpp"

#include "filterer.h"

#include "cv.h"
#include "highgui.h"
#include <stdexcept>

struct dtcwtFilters {
    cl::Buffer level1h0;
    cl::Buffer level1h1;
    cl::Buffer level1hbp;

    cl::Buffer level2h0;
    cl::Buffer level2h1;
    cl::Buffer level2hbp;
};

void dtcwtTransform(std::vector<std::vector<cl::Image2D> >& output,
                     Filterer& filterer,
                     cl::Image2D& input, dtcwtFilters& filters,
                     int numLevels, int startLevel = 1)
{
    //std::vector<std::vector<cl::Image2D> > output; 

    cl::Image2D lolo;

         /*   cv::Mat im = filterer.getImage2D(input);
            cv::imshow("Output", im);
            cv::waitKey();*/



    // Go down the tree until the point where we need to start recording
    // the results
    for (int n = 1; n < startLevel; ++n) {

        if (n == 1) {

            int width = input.getImageInfo<CL_IMAGE_WIDTH>();
            int height = input.getImageInfo<CL_IMAGE_HEIGHT>();

            // Pad if an odd number of pixels
            bool padW = width & 1,
                 padH = height & 1;

            // Low-pass filter the rows...
            cl::Image2D lo = 
                filterer.createImage2D(width + padW, height);
            filterer.rowFilter(lo, input, filters.level1h0);

            // ...and the columns
            lolo = 
                filterer.createImage2D(width + padW, height + padH);
            filterer.colFilter(lolo, lo, filters.level1h0);

        } else {

            int width = lolo.getImageInfo<CL_IMAGE_WIDTH>();
            int height = lolo.getImageInfo<CL_IMAGE_HEIGHT>();

            // Pad if a non-multiple of four
            bool padW = (width % 4) != 0,
                 padH = (height % 4) != 0;

            // Low-pass filter the rows...
            cl::Image2D lo = 
                filterer.createImage2D(width / 2 + padW, height);
            filterer.rowDecimateFilter(lo, lolo, filters.level2h0, padW);

            // ...and the columns
            lolo = 
                filterer.createImage2D(width / 2 + padW, height / 2 + padH);
            filterer.colDecimateFilter(lolo, lo, filters.level2h0, padH);
            
        }

    }

    // Transform the image
    for (int n = startLevel; n < (startLevel + numLevels); ++n) {
        cl::Image2D hilo, lohi, bpbp;

        if (n == 1) {

            int width = input.getImageInfo<CL_IMAGE_WIDTH>();
            int height = input.getImageInfo<CL_IMAGE_HEIGHT>();

            // Pad if an odd number of pixels
            bool padW = width & 1,
                 padH = height & 1;

            // Low (row) - high (cols)
            cl::Image2D lo = 
                filterer.createImage2D(width + padW, height);
            filterer.rowFilter(lo, input, filters.level1h0);

            
            lohi =
                filterer.createImage2D(width + padW, height + padH);
            filterer.colFilter(lohi, lo, filters.level1h1);

            // High (row) - low (cols)
            cl::Image2D hi =
                filterer.createImage2D(width + padW, height);
            filterer.rowFilter(hi, input, filters.level1h1);

            hilo =
                filterer.createImage2D(width + padW, height + padH);
            filterer.colFilter(hilo, hi, filters.level1h0);

            // Band pass - band pass
            cl::Image2D bp =
                filterer.createImage2D(width + padW, height);
            filterer.rowFilter(bp, input, filters.level1hbp);

            bpbp =
                filterer.createImage2D(width + padW, height + padH);
            filterer.colFilter(bpbp, bp, filters.level1hbp);


            // Low - low
            lolo = 
                filterer.createImage2D(width + padW, height + padH);
            filterer.colFilter(lolo, lo, filters.level1h0);



        } else {

            /*cv::Mat im = filterer.getImage2D(lolo);
            cv::imshow("Output", im);
            cv::waitKey();*/

            int width = lolo.getImageInfo<CL_IMAGE_WIDTH>();
            int height = lolo.getImageInfo<CL_IMAGE_HEIGHT>();

            // Pad if an odd number of pixels
            bool padW = (width % 4) != 0,
                 padH = (height % 4) != 0;

            // Low (row) - high (cols)
            cl::Image2D lo = 
                filterer.createImage2D(width / 2 + padW, height);
            filterer.rowDecimateFilter(lo, lolo, filters.level2h0, padW);

            lohi =
                filterer.createImage2D(width / 2 + padW, height / 2 + padH);
            filterer.colDecimateFilter(lohi, lo, filters.level2h1, padH);


            // High (row) - low (cols)
            cl::Image2D hi =
                filterer.createImage2D(width / 2 + padW, height);
            filterer.rowDecimateFilter(hi, lolo, filters.level2h1, padW);

            hilo =
                filterer.createImage2D(width / 2 + padW, height / 2 + padH);
            filterer.colDecimateFilter(hilo, hi, filters.level2h0, padH);


            // Band pass - band pass
            cl::Image2D bp =
                filterer.createImage2D(width / 2 + padW, height);
            filterer.rowDecimateFilter(bp, lolo, filters.level2hbp, padW);

            bpbp =
                filterer.createImage2D(width / 2 + padW, height / 2 + padH);
            filterer.colDecimateFilter(bpbp, bp, filters.level2hbp, padH);


            // Low - low
            lolo = 
                filterer.createImage2D(width / 2 + padW, height / 2 + padH);
            filterer.colDecimateFilter(lolo, lo, filters.level2h0, padH);

        }

        output.push_back(std::vector<cl::Image2D>());

        int idx = n - startLevel;
        int width = hilo.getImageInfo<CL_IMAGE_WIDTH>() / 2;
        int height = hilo.getImageInfo<CL_IMAGE_HEIGHT>() / 2;

        for (int n = 0; n < 12; ++n)
            output[idx].push_back(filterer.createImage2D(width, height));


        filterer.quadToComplex(output[idx][2], output[idx][2+6],
                               output[idx][3], output[idx][3+6],
                               lohi);

        filterer.quadToComplex(output[idx][0], output[idx][0+6],
                               output[idx][5], output[idx][5+6],
                               hilo);

        filterer.quadToComplex(output[idx][1], output[idx][1+6],
                               output[idx][4], output[idx][4+6],
                               bpbp);

    }
    //return output;

}


dtcwtFilters createFilters(Filterer& filterer)
{
    const float level1h0[13] = {
       -0.0018,
             0,
        0.0223,
       -0.0469,
       -0.0482,
        0.2969,
        0.5555,
        0.2969,
       -0.0482,
       -0.0469,
        0.0223,
             0,
       -0.0018
    };

    const float level1h1[19] = {
       -0.0001,
             0,
        0.0013,
       -0.0019,
       -0.0072,
        0.0239,
        0.0556,
       -0.0517,
       -0.2998,
        0.5594,
       -0.2998,
       -0.0517,
        0.0556,
        0.0239,
       -0.0072,
       -0.0019,
        0.0013,
             0,
       -0.0001
    };

    const float level1hbp[19] = {
       -0.0004,
       -0.0006,
       -0.0001,
        0.0042,
        0.0082,
       -0.0074,
       -0.0615,
       -0.1482,
       -0.1171,
        0.6529,
       -0.1171,
       -0.1482,
       -0.0615,
       -0.0074,
        0.0082,
        0.0042,
       -0.0001,
       -0.0006,
       -0.0004
    };


    const float level2h0[14] = {
       -0.0046,
       -0.0054,
        0.0170,
        0.0238,
       -0.1067,
        0.0119,
        0.5688,
        0.7561,
        0.2753,
       -0.1172,
       -0.0389,
        0.0347,
       -0.0039,
        0.0033
    };

    const float level2h1[14] = {
       -0.0033,
       -0.0039,
       -0.0347,
       -0.0389,
        0.1172,
        0.2753,
       -0.7561,
        0.5688,
       -0.0119,
       -0.1067,
       -0.0238,
        0.0170,
        0.0054,
       -0.0046
    };

    const float level2hbp[14] = {
       -0.0028,
       -0.0004,
        0.0210,
        0.0614,
        0.1732,
       -0.0448,
       -0.8381,
        0.4368,
        0.2627,
       -0.0076,
       -0.0264,
       -0.0255,
       -0.0096,
       -0.0000
    };

    dtcwtFilters filters;
    filters.level1h0 = filterer.createBuffer(level1h0, 13);
    filters.level1h1 = filterer.createBuffer(level1h1, 19);
    filters.level1hbp = filterer.createBuffer(level1hbp, 19);

    filters.level2h0 = filterer.createBuffer(level2h0, 14);
    filters.level2h1 = filterer.createBuffer(level2h1, 14);
    filters.level2hbp = filterer.createBuffer(level2hbp, 14);

    return filters;
}


int main()
{
    try {
        std::string displays[] = {"S1", "S2", "S3", "S4", "S5", "S6"};

        // Read the image (forcing to RGB), and convert it to floats ("1")
        cv::Mat inImage = cv::imread("circle.bmp", 1);

        // Open the camera for reading
        cv::VideoCapture videoIn(0);
        videoIn.set(CV_CAP_PROP_FRAME_WIDTH, 640);
        videoIn.set(CV_CAP_PROP_FRAME_HEIGHT, 480);


        Filterer filterer;

        dtcwtFilters filters = createFilters(filterer);
        for (int n = 0; n < 6; ++n)
            cv::namedWindow(displays[n], CV_WINDOW_NORMAL);
       
        cv::Mat vidImage;
        cv::Mat outImage;

        int x = 0;
        int numLevels = 4;
        while (1) {
            videoIn >> vidImage;
            //vidImage = inImage;

            cv::Mat inputTmp;
            cv::Mat inputTmp2;
            vidImage.convertTo(inputTmp, CV_32F);
            cvtColor(inputTmp, inputTmp2, CV_RGB2GRAY);
            cv::Mat input(vidImage.size(), CV_32FC4);

            // Working in BGRA (for ref.)
            // Now, put it into 4 channels
            int fromTo[] = {0,0, 0,1, 0,2, -1,3};
            cv::mixChannels(&inputTmp2, 1, &input, 1, fromTo, 4);

            // Send to the graphics card
            cl::Image2D img(filterer.createImage2D(input));

            // Get results
            std::vector<std::vector<cl::Image2D> > results;
            dtcwtTransform(results, filterer, img, filters, numLevels, 3);

            const int l = 0;
            int width = results[l][0].getImageInfo<CL_IMAGE_WIDTH>();
            int height = results[l][0].getImageInfo<CL_IMAGE_HEIGHT>();

            cv::Mat disp(height, width, CV_32FC4);

            // Read them out
            for (int n = 0; n < 6; ++n) {
                cv::Mat re = filterer.getImage2D(results[l][n]);
                cv::Mat im = filterer.getImage2D(results[l][n+6]);
                //cv::Mat outArea = disp.colRange(n*width, (n+1)*width-1);
                cv::sqrt(re.mul(re) + im.mul(im), disp);
                cv::imshow(displays[n], disp / 64.0f);
            }

            // Display
            std::cout << "Displayed! " << x++ <<  std::endl;
            cv::waitKey(1);
        }

/*
        for (int n = 0; n < numLevels; ++n) {
            for (int m = 0; m < 6; ++m) {
                cv::Mat re = filterer.getImage2D(results[n][m]);
                cv::Mat im = filterer.getImage2D(results[n][m+6]);

                //cv::imshow("Output", filteredImage + 0.5f);
            
                cv::imshow("Output", re);
                cv::waitKey();

                cv::imshow("Output", im);
                cv::waitKey();
                cv::imwrite("out.bmp", re);

            }
        }
*/
    }
    catch (cl::Error err) {
        std::cerr << "Error: " << err.what() << "(" << err.err() << ")"
                  << std::endl;
    }

                     
    return 0;
}



