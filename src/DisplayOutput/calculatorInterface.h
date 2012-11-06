#ifndef CALCULATORINTERFACE_H
#define CALCULATORINTERFACE_H


#include "DisplayOutput/calculator.h"
#include "DisplayOutput/VBOBuffer.h"
#include "DisplayOutput/texture.h"
#include "MiscKernels/greyscaleToRGBA.h"
#include "MiscKernels/absToRGBA.h"
#include <opencv2/imgproc/imgproc.hpp>
#include <array>

#if defined(CL_VERSION_1_2)
    typedef cl::ImageGL GLImage;
#else
    typedef cl::Image2DGL GLImage;
#endif


// Number of subbands the DTCWT produces
const size_t numSubbands = 6;

class CalculatorInterface {

private:
    unsigned int width_, height_;

    Calculator calculator_;

    // For interop OpenGL/OpenCL

    VBOBuffers pboBuffer_;
    // Used for quickly transfering the image

    cl::CommandQueue cq_;
    // Used for upload and output conversion

    // Kernel to convert into RGBA for display
    GreyscaleToRGBA greyscaleToRGBA_;
    AbsToRGBA absToRGBA_;

    // Image input and CL interface
    GLTexture imageTexture_;
    GLImage imageTextureCL_;
    cl::Event imageTextureCLDone_;

    cl::Event glObjsReady_;

    // The input needs to be put into greyscale before display
    cl::Image2D imageGreyscale_;
    cl::Event imageGreyscaleDone_;

    // For subband displays for levels 2 and 3
    std::array<GLTexture, numSubbands> subbandTextures2_;
    std::array<GLImage, numSubbands> subbandTextures2CL_;

    std::array<GLTexture, numSubbands> subbandTextures3_;
    std::array<GLImage, numSubbands> subbandTextures3CL_;

    // Where to put the keypoints
    VBOBuffers keypointLocationBuffers;




public:

    CalculatorInterface(cl::Context& context,
                        const cl::Device& device,
                        int width, int height);

    void processImage(const void* data, size_t length);

    bool isDone();

    void updateGL(void);

    GLuint getImageTexture();
    GLuint getSubband2Texture(int subband);
    GLuint getSubband3Texture(int subband);

};


#endif

