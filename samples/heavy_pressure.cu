extern "C" __global__ void heavy_pressure(const float* in, float* out, int iters) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    float v0 = in[idx + 0];
    float v1 = in[idx + 1];
    float v2 = in[idx + 2];
    float v3 = in[idx + 3];
    float v4 = in[idx + 4];
    float v5 = in[idx + 5];
    float v6 = in[idx + 6];
    float v7 = in[idx + 7];
    float v8 = in[idx + 8];
    float v9 = in[idx + 9];
    float v10 = in[idx + 10];
    float v11 = in[idx + 11];
    float v12 = in[idx + 12];
    float v13 = in[idx + 13];
    float v14 = in[idx + 14];
    float v15 = in[idx + 15];

    for (int i = 0; i < iters; ++i) {
        v0 += v1 * 1.01f;
        v1 += v2 * 1.02f;
        v2 += v3 * 1.03f;
        v3 += v4 * 1.04f;
        v4 += v5 * 1.05f;
        v5 += v6 * 1.06f;
        v6 += v7 * 1.07f;
        v7 += v8 * 1.08f;
        v8 += v9 * 1.09f;
        v9 += v10 * 1.10f;
        v10 += v11 * 1.11f;
        v11 += v12 * 1.12f;
        v12 += v13 * 1.13f;
        v13 += v14 * 1.14f;
        v14 += v15 * 1.15f;
        v15 += v0 * 1.16f;
    }

    out[idx] = v0 + v1 + v2 + v3 + v4 + v5 + v6 + v7 +
               v8 + v9 + v10 + v11 + v12 + v13 + v14 + v15;
}
