#include <cstdlib>
//#include <conv2d_NCHW.cuh>
#include <conv2d_NHWC.cuh> 
#include <transform.cuh>
#include <cstdint>

template <typename Type>
void conv2d_cutlass(const Type* X, const Type* W, Type* Y, int batchSize, int inh, int inw, int din, int dout, int wh, int ww, int padding, int stride, int dilation = 1) {
    Type* x;
    Type* w;
    Type* y;
    Type* xt;
    Type* wt;
    Type* yt;
    int xSize = inh * inw * din * batchSize;
    int wSize = wh * ww * din * dout;
    int outh = (inh + 2 * padding - wh - (wh - 1) * (dilation - 1)) / stride + 1;
    int outw = (inw + 2 * padding - ww - (ww - 1) * (dilation - 1)) / stride + 1;
    int ySize = outh * outw * dout * batchSize;
    cudaMalloc((void **)&x, xSize * sizeof(Type));
    cudaMemcpy(x, X, xSize * sizeof(Type), cudaMemcpyHostToDevice);
    cudaMalloc((void **)&xt, xSize * sizeof(Type));
    printf("XDimesions: %d, %d, %d, %d\n", batchSize, inh, inw, din);
    chwn_to_nhwc_(xt, x, batchSize, inh, inw, din);
    cudaFree(x);
    
    cudaMalloc((void **)&w, wSize * sizeof(Type));
    cudaMemcpy(w, W, wSize * sizeof(Type), cudaMemcpyHostToDevice);
    cudaMalloc((void **)&wt, wSize * sizeof(Type));
    nchw_to_nhwc_(wt, w, dout, wh, ww, din);
    cudaFree(w);
//    printf("X-Dimensions-- BatchSize: %d, Height: %d, Width: %d, Channels: %d\n", batchSize, inh, inw, din);
//    printf("W-Dimensions-- Height: %d, Width: %d, Channels: %d, Filters: %d\n", wh, ww, din, dout);
//    printf("Y-Dimensions-- BatchSize: %d, Height: %d, Width: %d, Channels: %d\n", batchSize, inh, inw, dout);



    //chwn_to_nhwc_(wt, w, dout, din, wh, ww);
 //   printf("b: %d, ih: %d, iw: %d, din: %d, dout: %d, wh: %d, ww: %d, padding: %d, stride: %d, dilation: %d\n", batchSize, inh, inw, din, dout, wh, ww, padding, stride, dilation);


    cudaMalloc((void **)&y, ySize * sizeof(Type));

    gpu::conv_fprop<Type>(
        xt, wt, y,
        batchSize, inh, inw, din, dout,
        wh, ww, padding, padding, stride, dilation
    );
//        cutlass::TensorRef<T, cutlass::layout::TensorNHWC> A,
 //       cutlass::TensorRef<T, cutlass::layout::TensorNHWC> B,
  //      cutlass::TensorRef<T, cutlass::layout::TensorNHWC> C,
   //     int b, int imageHeight, int imageWidth, int Din,
    //    int Dout, int filterHeight, int filterWidth,
     //   int paddingHeight, int paddingWidth,
      //  int stride, int dilation) {

    cudaFree(xt);
    cudaFree(wt);
    
    cudaMalloc((void **)&yt, ySize * sizeof(Type));
    nhwc_to_chwn_(yt, y, batchSize, outh, outw, dout);

    cudaMemcpy(Y, yt, ySize * sizeof(Type), cudaMemcpyDeviceToHost);

    cudaFree(yt);

}

//template void conv2d_cutlass<uint8_t>(const uint8_t* X, const uint8_t* W, uint8_t* Y, int batchSize, int inh, int inw, int din, int dout, int wh, int ww, int padding, int stride, int dilation);
//template void conv2d_cutlass<uint16_t>(const uint16_t* X, const uint16_t* W, uint16_t* Y, int batchSize, int inh, int inw, int din, int dout, int wh, int ww, int padding, int stride, int dilation); // INT8 and INT16 are not supported by all architectures
template void conv2d_cutlass<uint32_t>(const uint32_t* X, const uint32_t* W, uint32_t* Y, int batchSize, int inh, int inw, int din, int dout, int wh, int ww, int padding, int stride, int dilation);
template void conv2d_cutlass<uint64_t>(const uint64_t* X, const uint64_t* W, uint64_t* Y, int batchSize, int inh, int inw, int din, int dout, int wh, int ww, int padding, int stride, int dilation);
