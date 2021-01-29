#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <hexagon_standalone.h>

#include "util.h"
#include "hmx_common.h"
#include "hmx_input_dat.h"
#include "hmx_ref_output_dat.h"

#define SET_SSR_XE2()   \
{    \
    register uint32_t SSR_TEMP=0;    \
    asm( \
        "%0 = SSR\n"    \
        "%0 = setbit(%0 , #26)\n"    \
        "SSR = %0\n" \
        :: "r" (SSR_TEMP) \
		:\
    );\
};

#define SET_SSR_XE_XA(XA)   \
{    \
    register uint32_t SSR_TEMP=0;    \
    register uint32_t XA_TEMP = XA; \
    asm( \
        "%0 = SSR\n"    \
        "%0 = setbit(%0 , #31)\n"    \
        "%0 = insert(%1, #3, #27)\n"  \
        "SSR = %0\n" \
        :: "r" (SSR_TEMP), "r" (XA_TEMP) \
		:\
    );\
};

#define VTCM_SIZE_KB        (2048)
#define VTCM_BYTES_PER_KB   (1024)
#define VTCM_PAGE_SIZE_MULT (128)
void * setup_vtcm(int page_size) {

	unsigned char *vtcm_base = NULL;
	asm volatile(
            "r1 = cfgbase\n"
            "r1 = asl(r1, #5)\n"
            "r2 = #0x38\n"
            "r1 = memw_phys(r2, r1)\n"
            "%0 = asl(r1, #16)\n"
            : "=r"(vtcm_base) : : "r1", "r2" );


    void *va = (void *)vtcm_base;
    uint64_t pa = (uintptr_t)vtcm_base;
    add_translation_fixed(1, va, (void *)pa, 6, 7);
	add_translation_fixed(2, va+1024*1024, (void *)pa+1024*1024, 6, 7);

    printf("Adding %dKB VTCM Page at VA:%x PA:%llx\n", page_size*VTCM_PAGE_SIZE_MULT, (int)va, pa);
	return va;
}

unsigned int mxmem_gen_start(unsigned int dx, unsigned int dy, int input_depth, unsigned int Y0, int spatial_major) {

	unsigned int temp = 0;
	temp |= (dx & X_TILE_MASK) << X_TILE_OFFSET;
	temp |= (dy & Y_TILE_MASK) << Y_TILE_OFFSET;
	temp |= Y0;
	printf("mx start: input_depth=%d dx=%x dy=%x Y0=%x ", input_depth,  dx, dy,   Y0);
	if (spatial_major) {
		temp = spatial_major_convert(temp);
	}
	printf("start=%x %s\n", temp, (spatial_major) ? "spatial major" : "depth major");
	return temp;
}

unsigned int mxmem_gen_range( int input_depth, unsigned int dY, int spatial_major) {
	unsigned int temp = 0;
	unsigned int dC0 = ((input_depth-1) & 0x1F) & ~0x1;
	temp |= dC0;
	temp |= (Y_TILE_MASK << Y_TILE_OFFSET);
	temp |= dY;
	printf("mx range: input_depth=%d  dC0=%x dY=%x %x ", input_depth,dC0,  dY, temp);
	if (spatial_major) {
		temp = spatial_major_convert(temp);
	}
	printf("range=%x %s\n", temp, (spatial_major) ? "spatial major" : "depth major");
	return temp;
}


unsigned int mxmem_gen_range_cvt(unsigned int dY, int spatial_major) {
	unsigned int temp = 0;

	temp |= (Y_TILE_MASK << Y_TILE_OFFSET);
	temp |= dY;
	printf("mx range cvt:  dY=%x %x ",  dY, temp);

	if (spatial_major) {
		temp = spatial_major_convert(temp);
	}
	printf("range=%x %s\n", temp, (spatial_major) ? "spatial major" : "depth major");
	return temp;
}


