/*
 * Generated by asn1c-0.9.29 (http://lionet.info/asn1c)
 * From ASN.1 module "ITS-Container"
 * 	found in "./ASNfiles/ITS-Container_v2.asn"
 */

#ifndef	_DeltaAltitude_H_
#define	_DeltaAltitude_H_


#include "asn_application.h"

/* Including external dependencies */
#include "NativeInteger.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Dependencies */
typedef enum DeltaAltitude {
	DeltaAltitude_oneCentimeterUp	= 1,
	DeltaAltitude_oneCentimeterDown	= -1,
	DeltaAltitude_unavailable	= 12800
} e_DeltaAltitude;

/* DeltaAltitude */
typedef long	 DeltaAltitude_t;

/* Implementation */
extern asn_per_constraints_t asn_PER_type_DeltaAltitude_constr_1;
extern asn_TYPE_descriptor_t asn_DEF_DeltaAltitude;
asn_struct_free_f DeltaAltitude_free;
asn_struct_print_f DeltaAltitude_print;
asn_constr_check_f DeltaAltitude_constraint;
ber_type_decoder_f DeltaAltitude_decode_ber;
der_type_encoder_f DeltaAltitude_encode_der;
xer_type_decoder_f DeltaAltitude_decode_xer;
xer_type_encoder_f DeltaAltitude_encode_xer;
oer_type_decoder_f DeltaAltitude_decode_oer;
oer_type_encoder_f DeltaAltitude_encode_oer;
per_type_decoder_f DeltaAltitude_decode_uper;
per_type_encoder_f DeltaAltitude_encode_uper;

#ifdef __cplusplus
}
#endif

#endif	/* _DeltaAltitude_H_ */
#include "asn_internal.h"
