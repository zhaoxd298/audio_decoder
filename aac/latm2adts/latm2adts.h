#ifndef latm2adts_h
#define latm2adts_h
#include <stdio.h>
#include <stdint.h>

enum AudioObjectType
{
	AOT_NULL,
	// Support?                Name
	AOT_AAC_MAIN,              ///< Y                       Main
	AOT_AAC_LC,                ///< Y                       Low Complexity
	AOT_AAC_SSR,               ///< N (code in SoC repo)    Scalable Sample Rate
	AOT_AAC_LTP,               ///< Y                       Long Term Prediction
	AOT_SBR,                   ///< Y                       Spectral Band Replication
	AOT_AAC_SCALABLE,          ///< N                       Scalable
	AOT_TWINVQ,                ///< N                       Twin Vector Quantizer
	AOT_CELP,                  ///< N                       Code Excited Linear Prediction
	AOT_HVXC,                  ///< N                       Harmonic Vector eXcitation Coding
	AOT_TTSI             = 12, ///< N                       Text-To-Speech Interface
	AOT_MAINSYNTH,             ///< N                       Main Synthesis
	AOT_WAVESYNTH,             ///< N                       Wavetable Synthesis
	AOT_MIDI,                  ///< N                       General MIDI
	AOT_SAFX,                  ///< N                       Algorithmic Synthesis and Audio Effects
	AOT_ER_AAC_LC,             ///< N                       Error Resilient Low Complexity
	AOT_ER_AAC_LTP       = 19, ///< N                       Error Resilient Long Term Prediction
	AOT_ER_AAC_SCALABLE,       ///< N                       Error Resilient Scalable
	AOT_ER_TWINVQ,             ///< N                       Error Resilient Twin Vector Quantizer
	AOT_ER_BSAC,               ///< N                       Error Resilient Bit-Sliced Arithmetic Coding
	AOT_ER_AAC_LD,             ///< N                       Error Resilient Low Delay
	AOT_ER_CELP,               ///< N                       Error Resilient Code Excited Linear Prediction
	AOT_ER_HVXC,               ///< N                       Error Resilient Harmonic Vector eXcitation Coding
	AOT_ER_HILN,               ///< N                       Error Resilient Harmonic and Individual Lines plus Noise
	AOT_ER_PARAM,              ///< N                       Error Resilient Parametric
	AOT_SSC,                   ///< N                       SinuSoidal Coding
	AOT_PS,                    ///< N                       Parametric Stereo
	AOT_SURROUND,              ///< N                       MPEG Surround
	AOT_ESCAPE,                ///< Y                       Escape Value
	AOT_L1,                    ///< Y                       Layer 1
	AOT_L2,                    ///< Y                       Layer 2
	AOT_L3,                    ///< Y                       Layer 3
	AOT_DST,                   ///< N                       Direct Stream Transfer
	AOT_ALS,                   ///< Y                       Audio LosslesS
	AOT_SLS,                   ///< N                       Scalable LosslesS
	AOT_SLS_NON_CORE,          ///< N                       Scalable LosslesS (non core)
	AOT_ER_AAC_ELD,            ///< N                       Error Resilient Enhanced Low Delay
	AOT_SMR_SIMPLE,            ///< N                       Symbolic Music Representation Simple
	AOT_SMR_MAIN,              ///< N                       Symbolic Music Representation Main
	AOT_USAC_NOSBR,            ///< N                       Unified Speech and Audio Coding (no SBR)
	AOT_SAOC,                  ///< N                       Spatial Audio Object Coding
	AOT_LD_SURROUND,           ///< N                       Low Delay MPEG Surround
	AOT_USAC,                  ///< N                       Unified Speech and Audio Coding
};

enum AACOutputChannelOrder
{
	CHANNEL_ORDER_DEFAULT,
	CHANNEL_ORDER_CODED,
};