void hmx_conv_spatial_major_deep_asm(hmx_param_t * hmx_parameters);
void hmx_conv_channel_major_deep_asm(hmx_param_t * hmx_parameters);
void hmx_conv_spatial_major_asm(hmx_param_t * p_hmx_parameters);
void hmx_conv_channel_major_asm(hmx_param_t * p_hmx_parameters);

#define RESET_PMU() __asm__ __volatile__ (" r0 = #0x48 ; trap0(#0); \n" : : : "r0","r1","r2","r3","r4","r5","r6","r7","memory")
#define DUMP_PMU() __asm__ __volatile__ (" r0 = #0x4a ; trap0(#0); \n" : : : "r0","r1","r2","r3","r4","r5","r6","r7","memory")
#define cycles() ({ unsigned long long _; asm volatile ("%0=s31:30" : "=r" (_)); _; })


hmx_param_t hmx_parameters __attribute__((aligned(8)));


long fsize(FILE *fp)

{
  long int prev_pos = ftell(fp);
  fseek(fp, 0L, SEEK_END);
  int sz = ftell(fp);
  fseek(fp, prev_pos, SEEK_SET);

  return sz;
}

int validate_output(const char *ofname)

{
  FILE *fp1 = fopen(ofname, "r");
  FILE_MEM *fp2 = fmemopen_mem(hmx_ref_output_dat, sizeof(hmx_ref_output_dat), "r");

  if (!fp1) {
    printf("Cant open file to compare: %s\n", ofname);
    return -1;
  }
  if (!fp2) {
    printf("Cant open file to compare: ref data\n");
    return -1;
  }
  if (fsize(fp1) != fsize_mem(fp2)) {
    printf("file sizes differ\n");
    return -1;
  }

  int c1, c2;
  while ((c1 = fgetc(fp1)) != EOF && (c2 = fgetc_mem(fp2)) != EOF) {
    if (c1 != c2)
      return -1;
  }

  return 0;
}

