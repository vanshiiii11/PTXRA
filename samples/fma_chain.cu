extern "C" __global__ void fma_chain(const float* __restrict__ in,
                                      float* __restrict__ out) {
    int idx = blockDim.x * blockIdx.x + threadIdx.x;
    const float* ptr = in + idx * 48;

    // 48 simultaneously live values
    float v0  = ptr[0],  v1  = ptr[1],  v2  = ptr[2],  v3  = ptr[3];
    float v4  = ptr[4],  v5  = ptr[5],  v6  = ptr[6],  v7  = ptr[7];
    float v8  = ptr[8],  v9  = ptr[9],  v10 = ptr[10], v11 = ptr[11];
    float v12 = ptr[12], v13 = ptr[13], v14 = ptr[14], v15 = ptr[15];
    float v16 = ptr[16], v17 = ptr[17], v18 = ptr[18], v19 = ptr[19];
    float v20 = ptr[20], v21 = ptr[21], v22 = ptr[22], v23 = ptr[23];
    float v24 = ptr[24], v25 = ptr[25], v26 = ptr[26], v27 = ptr[27];
    float v28 = ptr[28], v29 = ptr[29], v30 = ptr[30], v31 = ptr[31];
    float v32 = ptr[32], v33 = ptr[33], v34 = ptr[34], v35 = ptr[35];
    float v36 = ptr[36], v37 = ptr[37], v38 = ptr[38], v39 = ptr[39];
    float v40 = ptr[40], v41 = ptr[41], v42 = ptr[42], v43 = ptr[43];
    float v44 = ptr[44], v45 = ptr[45], v46 = ptr[46], v47 = ptr[47];

    // Interdependent unrolled FMA chain keeping all 48 live simultaneously
    float acc = 0.0f;
    acc = acc * 1.01f + v0;   acc = acc * 1.01f + v1;
    acc = acc * 1.01f + v2;   acc = acc * 1.01f + v3;
    acc = acc * 1.01f + v4;   acc = acc * 1.01f + v5;
    acc = acc * 1.01f + v6;   acc = acc * 1.01f + v7;
    acc = acc * 1.01f + v8;   acc = acc * 1.01f + v9;
    acc = acc * 1.01f + v10;  acc = acc * 1.01f + v11;
    acc = acc * 1.01f + v12;  acc = acc * 1.01f + v13;
    acc = acc * 1.01f + v14;  acc = acc * 1.01f + v15;
    acc = acc * 1.01f + v16;  acc = acc * 1.01f + v17;
    acc = acc * 1.01f + v18;  acc = acc * 1.01f + v19;
    acc = acc * 1.01f + v20;  acc = acc * 1.01f + v21;
    acc = acc * 1.01f + v22;  acc = acc * 1.01f + v23;
    acc = acc * 1.01f + v24;  acc = acc * 1.01f + v25;
    acc = acc * 1.01f + v26;  acc = acc * 1.01f + v27;
    acc = acc * 1.01f + v28;  acc = acc * 1.01f + v29;
    acc = acc * 1.01f + v30;  acc = acc * 1.01f + v31;
    acc = acc * 1.01f + v32;  acc = acc * 1.01f + v33;
    acc = acc * 1.01f + v34;  acc = acc * 1.01f + v35;
    acc = acc * 1.01f + v36;  acc = acc * 1.01f + v37;
    acc = acc * 1.01f + v38;  acc = acc * 1.01f + v39;
    acc = acc * 1.01f + v40;  acc = acc * 1.01f + v41;
    acc = acc * 1.01f + v42;  acc = acc * 1.01f + v43;
    acc = acc * 1.01f + v44;  acc = acc * 1.01f + v45;
    acc = acc * 1.01f + v46;  acc = acc * 1.01f + v47;

    out[idx] = acc;
}