enum ChannelPosition
{
	AAC_CHANNEL_OFF   = 0,
	AAC_CHANNEL_FRONT = 1,
	AAC_CHANNEL_SIDE  = 2,
	AAC_CHANNEL_BACK  = 3,
	AAC_CHANNEL_LFE   = 4,
	AAC_CHANNEL_CC    = 5,
};

/**
 * Output configuration status
 */
enum OCStatus {
    OC_NONE,        ///< Output unconfigured
    OC_TRIAL_PCE,   ///< Output configuration under trial specified by an inband PCE
    OC_TRIAL_FRAME, ///< Output configuration under trial specified by a frame header
    OC_GLOBAL_HDR,  ///< Output configuration set in a global header but not yet locked
    OC_LOCKED,      ///< Output configuration locked in place
};

enum RawDataBlockType {
    TYPE_SCE,
    TYPE_CPE,
    TYPE_CCE,
    TYPE_LFE,
    TYPE_DSE,
    TYPE_PCE,
    TYPE_FIL,
    TYPE_END,
};

/**
 * @defgroup lavu_audio_channels Audio channels
 * @ingroup lavu_audio
 *
 * Audio channel layout utility functions
 *
 * @{
 */
enum AVChannel {
    ///< Invalid channel index
    AV_CHAN_NONE = -1,
    AV_CHAN_FRONT_LEFT,
    AV_CHAN_FRONT_RIGHT,
    AV_CHAN_FRONT_CENTER,
    AV_CHAN_LOW_FREQUENCY,
    AV_CHAN_BACK_LEFT,
    AV_CHAN_BACK_RIGHT,
    AV_CHAN_FRONT_LEFT_OF_CENTER,
    AV_CHAN_FRONT_RIGHT_OF_CENTER,
    AV_CHAN_BACK_CENTER,
    AV_CHAN_SIDE_LEFT,
    AV_CHAN_SIDE_RIGHT,
    AV_CHAN_TOP_CENTER,
    AV_CHAN_TOP_FRONT_LEFT,
    AV_CHAN_TOP_FRONT_CENTER,
    AV_CHAN_TOP_FRONT_RIGHT,
    AV_CHAN_TOP_BACK_LEFT,
    AV_CHAN_TOP_BACK_CENTER,
    AV_CHAN_TOP_BACK_RIGHT,
    /** Stereo downmix. */
    AV_CHAN_STEREO_LEFT = 29,
    /** See above. */
    AV_CHAN_STEREO_RIGHT,
    AV_CHAN_WIDE_LEFT,
    AV_CHAN_WIDE_RIGHT,
    AV_CHAN_SURROUND_DIRECT_LEFT,
    AV_CHAN_SURROUND_DIRECT_RIGHT,
    AV_CHAN_LOW_FREQUENCY_2,
    AV_CHAN_TOP_SIDE_LEFT,
    AV_CHAN_TOP_SIDE_RIGHT,
    AV_CHAN_BOTTOM_FRONT_CENTER,
    AV_CHAN_BOTTOM_FRONT_LEFT,
    AV_CHAN_BOTTOM_FRONT_RIGHT,

    /** Channel is empty can be safely skipped. */
    AV_CHAN_UNUSED = 0x200,

    /** Channel contains data, but its position is unknown. */
    AV_CHAN_UNKNOWN = 0x300,

    /**
     * Range of channels between AV_CHAN_AMBISONIC_BASE and
     * AV_CHAN_AMBISONIC_END represent Ambisonic components using the ACN system.
     *
     * Given a channel id `<i>` between AV_CHAN_AMBISONIC_BASE and
     * AV_CHAN_AMBISONIC_END (inclusive), the ACN index of the channel `<n>` is
     * `<n> = <i> - AV_CHAN_AMBISONIC_BASE`.
     *
     * @note these values are only used for AV_CHANNEL_ORDER_CUSTOM channel
     * orderings, the AV_CHANNEL_ORDER_AMBISONIC ordering orders the channels
     * implicitly by their position in the stream.
     */
    AV_CHAN_AMBISONIC_BASE = 0x400,
    // leave space for 1024 ids, which correspond to maximum order-32 harmonics,
    // which should be enough for the foreseeable use cases
    AV_CHAN_AMBISONIC_END  = 0x7ff,
};

