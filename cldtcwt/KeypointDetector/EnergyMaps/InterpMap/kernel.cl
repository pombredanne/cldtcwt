// Copyright (C) 2013 Timothy Gale
typedef float2 Complex;

// Load a rectangular region from a floating-point image
void readImageRegionToShared(const __global float2* input,
                unsigned int stride,
                unsigned int padding,
                uint2 inSize,
                int2 regionStart,
                int2 regionSize, 
                __local volatile Complex* output)
{
    // Take a region of regionSize, and load it into local memory with the
    // while workgroup.  Memory is laid out reading along the direction of
    // x.

    // We'll extract a rectangle the size of a workgroup each time.

    // The position within the workgroup
    int2 localPos = (int2) (get_local_id(0), get_local_id(1));

    // Loop over the rectangles
    for (int x = 0; x < regionSize.x; x += get_local_size(0)) {
        for (int y = 0; y < regionSize.y; y += get_local_size(1)) {

            int2 readPosOffset = (int2) (x,y) + localPos;

            bool inRegion = all(readPosOffset < regionSize);

            // Make sure we are still in the rectangular region asked for
            if (inRegion) {
                
                int2 pos = regionStart + readPosOffset;
                
                bool inImage = all((int2) (0, 0) <= pos) 
                                & all(pos < convert_int2(inSize));

                output[readPosOffset.y * regionSize.x + readPosOffset.x]
                    = inImage? 
                        input[pos.x + pos.y * stride]
                      : (float2) (0.f, 0.f);
            }

        }
    }
}



Complex differentiate(Complex y0, Complex y1, Complex y2,
                      float angularFreq, Complex expjw)
{
    // For the sequence y0, y1, y2, with angular frequency angularFreq
    // centre frequency and exp(j*angularFreq) work out the derivative of 
    // the cubic interpolation around y1.

    Complex result = (Complex) angularFreq * 
                        (Complex) (-y1.y, y1.x);

    result -= (Complex) 0.5f * 
        (Complex) (y0.x * expjw.x - y0.y * expjw.y,
                   y0.x * expjw.y + y0.y * expjw.x);

    result += (Complex) 0.5f * 
        (Complex) (y2.x * expjw.x + y2.y * expjw.y,
                   y2.x * -expjw.y + y2.y * expjw.x);


    return result;
}



typedef struct {
    float s00;
    float s01;
    float s11;
} Matrix2x2ConjSymmetric;


void clearMatrix2x2ConjSymmetric(__local Matrix2x2ConjSymmetric* M)
{
    M->s00 = 0.f;
    M->s01 = 0.f;
    M->s11 = 0.f;
}




void accumMatrix2x2ConjSymmetric(__private Matrix2x2ConjSymmetric* M,
                                 __local Matrix2x2ConjSymmetric* input,
                                 float gain)
{
    M->s00 += gain * input->s00;
    M->s01 += gain * input->s01;
    M->s11 += gain * input->s11;
}



float2 eigsMatrix2x2ConjSymmetric(__private const Matrix2x2ConjSymmetric* M)
{
    float s = native_sqrt((M->s00 + M->s11) * (M->s00 + M->s11)
                   - 4 * (M->s00 * M->s11 - M->s01 * M->s01));

    return (float2) (0.5 * (M->s00 + M->s11 - s),
                     0.5 * (M->s00 + M->s11 + s));
}


void addDiff(__local Matrix2x2ConjSymmetric* M, Complex Dx, Complex Dy)
{
    M->s00 += dot(Dx, Dx);
    M->s01 += dot(Dx, Dy); // real(conj(Dx) * Dy)
    M->s11 += dot(Dy, Dy);
}


