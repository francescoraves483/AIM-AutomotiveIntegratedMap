/*
 * Generated by asn1c-0.9.29 (http://lionet.info/asn1c)
 * From ASN.1 module "CAM-PDU-Descriptions"
 * 	found in "ASNfiles/ETSI CAM v1.4.1.asn"
 */

#include "EmergencyContainer.h"

asn_TYPE_member_t asn_MBR_EmergencyContainer_1[] = {
	{ ATF_NOFLAGS, 0, offsetof(struct EmergencyContainer, lightBarSirenInUse),
		(ASN_TAG_CLASS_CONTEXT | (0 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_LightBarSirenInUse,
		0,
		{ 0, 0, 0 },
		0, 0, /* No default value */
		"lightBarSirenInUse"
		},
	{ ATF_POINTER, 2, offsetof(struct EmergencyContainer, incidentIndication),
		(ASN_TAG_CLASS_CONTEXT | (1 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_CauseCode,
		0,
		{ 0, 0, 0 },
		0, 0, /* No default value */
		"incidentIndication"
		},
	{ ATF_POINTER, 1, offsetof(struct EmergencyContainer, emergencyPriority),
		(ASN_TAG_CLASS_CONTEXT | (2 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_EmergencyPriority,
		0,
		{ 0, 0, 0 },
		0, 0, /* No default value */
		"emergencyPriority"
		},
};
static const int asn_MAP_EmergencyContainer_oms_1[] = { 1, 2 };
static const ber_tlv_tag_t asn_DEF_EmergencyContainer_tags_1[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (16 << 2))
};
static const asn_TYPE_tag2member_t asn_MAP_EmergencyContainer_tag2el_1[] = {
    { (ASN_TAG_CLASS_CONTEXT | (0 << 2)), 0, 0, 0 }, /* lightBarSirenInUse */
    { (ASN_TAG_CLASS_CONTEXT | (1 << 2)), 1, 0, 0 }, /* incidentIndication */
    { (ASN_TAG_CLASS_CONTEXT | (2 << 2)), 2, 0, 0 } /* emergencyPriority */
};
asn_SEQUENCE_specifics_t asn_SPC_EmergencyContainer_specs_1 = {
	sizeof(struct EmergencyContainer),
	offsetof(struct EmergencyContainer, _asn_ctx),
	asn_MAP_EmergencyContainer_tag2el_1,
	3,	/* Count of tags in the map */
	asn_MAP_EmergencyContainer_oms_1,	/* Optional members */
	2, 0,	/* Root/Additions */
	-1,	/* First extension addition */
};
asn_TYPE_descriptor_t asn_DEF_EmergencyContainer = {
	"EmergencyContainer",
	"EmergencyContainer",
	&asn_OP_SEQUENCE,
	asn_DEF_EmergencyContainer_tags_1,
	sizeof(asn_DEF_EmergencyContainer_tags_1)
		/sizeof(asn_DEF_EmergencyContainer_tags_1[0]), /* 1 */
	asn_DEF_EmergencyContainer_tags_1,	/* Same as above */
	sizeof(asn_DEF_EmergencyContainer_tags_1)
		/sizeof(asn_DEF_EmergencyContainer_tags_1[0]), /* 1 */
	{ 0, 0, SEQUENCE_constraint },
	asn_MBR_EmergencyContainer_1,
	3,	/* Elements count */
	&asn_SPC_EmergencyContainer_specs_1	/* Additional specs */
};