/**
 * @defgroup channel_masks Audio channel masks
 *
 * A channel layout is a 64-bits integer with a bit set for every channel.
 * The number of bits set must be equal to the number of channels.
 * The value 0 means that the channel layout is not known.
 * @note this data structure is not powerful enough to handle channels
 * combinations that have the same channel multiple times, such as
 * dual-mono.
 *
 * @{
 */
#define AV_CH_FRONT_LEFT             (1ULL << AV_CHAN_FRONT_LEFT           )
#define AV_CH_FRONT_RIGHT            (1ULL << AV_CHAN_FRONT_RIGHT          )
#define AV_CH_FRONT_CENTER           (1ULL << AV_CHAN_FRONT_CENTER         )
#define AV_CH_LOW_FREQUENCY          (1ULL << AV_CHAN_LOW_FREQUENCY        )
#define AV_CH_BACK_LEFT              (1ULL << AV_CHAN_BACK_LEFT            )
#define AV_CH_BACK_RIGHT             (1ULL << AV_CHAN_BACK_RIGHT           )
#define AV_CH_FRONT_LEFT_OF_CENTER   (1ULL << AV_CHAN_FRONT_LEFT_OF_CENTER )
#define AV_CH_FRONT_RIGHT_OF_CENTER  (1ULL << AV_CHAN_FRONT_RIGHT_OF_CENTER)
#define AV_CH_BACK_CENTER            (1ULL << AV_CHAN_BACK_CENTER          )
#define AV_CH_SIDE_LEFT              (1ULL << AV_CHAN_SIDE_LEFT            )
#define AV_CH_SIDE_RIGHT             (1ULL << AV_CHAN_SIDE_RIGHT           )
#define AV_CH_TOP_CENTER             (1ULL << AV_CHAN_TOP_CENTER           )
#define AV_CH_TOP_FRONT_LEFT         (1ULL << AV_CHAN_TOP_FRONT_LEFT       )
#define AV_CH_TOP_FRONT_CENTER       (1ULL << AV_CHAN_TOP_FRONT_CENTER     )
#define AV_CH_TOP_FRONT_RIGHT        (1ULL << AV_CHAN_TOP_FRONT_RIGHT      )
#define AV_CH_TOP_BACK_LEFT          (1ULL << AV_CHAN_TOP_BACK_LEFT        )
#define AV_CH_TOP_BACK_CENTER        (1ULL << AV_CHAN_TOP_BACK_CENTER      )
#define AV_CH_TOP_BACK_RIGHT         (1ULL << AV_CHAN_TOP_BACK_RIGHT       )
#define AV_CH_STEREO_LEFT            (1ULL << AV_CHAN_STEREO_LEFT          )
#define AV_CH_STEREO_RIGHT           (1ULL << AV_CHAN_STEREO_RIGHT         )
#define AV_CH_WIDE_LEFT              (1ULL << AV_CHAN_WIDE_LEFT            )
#define AV_CH_WIDE_RIGHT             (1ULL << AV_CHAN_WIDE_RIGHT           )
#define AV_CH_SURROUND_DIRECT_LEFT   (1ULL << AV_CHAN_SURROUND_DIRECT_LEFT )
#define AV_CH_SURROUND_DIRECT_RIGHT  (1ULL << AV_CHAN_SURROUND_DIRECT_RIGHT)
#define AV_CH_LOW_FREQUENCY_2        (1ULL << AV_CHAN_LOW_FREQUENCY_2      )
#define AV_CH_TOP_SIDE_LEFT          (1ULL << AV_CHAN_TOP_SIDE_LEFT        )
#define AV_CH_TOP_SIDE_RIGHT         (1ULL << AV_CHAN_TOP_SIDE_RIGHT       )
#define AV_CH_BOTTOM_FRONT_CENTER    (1ULL << AV_CHAN_BOTTOM_FRONT_CENTER  )
#define AV_CH_BOTTOM_FRONT_LEFT      (1ULL << AV_CHAN_BOTTOM_FRONT_LEFT    )
#define AV_CH_BOTTOM_FRONT_RIGHT     (1ULL << AV_CHAN_BOTTOM_FRONT_RIGHT   )


