/*syn:782518, cosim:628318 */
//#include "hls_target.h"
#include "wrapper.h"


//#include "Linebuffer.h"
//#include "halide_math.h"


void hls_target(
PackedStencil<dtype, DATAWIDTH, 1, 1, 1>* arg_0,//[32*124*32],output
PackedStencil<dtype, DATAWIDTH, 1, 1, 1>* arg_1,//[34*126*32],input_FM
PackedStencil<dtype, DATAWIDTH, 1, 1, 1>* arg_2,//input weight
uint8_t Ksz,
uint8_t X_n,
uint8_t Y_n,
/*remaining channel chunk in the last tile of input,
 * ChNum = (n-1)*C_SZ + r*P_C*/
uint8_t Cin_n,
uint8_t Cout_n,
bool pool)

{
#pragma HLS INTERFACE s_axilite port=return bundle=config
#pragma HLS INTERFACE m_axi depth = 8192 port=arg_0
#pragma HLS INTERFACE m_axi depth = 8192 port=arg_1
#pragma HLS INTERFACE m_axi depth = 9216 port=arg_2


 // alias the arguments
 PackedStencil<dtype, DATAWIDTH, 1, 1, 1> *_clamped = arg_1;
 PackedStencil<dtype, DATAWIDTH, 1, 1, 1> *_output = arg_0;
 //dtype *_weight = arg_2;
PackedStencil<dtype, DATAWIDTH, 1, 1, 1> *_weight = arg_2;

 struct layerPara para;
 para.Ksz = Ksz;
 para.X_n = X_n;
 para.Y_n = Y_n;
 para.Cin_n = Cin_n;
 para.Cout_n = Cout_n;
 para.pool = pool;
 para.loop_cnt = X_n * Y_n * Cin_n * Cout_n;

 //note: optimization bit shift
 para.Width = X_SZ*(X_n);
 para.Height = Y_SZ*(Y_n);

 //Total channel number
 para.Chin = Cin_n * Cin_SZ;
 para.Chout = Cout_n * Cout_SZ;

 //for kernel center shift
 para.Anchor = (Ksz - 1) >> 1;

struct tilingID iter;
iter.tilingIDc_i = 0;
iter.tilingIDc_o = 0;
iter.tilingIDx = 0;
iter.tilingIDy = 0;

 //parameter for sw pipeline
 bool flag_out = true;
 bool flag_in = true;
 bool conv_finish = false;

 int conv_cnt = 0;
 int wb_cnt = 0;

 //define the BRAM

 Doublebuffer_feature<dtype, P_CIN> feature;
 Doublebuffer_weight<dtype, P_CIN, P_COUT> weight;
 Doublebuffer_psum<dtype, P_COUT> psum;


 //define the stream
 hls::stream<PackedStencil<dtype, DATAWIDTH, 1, 1, 1>> unpadded_feature("in_fm");
 hls::stream<PackedStencil<dtype, DATAWIDTH, 1, 1, 1>> padded_feature("out_fm");
 hls::stream<PackedStencil<dtype, P_CIN, 1, 1, 1>> padded_feature_short("out_short_fm");
#pragma HLS STREAM variable=unpadded_feature depth=1
#pragma HLS STREAM variable=padded_feature depth=1
#pragma HLS STREAM variable=padded_feature_short depth=1

 hls::stream<PackedStencil<dtype, DATAWIDTH, 1, 1, 1>> weight_long("in_wt");
 hls::stream<PackedStencil<dtype, P_CIN*P_COUT, 1, 1, 1>> weight_short("out_wt");
#pragma HLS STREAM variable=weight_long depth=1
#pragma HLS STREAM variable=weight_short depth=1
#pragma HLS RESOURCE variable=weight_short core=FIFO_LUTRAM

 hls::stream<PackedStencil<dtype, P_CIN, 1, 1, 1>> feature_stream;
 hls::stream<PackedStencil<dtype, P_CIN, P_COUT, 1, 1>> weight_stream;
#pragma HLS STREAM variable=feature_stream depth=1
//#pragma HLS_RESOURCE variable=feature_stream core=FIFO_LUTRAM
#pragma HLS STREAM variable=weight_stream depth=1
#pragma HLS RESOURCE variable=weight_stream core=FIFO_LUTRAM

 hls::stream<PackedStencil<dtype, P_COUT, 1, 1, 1>> psum_stream;
#pragma HLS STREAM variable=psum_stream depth=1
//#pragma HLS_RESOURCE variable=feature_stream core=FIFO_LUTRAM

 hls::stream<PackedStencil<dtype, DATAWIDTH, 1, 1, 1>> output_long("long_ofm");
 hls::stream<PackedStencil<dtype, P_COUT, 1, 1, 1>> output_short("short_ofm");
 hls::stream<PackedStencil<dtype, P_COUT, 1, 1, 1>> relu_short("relu");
#pragma HLS STREAM variable=output_long depth=1
#pragma HLS STREAM variable=output_short depth=1
#pragma HLS STREAM variable=relu_short depth=1


#pragma HLS dataflow
DMA_feature_tiling_wrapper(_clamped, unpadded_feature, para);
feature_pad(unpadded_feature, padded_feature, para);
datawidth_convert_feature(padded_feature, padded_feature_short, para);

DMA_weight_tiling_wrapper(_weight, weight_long, para);
datawidth_convert_weight(weight_long, weight_short, para);

read_input(padded_feature_short, feature, feature_stream, para);
read_weight(weight_short, weight, weight_stream, para);

compute(feature_stream, weight_stream, psum_stream, para);

write_back(relu_short, psum, psum_stream, para);

ReLU(relu_short, output_short, para);
datawidth_convert_output(output_short, output_long, para);
DMA_output_tiling_wrapper(_output, output_long, para);

}