// Parameters: WG_SIZE_X, WG_SIZE_Y need to be set for the work group size.
// POS_LEN should be the number of floats to make the output structure.
__kernel __attribute__((reqd_work_group_size(WG_SIZE_X, WG_SIZE_Y, 1)))
void interpMap(const __global float2* sb,
               const unsigned int sbStart,
               const unsigned int sbPitch,
               const unsigned int sbStride,
               const unsigned int sbPadding,
               const unsigned int sbWidth,
               const unsigned int sbHeight,
               __write_only image2d_t output)
{

    // Angular frequency for each subband, x and y
    const float2 angularFreq[6] = {
        (float2) (-3*tan(M_PI_F/12.f),-3) * M_PI_F / 2.15f, 
        (float2) (-sqrt(5.f), -sqrt(5.f)) * M_PI_F / 2.15f, 
        (float2) (-3, -3*tan(M_PI_F/12.f)) * M_PI_F / 2.15f, 
        (float2) (-3,  3*tan(M_PI_F/12.f)) * M_PI_F / 2.15f, 
        (float2) (-sqrt(5.f), sqrt(5.f)) * M_PI_F / 2.15f, 
        (float2) (-3*tan(M_PI_F/12.f), 3) * M_PI_F / 2.15f 
    };


    // For each subband, and for x and y directions, the complex exponent 
    // of j*angular frequency
    const Complex expjAngFreq[6][2] = { 
        {(Complex) (cos(angularFreq[0].x), sin(angularFreq[0].x)),
         (Complex) (cos(angularFreq[0].y), sin(angularFreq[0].y))},
        {(Complex) (cos(angularFreq[1].x), sin(angularFreq[1].x)),
         (Complex) (cos(angularFreq[1].y), sin(angularFreq[1].y))},
        {(Complex) (cos(angularFreq[2].x), sin(angularFreq[2].x)),
         (Complex) (cos(angularFreq[2].y), sin(angularFreq[2].y))},
        {(Complex) (cos(angularFreq[3].x), sin(angularFreq[3].x)),
         (Complex) (cos(angularFreq[3].y), sin(angularFreq[3].y))},
        {(Complex) (cos(angularFreq[4].x), sin(angularFreq[4].x)),
         (Complex) (cos(angularFreq[4].y), sin(angularFreq[4].y))},
        {(Complex) (cos(angularFreq[5].x), sin(angularFreq[5].x)),
         (Complex) (cos(angularFreq[5].y), sin(angularFreq[5].y))},
    };


    int2 g = (int2) (get_global_id(0), get_global_id(1));
    int2 l = (int2) (get_local_id(0), get_local_id(1));

    // Storage for the subband values
    __local volatile Complex sbVals[WG_SIZE_Y+4][WG_SIZE_X+4];

    // Holds the covariance-like matrix for working out the distance
    // the subbands change for a small change in position
    __local Matrix2x2ConjSymmetric Q[WG_SIZE_Y+2][WG_SIZE_X+2];
    __local float energy [WG_SIZE_Y+2][WG_SIZE_X+2];

    for (int a = 0; a < 2; ++a) {
        for (int b = 0; b < 2; ++b) {

            int2 p = l + (int2) (a * WG_SIZE_X, b * WG_SIZE_Y);
            
            if (all(p < (int2) (WG_SIZE_X+2, WG_SIZE_Y+2))) {

                clearMatrix2x2ConjSymmetric(&Q[p.y][p.x]);

                energy[p.y][p.x] = 0.f;

            }
        }
    }

    int2 regionStart = (int2)
        (get_group_id(0) * get_local_size(0) - 2,
         get_group_id(1) * get_local_size(1) - 2);

    int2 regionSize = (int2) (WG_SIZE_X+4, WG_SIZE_Y+4);

    // For each subband
    for (int n = 0; n < 6; ++n) {

        readImageRegionToShared(sb + sbStart + n * sbPitch, 
                                sbStride, sbPadding, 
                                (uint2) (sbWidth, sbHeight),
                                regionStart, regionSize, 
                                &sbVals[0][0]);

        // Make sure we don't start using values until they're valid
        barrier(CLK_LOCAL_MEM_FENCE);


        for (int a = 0; a < 2; ++a) {
            for (int b = 0; b < 2; ++b) {

                int2 p = l + (int2) (a * WG_SIZE_X, b * WG_SIZE_Y);
                
                if (all(p < (int2) (WG_SIZE_X+2, WG_SIZE_Y+2))) {

                    energy[p.y][p.x] += dot(sbVals[p.y+1][p.x+1],
                                            sbVals[p.y+1][p.x+1]);

                    Complex Dx = differentiate(sbVals[p.y+1][p.x  ],
                                               sbVals[p.y+1][p.x+1],
                                               sbVals[p.y+1][p.x+2],
                                               angularFreq[n].x,
                                               expjAngFreq[n][0]);

                    Complex Dy = differentiate(sbVals[p.y  ][p.x+1],
                                               sbVals[p.y+1][p.x+1],
                                               sbVals[p.y+2][p.x+1],
                                               angularFreq[n].y,
                                               expjAngFreq[n][1]);

                    // Append to the differences matrix
                    addDiff(&Q[p.y][p.x], Dx, Dy);

                }
            }
        }

        // Make sure values aren't overwritten while they might still be
        // being used
        barrier(CLK_LOCAL_MEM_FENCE);

    }

    if (all(g < get_image_dim(output))) {

        Matrix2x2ConjSymmetric R = {0, 0, 0};

        float h[] = {0.5, 1, 0.5};

        for (int n = 0; n < 3; ++n) 
            for (int m = 0; m < 3; ++m)
                accumMatrix2x2ConjSymmetric(&R, &Q[l.y+n][l.x+m], 
                                                h[n] * h[m]);

        // Calculate eigenvalues: how quickly does the distance
        // to the interpolated subbands change in the most and least 
        // variable directions
        float2 eigs = eigsMatrix2x2ConjSymmetric(&R);

        float result = eigs.s0 * eigs.s0 / (eigs.s1 + 0.01f);

        write_imagef(output, g, result);
    }
                 
}