/**
 * @}
 * @defgroup channel_mask_c Audio channel layouts
 * @{
 * */
#define AV_CH_LAYOUT_MONO              (AV_CH_FRONT_CENTER)
#define AV_CH_LAYOUT_STEREO            (AV_CH_FRONT_LEFT|AV_CH_FRONT_RIGHT)
#define AV_CH_LAYOUT_2POINT1           (AV_CH_LAYOUT_STEREO|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_2_1               (AV_CH_LAYOUT_STEREO|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_SURROUND          (AV_CH_LAYOUT_STEREO|AV_CH_FRONT_CENTER)
#define AV_CH_LAYOUT_3POINT1           (AV_CH_LAYOUT_SURROUND|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_4POINT0           (AV_CH_LAYOUT_SURROUND|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_4POINT1           (AV_CH_LAYOUT_4POINT0|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_2_2               (AV_CH_LAYOUT_STEREO|AV_CH_SIDE_LEFT|AV_CH_SIDE_RIGHT)
#define AV_CH_LAYOUT_QUAD              (AV_CH_LAYOUT_STEREO|AV_CH_BACK_LEFT|AV_CH_BACK_RIGHT)
#define AV_CH_LAYOUT_5POINT0           (AV_CH_LAYOUT_SURROUND|AV_CH_SIDE_LEFT|AV_CH_SIDE_RIGHT)
#define AV_CH_LAYOUT_5POINT1           (AV_CH_LAYOUT_5POINT0|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_5POINT0_BACK      (AV_CH_LAYOUT_SURROUND|AV_CH_BACK_LEFT|AV_CH_BACK_RIGHT)
#define AV_CH_LAYOUT_5POINT1_BACK      (AV_CH_LAYOUT_5POINT0_BACK|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_6POINT0           (AV_CH_LAYOUT_5POINT0|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_6POINT0_FRONT     (AV_CH_LAYOUT_2_2|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER)
#define AV_CH_LAYOUT_HEXAGONAL         (AV_CH_LAYOUT_5POINT0_BACK|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_6POINT1           (AV_CH_LAYOUT_5POINT1|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_6POINT1_BACK      (AV_CH_LAYOUT_5POINT1_BACK|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_6POINT1_FRONT     (AV_CH_LAYOUT_6POINT0_FRONT|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_7POINT0           (AV_CH_LAYOUT_5POINT0|AV_CH_BACK_LEFT|AV_CH_BACK_RIGHT)
#define AV_CH_LAYOUT_7POINT0_FRONT     (AV_CH_LAYOUT_5POINT0|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER)
#define AV_CH_LAYOUT_7POINT1           (AV_CH_LAYOUT_5POINT1|AV_CH_BACK_LEFT|AV_CH_BACK_RIGHT)
#define AV_CH_LAYOUT_7POINT1_WIDE      (AV_CH_LAYOUT_5POINT1|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER)
#define AV_CH_LAYOUT_7POINT1_WIDE_BACK (AV_CH_LAYOUT_5POINT1_BACK|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER)
#define AV_CH_LAYOUT_7POINT1_TOP_BACK  (AV_CH_LAYOUT_5POINT1_BACK|AV_CH_TOP_FRONT_LEFT|AV_CH_TOP_FRONT_RIGHT)
#define AV_CH_LAYOUT_OCTAGONAL         (AV_CH_LAYOUT_5POINT0|AV_CH_BACK_LEFT|AV_CH_BACK_CENTER|AV_CH_BACK_RIGHT)
#define AV_CH_LAYOUT_CUBE              (AV_CH_LAYOUT_QUAD|AV_CH_TOP_FRONT_LEFT|AV_CH_TOP_FRONT_RIGHT|AV_CH_TOP_BACK_LEFT|AV_CH_TOP_BACK_RIGHT)
#define AV_CH_LAYOUT_HEXADECAGONAL     (AV_CH_LAYOUT_OCTAGONAL|AV_CH_WIDE_LEFT|AV_CH_WIDE_RIGHT|AV_CH_TOP_BACK_LEFT|AV_CH_TOP_BACK_RIGHT|AV_CH_TOP_BACK_CENTER|AV_CH_TOP_FRONT_CENTER|AV_CH_TOP_FRONT_LEFT|AV_CH_TOP_FRONT_RIGHT)
#define AV_CH_LAYOUT_STEREO_DOWNMIX    (AV_CH_STEREO_LEFT|AV_CH_STEREO_RIGHT)
#define AV_CH_LAYOUT_22POINT2          (AV_CH_LAYOUT_5POINT1_BACK|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER|AV_CH_BACK_CENTER|AV_CH_LOW_FREQUENCY_2|AV_CH_SIDE_LEFT|AV_CH_SIDE_RIGHT|AV_CH_TOP_FRONT_LEFT|AV_CH_TOP_FRONT_RIGHT|AV_CH_TOP_FRONT_CENTER|AV_CH_TOP_CENTER|AV_CH_TOP_BACK_LEFT|AV_CH_TOP_BACK_RIGHT|AV_CH_TOP_SIDE_LEFT|AV_CH_TOP_SIDE_RIGHT|AV_CH_TOP_BACK_CENTER|AV_CH_BOTTOM_FRONT_CENTER|AV_CH_BOTTOM_FRONT_LEFT|AV_CH_BOTTOM_FRONT_RIGHT)