int main(int argc, const char *argv[])
{
  const char *ifname = "./hmx_input.dat";
  const char *ofname = "./hmx_asm_output.dat";
	SET_SSR_XE2();

  if (argc >= 3)
    ofname = argv[2];
  if (argc >= 2)
    ifname = argv[1];

	// Pick some memory location for inputs and outputs in VTCM
	uint8_t * vtcm_addr = (uint8_t *)setup_vtcm(VTCM_SIZE_KB / VTCM_PAGE_SIZE_MULT);

	// Image Parameters
	uint32_t X, Y ,input_depth, dx, dy, da, output_depth, output_bias, spatial_major, deep_format;

  FILE_MEM *fp = fmemopen_mem(hmx_input_dat, sizeof(hmx_input_dat), "r");
	fread_mem(&X, sizeof(uint32_t), 1, fp);
	fread_mem(&Y, sizeof(uint32_t), 1, fp);
	fread_mem(&input_depth, sizeof(uint32_t), 1, fp);
	fread_mem(&dx, sizeof(uint32_t), 1, fp);
	fread_mem(&dy, sizeof(uint32_t), 1, fp);
	fread_mem(&output_depth, sizeof(uint32_t), 1, fp);
	fread_mem(&output_bias, sizeof(uint32_t), 1, fp);
	fread_mem(&spatial_major, sizeof(uint32_t), 1, fp);
	fread_mem(&deep_format, sizeof(uint32_t), 1, fp);

	uint8_t * activations = vtcm_addr;
	int8_t *  weights = (int8_t *)vtcm_addr + 512*1024;
	uint8_t * output = vtcm_addr + (VTCM_SIZE_KB*1/16)*VTCM_BYTES_PER_KB;

	uint32_t blocks;

	int32_t x_tap = (dx & 0x7)+1;
	int32_t y_tap = (dy & 0x7)+1;

	uint32_t dY = 0;

	if (deep_format) {
		y_tap = 1;
		dY = 2048*(input_depth-32)/32;
	} else {
		dY = 2048;
		if (y_tap == 1) {
			dY = 0;
		}
	}


	int32_t weights_size = (x_tap)*(y_tap)*input_depth*output_depth;

	uint32_t single_tile = ((X_TILE_SIZE_DEF==X) && (Y_TILE_SIZE_DEF==Y)) ? 0 : 1;
	uint32_t Y0 = (uint32_t)activations & ~(2048-1);

	uint32_t * bias_addr = (uint32_t *)(vtcm_addr + (VTCM_SIZE_KB*8/16)*VTCM_BYTES_PER_KB);

	for(int32_t i = 0; i < 32; i++) {
		bias_addr[i] = output_bias;
	}

	activations = (uint8_t *)Y0;

	int32_t in_size = X*Y*input_depth;
	int32_t out_size = X*Y*output_depth;

	int8_t * tmp0 = malloc(sizeof(uint8_t)*in_size);
	int8_t * tmp1 = malloc(sizeof(uint8_t)*weights_size);
	int8_t * tmp2 = malloc(sizeof(uint8_t)*out_size);


	printf("Running HMX for input %lux%lux%lu filtered with %lux%lu. "
         "bias used: %lu tile format: %lu deep_format: %lu\n",
      X, Y, input_depth, x_tap, y_tap, output_bias, spatial_major, deep_format);


	if(fread_mem(tmp0, 1, in_size, fp)!=in_size){
		printf("Error, Unable to read from %s\n", ifname);
		fclose_mem(fp);
		return 1;
    }
	if(fread_mem(tmp1, 1, weights_size, fp)!=weights_size){
		printf("Error, Unable to read from %s\n", ifname);
		fclose_mem(fp);
		return 1;
    }
	fclose_mem(fp);

	// VTCM seems to fail with reading and writing with read/write
	for(int i = 0; i < in_size; i++) {
		activations[i] = tmp0[i];
	}
	for(int i = 0; i < weights_size; i++) {
		weights[i] = tmp1[i];
	}

	hmx_parameters.activations_start = mxmem_gen_start(dx, dy, input_depth, Y0, spatial_major);
	hmx_parameters.activations_range = mxmem_gen_range(input_depth, dY, spatial_major);
	hmx_parameters.weights_start = (uint32_t)weights;
	hmx_parameters.weights_range = weights_size;
	hmx_parameters.output = output + (0<< (spatial_major ? 0: X_TILE_OFFSET)) + (0<<Y_TILE_OFFSET);
	hmx_parameters.cvt_range = mxmem_gen_range_cvt(2048, spatial_major);
	hmx_parameters.bias = (uint32_t)(uintptr_t)bias_addr;
	hmx_parameters.x_tiles = X/X_TILE_SIZE_DEF;
	hmx_parameters.y_tiles = Y/Y_TILE_SIZE_DEF;;
	hmx_parameters.z_tiles = input_depth/Z_TILE_SIZE_DEF;;


	printf("Inputs loaded and kicking off HMX. single_tile=%lu\n", single_tile);
	long long int cyc[3] = {0};
	for(int iteration = 0; iteration < 2; iteration++) {
		RESET_PMU();
		cyc[iteration] = cycles();

		if (spatial_major) {
			if (deep_format) {
				hmx_conv_spatial_major_deep_asm(&hmx_parameters);
			} else {
				hmx_conv_spatial_major_asm(&hmx_parameters);
			}
		} else {
			if (deep_format) {
				hmx_conv_channel_major_deep_asm(&hmx_parameters);
			} else {
				hmx_conv_channel_major_asm(&hmx_parameters);
			}
		}
		cyc[iteration] = cycles() - cyc[iteration];

		DUMP_PMU();
	}

	printf("AppReported Cycles=%lli\n",cyc[1]);

	for(int i = 0; i < X*Y*output_depth; i++) {
		tmp2[i] = output[i];
	}

	write_output_image(ofname, tmp2, X, 8, output_depth, dy);
  int rval = validate_output(ofname);
  printf("%s: %s\n", argv[0], ((rval == 0) ? "PASS" : "FAIL"));

	free(tmp0);
	free(tmp1);
	free(tmp2);

	return rval;

}
