#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

#define TEX_W 1024
#define TEX_H 512
#define COPY_W 32
#define COPY_H 128
static const int copy_y[] = {512, 544, 576, 608, 640, 672};
#define NUM_COPIES ((int)(sizeof(copy_y) / sizeof(copy_y[0])))

int main(int argc, char** argv) {
    int in_bounds = 0;
    int iterations = 64;
    for (int i = 1; i < argc; i++) {
        // --in-bounds runs the identical code with a legal destination, to
        // show the fault comes from the region and nothing else.
        if (strcmp(argv[i], "--in-bounds") == 0) {
            in_bounds = 1;
        } else if (strncmp(argv[i], "--iterations=", 13) == 0) {
            iterations = atoi(argv[i] + 13);
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }
    SDL_GPUDevice* device =
        SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, NULL);
    if (!device) {
        SDL_Log("SDL_CreateGPUDevice: %s", SDL_GetError());
        return 1;
    }
    SDL_Log("driver: %s", SDL_GetGPUDeviceDriver(device));

    const SDL_GPUTextureCreateInfo tex_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = TEX_W,
        .height = TEX_H,
        .layer_count_or_depth = 1,
        .num_levels = 1,
    };
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &tex_info);
    if (!texture) {
        SDL_Log("SDL_CreateGPUTexture: %s", SDL_GetError());
        return 1;
    }

    // Deliberately far larger than any copy below, so the source side is
    // never the constraint: only the destination region is out of range.
    const Uint32 transfer_size = TEX_W * TEX_H * 4;
    const SDL_GPUTransferBufferCreateInfo xfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = transfer_size,
    };
    SDL_GPUTransferBuffer* xfer =
        SDL_CreateGPUTransferBuffer(device, &xfer_info);
    if (!xfer) {
        SDL_Log("SDL_CreateGPUTransferBuffer: %s", SDL_GetError());
        return 1;
    }
    Uint8* map = SDL_MapGPUTransferBuffer(device, xfer, false);
    if (!map) {
        SDL_Log("SDL_MapGPUTransferBuffer: %s", SDL_GetError());
        return 1;
    }
    memset(map, 0xA5, transfer_size);
    SDL_UnmapGPUTransferBuffer(device, xfer);

    SDL_Log(
        "texture %dx%d, uploading %d regions of %dx%d, %s",
        TEX_W, TEX_H, NUM_COPIES, COPY_W, COPY_H,
        in_bounds ? "in bounds" : "PAST THE BOTTOM EDGE");

    for (int it = 0; it < iterations; it++) {
        SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
        if (!cmd) {
            SDL_Log("SDL_AcquireGPUCommandBuffer: %s", SDL_GetError());
            return 1;
        }
        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
        for (int i = 0; i < NUM_COPIES; i++) {
            // In bounds, the same regions are folded back inside the texture.
            const int y = in_bounds ? (copy_y[i] % (TEX_H - COPY_H)) : copy_y[i];
            const SDL_GPUTextureTransferInfo src = {
                .transfer_buffer = xfer,
                .pixels_per_row = COPY_W,
                .rows_per_layer = COPY_H,
            };
            const SDL_GPUTextureRegion dst = {
                .texture = texture,
                .x = 0,
                .y = (Uint32)y,
                .w = COPY_W,
                .h = COPY_H,
                .d = 1,
            };
            if (it == 0) {
                SDL_Log(
                    "  region %d: y %d..%d (texture height %d)%s", i, y,
                    y + COPY_H, TEX_H,
                    (y + COPY_H > TEX_H) ? "  <-- out of bounds" : "");
            }
            SDL_UploadToGPUTexture(copy, &src, &dst, false);
        }
        SDL_EndGPUCopyPass(copy);
        SDL_SubmitGPUCommandBuffer(cmd);

        // Wait for the copies to land so the fault is attributed to this
        // iteration rather than surfacing much later.
        SDL_WaitForGPUIdle(device);
        if (it % 8 == 0) {
            SDL_Log("iteration %d/%d survived", it, iterations);
        }
    }

    SDL_Log("completed %d iterations without a fault", iterations);
    SDL_ReleaseGPUTransferBuffer(device, xfer);
    SDL_ReleaseGPUTexture(device, texture);
    SDL_DestroyGPUDevice(device);
    SDL_Quit();
    return 0;
}