#define AV_INPUT_BUFFER_PADDING_SIZE 64
#define MAX_ELEM_ID 16
#define FFSWAP(type,a,b) do{type SWAP_tmp= b; b= a; a= SWAP_tmp;}while(0)
#define MKBETAG(a,b,c,d) ((d) | ((c) << 8) | ((b) << 16) | ((unsigned)(a) << 24))
#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#define FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))

#define LOAS_SYNC_WORD   0x2b7       ///< 11 bits LOAS sync word

#define ADTS_HEADER_SIZE 7
#define ADTS_MAX_FRAME_BYTES ((1 << 13) - 1)

typedef struct MPEG4AudioConfig {
    int object_type;
    int sampling_index;
    int sample_rate;
    int chan_config;
    int sbr; ///< -1 implicit, 1 presence
    int ext_object_type;
    int ext_sampling_index;
    int ext_sample_rate;
    int ext_chan_config;
    int channels;
    int ps;  ///< -1 implicit, 1 presence
    int frame_length_short;
} MPEG4AudioConfig;

struct elem_to_channel {
    uint64_t av_position;
    uint8_t syn_ele;
    uint8_t elem_id;
    uint8_t aac_position;
};

struct LATMContext {
    unsigned char extradata[128];
    int extradata_size;

    //AACContext aac_ctx;     ///< containing AACContext
    int initialized;        ///< initialized after a valid extradata was seen

    // parser data
    int audio_mux_version_A; ///< LATM syntax version
    int frame_length_type;   ///< 0/1 variable/fixed frame length
    int frame_length;        ///< frame length for fixed frame length

	int all_stream_same_time_framing;
	int num_sub_frames;
	int num_programs;
	int other_data_present;
	int other_data_len_bits;

    MPEG4AudioConfig *m4ac;
};

typedef struct latm_parse_s
{
	struct LATMContext latmctx;
	MPEG4AudioConfig m4ac;
} latm_parse_t;


#endif /* latm2adts_h */

