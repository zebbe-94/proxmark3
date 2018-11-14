//-----------------------------------------------------------------------------
// Copyright (C) 2018 Merlok
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// asn.1 utils
//-----------------------------------------------------------------------------

#include "asn1utils.h"
#include <mbedtls/asn1.h>
#include <mbedtls/oid.h>
#include "util.h"
#include "emv/tlv.h"
#include "emv/emv_tags.h"
#include "emv/dump.h"

int ecdsa_asn1_get_signature(uint8_t *signature, size_t signaturelen, uint8_t *rval, uint8_t *sval) {
	if (!signature || !signaturelen || !rval || !sval)
		return 1;

	int res = 0;
	unsigned char *p = signature;
	const unsigned char *end = p + signaturelen;
	size_t len;
	mbedtls_mpi xmpi;

	if ((res = mbedtls_asn1_get_tag(&p, end, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) == 0) {
		mbedtls_mpi_init(&xmpi);
		res = mbedtls_asn1_get_mpi(&p, end, &xmpi);
		if (res) {
			mbedtls_mpi_free(&xmpi);
			goto exit;
		}
		
		res = mbedtls_mpi_write_binary(&xmpi, rval, 32);
		mbedtls_mpi_free(&xmpi);
		if (res) 
			goto exit;

		mbedtls_mpi_init(&xmpi);
		res = mbedtls_asn1_get_mpi(&p, end, &xmpi);
		if (res) {
			mbedtls_mpi_free(&xmpi);
			goto exit;
		}
		
		res = mbedtls_mpi_write_binary(&xmpi, sval, 32);
		mbedtls_mpi_free(&xmpi);
		if (res) 
			goto exit;

		// check size
		if (end != p)
			return 2;
		}

exit:
	return res;
}

#define PRINT_INDENT(level) 	{for (int i = 0; i < (level); i++) fprintf(f, "   ");}

enum asn1_tag_t {
	ASN1_TAG_GENERIC,
	ASN1_TAG_BOOLEAN,
	ASN1_TAG_INTEGER,
	ASN1_TAG_STRING,
	ASN1_TAG_UTC_TIME,
	ASN1_TAG_OBJECT_ID,
};

struct asn1_tag {
	tlv_tag_t tag;
	char *name;
	enum asn1_tag_t type;
	const void *data;
};

static const struct asn1_tag asn1_tags[] = {
	// internal
	{ 0x00  , "Unknown ???" },

	// ASN.1
	{ 0x01, "BOOLEAN", ASN1_TAG_BOOLEAN },
	{ 0x02, "INTEGER", ASN1_TAG_INTEGER },
	{ 0x03, "BIT STRING" },
	{ 0x04, "OCTET STRING" },
	{ 0x05, "NULL" },
	{ 0x06, "OBJECT IDENTIFIER", ASN1_TAG_OBJECT_ID },
	{ 0x0C, "UTF8String", ASN1_TAG_STRING },
	{ 0x10, "SEQUENCE" },
	{ 0x11, "SET" },
	{ 0x13, "PrintableString", ASN1_TAG_STRING },
	{ 0x14, "T61String", ASN1_TAG_STRING },
	{ 0x16, "IA5String", ASN1_TAG_STRING },
	{ 0x17, "UTCTime", ASN1_TAG_UTC_TIME },
	{ 0x18, "GeneralizedTime", ASN1_TAG_UTC_TIME },
	{ 0x30, "SEQUENCE" },
	{ 0x31, "SET" },
	{ 0xa0, "[0]" },
	{ 0xa1, "[1]" },
	{ 0xa2, "[2]" },
	{ 0xa3, "[3]" },
	{ 0xa4, "[4]" },
	{ 0xa5, "[5]" },
};

static int asn1_sort_tag(tlv_tag_t tag) {
	return (int)(tag >= 0x100 ? tag : tag << 8);
}

static int asn1_tlv_compare(const void *a, const void *b) {
	const struct tlv *tlv = a;
	const struct asn1_tag *tag = b;

	return asn1_sort_tag(tlv->tag) - (asn1_sort_tag(tag->tag));
}

static const struct asn1_tag *asn1_get_tag(const struct tlv *tlv) {
	struct asn1_tag *tag = bsearch(tlv, asn1_tags, sizeof(asn1_tags) / sizeof(asn1_tags[0]),
			sizeof(asn1_tags[0]), asn1_tlv_compare);

	return tag ? tag : &asn1_tags[0];
}

static void asn1_tag_dump_string(const struct tlv *tlv, const struct asn1_tag *tag, FILE *f, int level){
	fprintf(f, "\tvalue: '");
	fwrite(tlv->value, 1, tlv->len, f);
	fprintf(f, "'\n");
}

static unsigned long asn1_value_integer(const struct tlv *tlv, unsigned start, unsigned end) {
	unsigned long ret = 0;
	int i;

	if (end > tlv->len * 2)
		return ret;
	if (start >= end)
		return ret;

	if (start & 1) {
		ret += tlv->value[start/2] & 0xf;
		i = start + 1;
	} else
		i = start;

	for (; i < end - 1; i += 2) {
		ret *= 10;
		ret += tlv->value[i/2] >> 4;
		ret *= 10;
		ret += tlv->value[i/2] & 0xf;
	}

	if (end & 1) {
		ret *= 10;
		ret += tlv->value[end/2] >> 4;
	}

	return ret;
}

static void asn1_tag_dump_boolean(const struct tlv *tlv, const struct asn1_tag *tag, FILE *f, int level) {
	PRINT_INDENT(level);
	if (tlv->len > 0) {
		fprintf(f, "\tvalue: %s\n", tlv->value[0]?"true":"false");
	} else {
		fprintf(f, "n/a\n");
	}
}

static void asn1_tag_dump_integer(const struct tlv *tlv, const struct asn1_tag *tag, FILE *f, int level) {
	PRINT_INDENT(level);
	fprintf(f, "\tvalue: %lu\n", asn1_value_integer(tlv, 0, tlv->len * 2));
}

static void asn1_tag_dump_object_id(const struct tlv *tlv, const struct asn1_tag *tag, FILE *f, int level) {
	PRINT_INDENT(level);
	mbedtls_asn1_buf asn1_buf;
	asn1_buf.len = tlv->len;
	asn1_buf.p = (uint8_t *)tlv->value;
	char pstr[300];
	mbedtls_oid_get_numeric_string(pstr, sizeof(pstr), &asn1_buf); 
	fprintf(f, " %s\n", pstr);
}

bool asn1_tag_dump(const struct tlv *tlv, FILE *f, int level, bool *candump) {
	if (!tlv) {
		fprintf(f, "NULL\n");
		return false;
	}

	const struct asn1_tag *tag = asn1_get_tag(tlv);

	PRINT_INDENT(level);
	fprintf(f, "--%2hx[%02zx] '%s':", tlv->tag, tlv->len, tag->name);

	switch (tag->type) {
	case ASN1_TAG_GENERIC:
		fprintf(f, "\n");
		break;
	case ASN1_TAG_STRING:
		asn1_tag_dump_string(tlv, tag, f, level);
		*candump = false;
		break;
	case ASN1_TAG_BOOLEAN:
		asn1_tag_dump_boolean(tlv, tag, f, level);
		*candump = false;
		break;
	case ASN1_TAG_INTEGER:
		asn1_tag_dump_integer(tlv, tag, f, level);
		*candump = false;
		break;
	case ASN1_TAG_UTC_TIME:
//		asn1_tag_dump_utc_time(tlv, tag, f, level);
		fprintf(f, "\n");
		break;
	case ASN1_TAG_OBJECT_ID:
		asn1_tag_dump_object_id(tlv, tag, f, level);
		*candump = false;
		break;
	};
	
	return true;
}

static bool print_cb(void *data, const struct tlv *tlv, int level, bool is_leaf) {
	bool candump = true;
	asn1_tag_dump(tlv, stdout, level, &candump);
	if (is_leaf && candump) {
		dump_buffer(tlv->value, tlv->len, stdout, level);
	}

	return true;
}

int asn1_print(uint8_t *asn1buf, size_t asn1buflen, char *indent) {
	
	struct tlvdb *t = NULL;
	t = tlvdb_parse_multi(asn1buf, asn1buflen);
	if (t) {
		tlvdb_visit(t, print_cb, NULL, 0);
		tlvdb_free(t);
	} else {
		PrintAndLogEx(ERR, "Can't parse data as TLV tree.");
		return 1;
	}
	
	return 0;
}


