extern "C" __global__ void long_lived(const float* __restrict__ in,
                                      float* __restrict__ out,
                                      int iters, float alpha, float beta) {
    int idx = blockDim.x * blockIdx.x + threadIdx.x;

    // Long-lived values defined before the hot loop
    float long_lived_alpha = alpha * 3.14159f;
    float long_lived_beta  = beta * 2.71828f;
    float long_lived_bias  = (alpha + beta) * 0.5f;

    float acc = in[idx];

    // Hot inner loop
    for (int i = 0; i < iters; ++i) {
        float x = acc * 1.001f + 0.05f;
        float y = x * 0.999f - 0.02f;
        float z = (x + y) * 0.5f;
        acc = z;
    }

    // Cold post-loop code using the long-lived values
    if (acc > 1000.0f) {
        out[idx] = acc * long_lived_alpha + long_lived_beta;
    } else {
        out[idx] = acc * long_lived_beta + long_lived_bias;
    }
}
