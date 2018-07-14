SRC_DIRS ?= ./src

Celt := ./celt/pitch.c ./celt/celt_lpc.c ./celt/entenc.c

Opus := ./src/opus_encoder.c

SILK_SOURCES = \
./silk/CNG.c \
./silk/code_signs.c \
./silk/enc_API.c \
./silk/encode_indices.c \
./silk/encode_pulses.c \
./silk/gain_quant.c \
./silk/interpolate.c \
./silk/LP_variable_cutoff.c \
./silk/NLSF_decode.c \
./silk/NSQ.c \
./silk/NSQ_del_dec.c \
./silk/shell_coder.c \
./silk/tables_gain.c \
./silk/tables_LTP.c \
./silk/tables_NLSF_CB_NB_MB.c \
./silk/tables_NLSF_CB_WB.c \
./silk/tables_other.c \
./silk/tables_pitch_lag.c \
./silk/tables_pulses_per_block.c \
./silk/VAD.c \
./silk/control_audio_bandwidth.c \
./silk/quant_LTP_gains.c \
./silk/VQ_WMat_EC.c \
./silk/HP_variable_cutoff.c \
./silk/NLSF_encode.c \
./silk/NLSF_VQ.c \
./silk/NLSF_unpack.c \
./silk/NLSF_del_dec_quant.c \
./silk/process_NLSFs.c \
./silk/check_control_input.c \
./silk/control_SNR.c \
./silk/init_encoder.c \
./silk/control_codec.c \
./silk/A2NLSF.c \
./silk/ana_filt_bank_1.c \
./silk/biquad_alt.c \
./silk/bwexpander_32.c \
./silk/bwexpander.c \
./silk/inner_prod_aligned.c \
./silk/lin2log.c \
./silk/log2lin.c \
./silk/LPC_analysis_filter.c \
./silk/LPC_inv_pred_gain.c \
./silk/table_LSF_cos.c \
./silk/NLSF2A.c \
./silk/NLSF_stabilize.c \
./silk/NLSF_VQ_weights_laroia.c \
./silk/pitch_est_tables.c \
./silk/sigm_Q15.c \
./silk/sort.c \
./silk/sum_sqr_shift.c \
./silk/LPC_fit.c


SILK_SOURCES += ./silk/resampler.c \
./silk/resampler_down2.c \
./silk/resampler_private_down_FIR.c \
./silk/resampler_private_AR2.c \
./silk/resampler_rom.c


SILK_SOURCES_FIXED = \
./silk/fixed/LTP_analysis_filter_FIX.c \
./silk/fixed/LTP_scale_ctrl_FIX.c \
./silk/fixed/corrMatrix_FIX.c \
./silk/fixed/encode_frame_FIX.c \
./silk/fixed/find_LPC_FIX.c \
./silk/fixed/find_LTP_FIX.c \
./silk/fixed/find_pitch_lags_FIX.c \
./silk/fixed/find_pred_coefs_FIX.c \
./silk/fixed/noise_shape_analysis_FIX.c \
./silk/fixed/process_gains_FIX.c \
./silk/fixed/regularize_correlations_FIX.c \
./silk/fixed/residual_energy16_FIX.c \
./silk/fixed/residual_energy_FIX.c \
./silk/fixed/warped_autocorrelation_FIX.c \
./silk/fixed/apply_sine_window_FIX.c \
./silk/fixed/autocorr_FIX.c \
./silk/fixed/burg_modified_FIX.c \
./silk/fixed/k2a_FIX.c \
./silk/fixed/k2a_Q16_FIX.c \
./silk/fixed/pitch_analysis_core_FIX.c \
./silk/fixed/vector_ops_FIX.c \
./silk/fixed/schur64_FIX.c \
./silk/fixed/schur_FIX.c

SRCS := opusenc.c $(Celt) $(Opus) $(SILK_SOURCES) $(SILK_SOURCES_FIXED) # $(shell find $(SRC_DIRS) -name "*.c")
OBJS := $(addsuffix .o,$(basename $(SRCS)))
DEPS := $(OBJS:.o=.d)

INC_DIRS := ./src/ ./include/ ./celt/ ./silk/ ./silk/fixed/ # $(shell find $(SRC_DIRS) -type d)

INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CFLAGS ?= -DHAVE_CONFIG_H -DLOCALE_NOT_USED # -DOVERRIDE_OPUS_ALLOC -DOVERRIDE_OPUS_FREE #-D'opus_alloc(x)=NULL' -D'opus_free(x)=NULL'
CPPFLAGS ?= $(INC_FLAGS)

opusenc: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ $(LOADLIBES) $(LDLIBS)

.PHONY: clean
clean:
	$(RM) $(TARGET) $(OBJS) $(DEPS)

-include $(DEPS)

