#define TILE_SIZE 16

extern "C" __global__ void matmul(const float* A, const float* B, float* C, int width) {
    __shared__ float sA[TILE_SIZE][TILE_SIZE];
    __shared__ float sB[TILE_SIZE][TILE_SIZE];

    int tx = threadIdx.x, ty = threadIdx.y;
    int row = blockIdx.y * TILE_SIZE + ty;
    int col = blockIdx.x * TILE_SIZE + tx;

    float sum = 0.0f;
    for (int t = 0; t < width / TILE_SIZE; ++t) {
        sA[ty][tx] = A[row * width + t * TILE_SIZE + tx];
        sB[ty][tx] = B[(t * TILE_SIZE + ty) * width + col];
        __syncthreads();

        for (int k = 0; k < TILE_SIZE; ++k) {
            sum += sA[ty][k] * sB[k][tx];
        }
        __syncthreads();
    }

    if (row < width && col < width) {
        C[row * width + col] = sum;
    }
}
