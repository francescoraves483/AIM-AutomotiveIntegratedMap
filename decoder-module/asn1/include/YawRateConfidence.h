/*
 * Generated by asn1c-0.9.29 (http://lionet.info/asn1c)
 * From ASN.1 module "ITS-Container"
 * 	found in "./ASNfiles/ITS-Container_v2.asn"
 */

#ifndef	_YawRateConfidence_H_
#define	_YawRateConfidence_H_


#include "asn_application.h"

/* Including external dependencies */
#include "NativeEnumerated.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Dependencies */
typedef enum YawRateConfidence {
	YawRateConfidence_degSec_000_01	= 0,
	YawRateConfidence_degSec_000_05	= 1,
	YawRateConfidence_degSec_000_10	= 2,
	YawRateConfidence_degSec_001_00	= 3,
	YawRateConfidence_degSec_005_00	= 4,
	YawRateConfidence_degSec_010_00	= 5,
	YawRateConfidence_degSec_100_00	= 6,
	YawRateConfidence_outOfRange	= 7,
	YawRateConfidence_unavailable	= 8
} e_YawRateConfidence;

/* YawRateConfidence */
typedef long	 YawRateConfidence_t;

/* Implementation */
extern asn_per_constraints_t asn_PER_type_YawRateConfidence_constr_1;
extern asn_TYPE_descriptor_t asn_DEF_YawRateConfidence;
extern const asn_INTEGER_specifics_t asn_SPC_YawRateConfidence_specs_1;
asn_struct_free_f YawRateConfidence_free;
asn_struct_print_f YawRateConfidence_print;
asn_constr_check_f YawRateConfidence_constraint;
ber_type_decoder_f YawRateConfidence_decode_ber;
der_type_encoder_f YawRateConfidence_encode_der;
xer_type_decoder_f YawRateConfidence_decode_xer;
xer_type_encoder_f YawRateConfidence_encode_xer;
oer_type_decoder_f YawRateConfidence_decode_oer;
oer_type_encoder_f YawRateConfidence_encode_oer;
per_type_decoder_f YawRateConfidence_decode_uper;
per_type_encoder_f YawRateConfidence_encode_uper;

#ifdef __cplusplus
}
#endif

#endif	/* _YawRateConfidence_H_ */
#include "asn_internal.h"
