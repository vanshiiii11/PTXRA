extern "C" __global__ void live_range_hole(const float* __restrict__ in,
                                           float* __restrict__ out,
                                           int mode) {
    int idx = blockDim.x * blockIdx.x + threadIdx.x;
    float val = in[idx];

    // Value with a hole: defined before the branch
    float hole_val = val * 2.5f + 1.25f;

    float result;
    if (mode > 0) {
        // 'then' branch: hole_val is used and dead
        result = hole_val * 3.0f + 0.5f;
    } else {
        // 'else' branch: hole_val is completely dead here!
        // Heavy local temporaries can reuse hole_val's physical register under precise liveness.
        float t1 = val + 10.0f;
        float t2 = t1 * 1.5f;
        float t3 = t2 - 4.0f;
        float t4 = t3 * t1;
        result = t4 + t2;
    }

    out[idx] = result;
}
