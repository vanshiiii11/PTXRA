extern "C" __global__ void nested_loops(const float* __restrict__ A,
                                        const float* __restrict__ B,
                                        float* __restrict__ C,
                                        int N, int M) {
    int idx = blockDim.x * blockIdx.x + threadIdx.x;
    if (idx >= N) return;

    float sum0 = 0.0f;
    float sum1 = 1.0f;
    float sum2 = 2.0f;
    float sum3 = 3.0f;

    for (int i = 0; i < N; ++i) {
        float a = A[i];
        for (int j = 0; j < M; ++j) {
            float b = B[j];
            sum0 += a * b;
            sum1 += a + b;
            sum2 = sum2 * 1.01f + b;
            sum3 = sum3 * 0.99f + a;
        }
    }

    C[idx] = sum0 + sum1 + sum2 + sum3;
}
