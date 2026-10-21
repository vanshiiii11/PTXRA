extern "C" __global__ void divergent(const int* in, int* out, int N) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= N) return;

    int val = in[idx];
    if (val % 2 == 0) {
        if (val > 100) {
            val = val * 3 + 1;
        } else {
            val = val / 2;
        }
    } else {
        if (val < 50) {
            val = val * 7 - 3;
        } else {
            val = val + 42;
        }
    }
    out[idx] = val;
}
