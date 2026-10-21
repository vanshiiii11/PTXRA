extern "C" __global__ void stencil_2d(const float* in, float* out, int width, int height) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x > 0 && x < width - 1 && y > 0 && y < height - 1) {
        int idx = y * width + x;
        float center = in[idx];
        float left   = in[idx - 1];
        float right  = in[idx + 1];
        float top    = in[idx - width];
        float bottom = in[idx + width];

        out[idx] = 0.2f * (center + left + right + top + bottom);
    }
}
