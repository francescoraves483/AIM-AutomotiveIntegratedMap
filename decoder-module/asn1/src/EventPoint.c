/*
 * Generated by asn1c-0.9.29 (http://lionet.info/asn1c)
 * From ASN.1 module "ITS-Container"
 * 	found in "ASNfiles/ITS-Container_v2.asn"
 */

#include "EventPoint.h"

asn_TYPE_member_t asn_MBR_EventPoint_1[] = {
	{ ATF_NOFLAGS, 0, offsetof(struct EventPoint, eventPosition),
		(ASN_TAG_CLASS_CONTEXT | (0 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_DeltaReferencePosition,
		0,
		{ 0, 0, 0 },
		0, 0, /* No default value */
		"eventPosition"
		},
	{ ATF_POINTER, 1, offsetof(struct EventPoint, eventDeltaTime),
		(ASN_TAG_CLASS_CONTEXT | (1 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_PathDeltaTime,
		0,
		{ 0, 0, 0 },
		0, 0, /* No default value */
		"eventDeltaTime"
		},
	{ ATF_NOFLAGS, 0, offsetof(struct EventPoint, informationQuality),
		(ASN_TAG_CLASS_CONTEXT | (2 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_InformationQuality,
		0,
		{ 0, 0, 0 },
		0, 0, /* No default value */
		"informationQuality"
		},
};
static const int asn_MAP_EventPoint_oms_1[] = { 1 };
static const ber_tlv_tag_t asn_DEF_EventPoint_tags_1[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (16 << 2))
};
static const asn_TYPE_tag2member_t asn_MAP_EventPoint_tag2el_1[] = {
    { (ASN_TAG_CLASS_CONTEXT | (0 << 2)), 0, 0, 0 }, /* eventPosition */
    { (ASN_TAG_CLASS_CONTEXT | (1 << 2)), 1, 0, 0 }, /* eventDeltaTime */
    { (ASN_TAG_CLASS_CONTEXT | (2 << 2)), 2, 0, 0 } /* informationQuality */
};
asn_SEQUENCE_specifics_t asn_SPC_EventPoint_specs_1 = {
	sizeof(struct EventPoint),
	offsetof(struct EventPoint, _asn_ctx),
	asn_MAP_EventPoint_tag2el_1,
	3,	/* Count of tags in the map */
	asn_MAP_EventPoint_oms_1,	/* Optional members */
	1, 0,	/* Root/Additions */
	-1,	/* First extension addition */
};
asn_TYPE_descriptor_t asn_DEF_EventPoint = {
	"EventPoint",
	"EventPoint",
	&asn_OP_SEQUENCE,
	asn_DEF_EventPoint_tags_1,
	sizeof(asn_DEF_EventPoint_tags_1)
		/sizeof(asn_DEF_EventPoint_tags_1[0]), /* 1 */
	asn_DEF_EventPoint_tags_1,	/* Same as above */
	sizeof(asn_DEF_EventPoint_tags_1)
		/sizeof(asn_DEF_EventPoint_tags_1[0]), /* 1 */
	{ 0, 0, SEQUENCE_constraint },
	asn_MBR_EventPoint_1,
	3,	/* Elements count */
	&asn_SPC_EventPoint_specs_1	/* Additional specs */
};

