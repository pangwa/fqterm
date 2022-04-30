#include "ssh_diffie-hellman.h"
#include "ssh_session.h"
#include "ssh_crypto_common.h"
#include "curve25519/crypto_scalarmult.h"
#include <stdlib.h>
#include <openssl/rand.h>

static const char DH_G16_SHA512[] = "diffie-hellman-group16-sha512";
static const char DH_G14_SHA256[] = "diffie-hellman-group14-sha256";
static const char DH_G14_SHA1[] = "diffie-hellman-group14-sha1";
static const char DH_G1_SHA1[] = "diffie-hellman-group1-sha1";
static const char DH_CURVE25519_SHA256[] = "curve25519-sha256@libssh.org";

static const int g = 2;

/* groups: https://tools.ietf.org/html/rfc3526 */

static const unsigned char prime_group1[]={
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34,
	0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
	0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74,
	0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22,
	0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
	0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B,
	0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37,
	0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
	0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6,
	0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B,
	0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
	0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5,
	0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6,
	0x49, 0x28, 0x66, 0x51, 0xEC, 0xE6, 0x53, 0x81,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static const unsigned char prime_group14[] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34,
	0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
	0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74,
	0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22,
	0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
	0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B,
	0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37,
	0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
	0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6,
	0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B,
	0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
	0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5,
	0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6,
	0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D,
	0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05,
	0x98, 0xDA, 0x48, 0x36, 0x1C, 0x55, 0xD3, 0x9A,
	0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
	0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96,
	0x1C, 0x62, 0xF3, 0x56, 0x20, 0x85, 0x52, 0xBB,
	0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D,
	0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04,
	0xF1, 0x74, 0x6C, 0x08, 0xCA, 0x18, 0x21, 0x7C,
	0x32, 0x90, 0x5E, 0x46, 0x2E, 0x36, 0xCE, 0x3B,
	0xE3, 0x9E, 0x77, 0x2C, 0x18, 0x0E, 0x86, 0x03,
	0x9B, 0x27, 0x83, 0xA2, 0xEC, 0x07, 0xA2, 0x8F,
	0xB5, 0xC5, 0x5D, 0xF0, 0x6F, 0x4C, 0x52, 0xC9,
	0xDE, 0x2B, 0xCB, 0xF6, 0x95, 0x58, 0x17, 0x18,
	0x39, 0x95, 0x49, 0x7C, 0xEA, 0x95, 0x6A, 0xE5,
	0x15, 0xD2, 0x26, 0x18, 0x98, 0xFA, 0x05, 0x10,
	0x15, 0x72, 0x8E, 0x5A, 0x8A, 0xAC, 0xAA, 0x68,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static const unsigned char prime_group16[] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC9, 0x0F, 0xDA, 0xA2,
	0x21, 0x68, 0xC2, 0x34, 0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
	0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74, 0x02, 0x0B, 0xBE, 0xA6,
	0x3B, 0x13, 0x9B, 0x22, 0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
	0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B, 0x30, 0x2B, 0x0A, 0x6D,
	0xF2, 0x5F, 0x14, 0x37, 0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
	0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6, 0xF4, 0x4C, 0x42, 0xE9,
	0xA6, 0x37, 0xED, 0x6B, 0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
	0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5, 0xAE, 0x9F, 0x24, 0x11,
	0x7C, 0x4B, 0x1F, 0xE6, 0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D,
	0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05, 0x98, 0xDA, 0x48, 0x36,
	0x1C, 0x55, 0xD3, 0x9A, 0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
	0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96, 0x1C, 0x62, 0xF3, 0x56,
	0x20, 0x85, 0x52, 0xBB, 0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D,
	0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04, 0xF1, 0x74, 0x6C, 0x08,
	0xCA, 0x18, 0x21, 0x7C, 0x32, 0x90, 0x5E, 0x46, 0x2E, 0x36, 0xCE, 0x3B,
	0xE3, 0x9E, 0x77, 0x2C, 0x18, 0x0E, 0x86, 0x03, 0x9B, 0x27, 0x83, 0xA2,
	0xEC, 0x07, 0xA2, 0x8F, 0xB5, 0xC5, 0x5D, 0xF0, 0x6F, 0x4C, 0x52, 0xC9,
	0xDE, 0x2B, 0xCB, 0xF6, 0x95, 0x58, 0x17, 0x18, 0x39, 0x95, 0x49, 0x7C,
	0xEA, 0x95, 0x6A, 0xE5, 0x15, 0xD2, 0x26, 0x18, 0x98, 0xFA, 0x05, 0x10,
	0x15, 0x72, 0x8E, 0x5A, 0x8A, 0xAA, 0xC4, 0x2D, 0xAD, 0x33, 0x17, 0x0D,
	0x04, 0x50, 0x7A, 0x33, 0xA8, 0x55, 0x21, 0xAB, 0xDF, 0x1C, 0xBA, 0x64,
	0xEC, 0xFB, 0x85, 0x04, 0x58, 0xDB, 0xEF, 0x0A, 0x8A, 0xEA, 0x71, 0x57,
	0x5D, 0x06, 0x0C, 0x7D, 0xB3, 0x97, 0x0F, 0x85, 0xA6, 0xE1, 0xE4, 0xC7,
	0xAB, 0xF5, 0xAE, 0x8C, 0xDB, 0x09, 0x33, 0xD7, 0x1E, 0x8C, 0x94, 0xE0,
	0x4A, 0x25, 0x61, 0x9D, 0xCE, 0xE3, 0xD2, 0x26, 0x1A, 0xD2, 0xEE, 0x6B,
	0xF1, 0x2F, 0xFA, 0x06, 0xD9, 0x8A, 0x08, 0x64, 0xD8, 0x76, 0x02, 0x73,
	0x3E, 0xC8, 0x6A, 0x64, 0x52, 0x1F, 0x2B, 0x18, 0x17, 0x7B, 0x20, 0x0C,
	0xBB, 0xE1, 0x17, 0x57, 0x7A, 0x61, 0x5D, 0x6C, 0x77, 0x09, 0x88, 0xC0,
	0xBA, 0xD9, 0x46, 0xE2, 0x08, 0xE2, 0x4F, 0xA0, 0x74, 0xE5, 0xAB, 0x31,
	0x43, 0xDB, 0x5B, 0xFC, 0xE0, 0xFD, 0x10, 0x8E, 0x4B, 0x82, 0xD1, 0x20,
	0xA9, 0x21, 0x08, 0x01, 0x1A, 0x72, 0x3C, 0x12, 0xA7, 0x87, 0xE6, 0xD7,
	0x88, 0x71, 0x9A, 0x10, 0xBD, 0xBA, 0x5B, 0x26, 0x99, 0xC3, 0x27, 0x18,
	0x6A, 0xF4, 0xE2, 0x3C, 0x1A, 0x94, 0x68, 0x34, 0xB6, 0x15, 0x0B, 0xDA,
	0x25, 0x83, 0xE9, 0xCA, 0x2A, 0xD4, 0x4C, 0xE8, 0xDB, 0xBB, 0xC2, 0xDB,
	0x04, 0xDE, 0x8E, 0xF9, 0x2E, 0x8E, 0xFC, 0x14, 0x1F, 0xBE, 0xCA, 0xA6,
	0x28, 0x7C, 0x59, 0x47, 0x4E, 0x6B, 0xC0, 0x5D, 0x99, 0xB2, 0x96, 0x4F,
	0xA0, 0x90, 0xC3, 0xA2, 0x23, 0x3B, 0xA1, 0x86, 0x51, 0x5B, 0xE7, 0xED,
	0x1F, 0x61, 0x29, 0x70, 0xCE, 0xE2, 0xD7, 0xAF, 0xB8, 0x1B, 0xDD, 0x76,
	0x21, 0x70, 0x48, 0x1C, 0xD0, 0x06, 0x91, 0x27, 0xD5, 0xB0, 0x5A, 0xA9,
	0x93, 0xB4, 0xEA, 0x98, 0x8D, 0x8F, 0xDD, 0xC1, 0x86, 0xFF, 0xB7, 0xDC,
	0x90, 0xA6, 0xC0, 0x8F, 0x4D, 0xF4, 0x35, 0xC9, 0x34, 0x06, 0x31, 0x99,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void
ssh_dh_free(SSH_DH *dh)
{
	if (dh->name != DH_CURVE25519_SHA256) {
		BN_free(dh->priv.dh.g);
		BN_free(dh->priv.dh.p);
		BN_clear_free(dh->priv.dh.bn_x);
		BN_CTX_free(dh->priv.dh.ctx);
	}
	ssh_md_ctx_free(dh->digest.mdctx);
	free(dh->mpint_e);
	free(dh->secret);
	free(dh);
}

static void dh_compute(SSH_DH *dh)
{
	dh->priv.dh.bn_x = BN_new();

	/* FIXME: check return value and throw error */
	BN_rand_range(dh->priv.dh.bn_x, dh->priv.dh.p);

	BIGNUM *bn_e = BN_new();
	BN_mod_exp(bn_e, dh->priv.dh.g, dh->priv.dh.bn_x, dh->priv.dh.p, dh->priv.dh.ctx);
	dh->e_len = BN_bn2mpi(bn_e, NULL);
	dh->mpint_e = (unsigned char*)malloc(dh->e_len);
	BN_bn2mpi(bn_e, dh->mpint_e);
	BN_free(bn_e);
}

static SSH_DH *create_dh()
{
	SSH_DH *dh = (SSH_DH*)malloc(sizeof(SSH_DH));
	dh->priv.dh.ctx = BN_CTX_new();
	dh->priv.dh.g = BN_new();
	dh->priv.dh.p = BN_new();
	return dh;
}

static SSH_DH *
ssh_dh_group1_sha1(void)
{
	SSH_DH *dh = create_dh();
	dh->name = DH_G1_SHA1;
	dh->digest = (evp_md_t) {
		.mdctx = ssh_md_ctx_new(),
		.md = EVP_sha1(),
		.hashlen = SHA_DIGEST_LENGTH
	};
	BN_set_word(dh->priv.dh.g, g);
	BN_bin2bn(prime_group1, 128, dh->priv.dh.p);
	dh_compute(dh);
	return dh;
}

static SSH_DH *
ssh_dh_group14_sha1(void)
{
	SSH_DH *dh = create_dh();
	dh->name = DH_G14_SHA1;
	dh->digest = (evp_md_t) {
		.mdctx = ssh_md_ctx_new(),
		.md = EVP_sha1(),
		.hashlen = SHA_DIGEST_LENGTH
	};
	BN_set_word(dh->priv.dh.g, g);
	BN_bin2bn(prime_group14, 256, dh->priv.dh.p);
	dh_compute(dh);
	return dh;
}

static SSH_DH *
ssh_dh_group14_sha256(void)
{
	SSH_DH *dh = create_dh();
	dh->name = DH_G14_SHA256;
	dh->digest = (evp_md_t) {
		.mdctx = ssh_md_ctx_new(),
		.md = EVP_sha256(),
		.hashlen = SHA256_DIGEST_LENGTH
	};
	BN_set_word(dh->priv.dh.g, g);
	BN_bin2bn(prime_group14, 256, dh->priv.dh.p);
	dh_compute(dh);
	return dh;
}

static SSH_DH *
ssh_dh_group16_sha512(void)
{
	SSH_DH *dh = create_dh();
	dh->name = DH_G16_SHA512;
	dh->digest = (evp_md_t) {
		.mdctx = ssh_md_ctx_new(),
		.md = EVP_sha512(),
		.hashlen = SHA512_DIGEST_LENGTH
	};
	BN_set_word(dh->priv.dh.g, g);
	BN_bin2bn(prime_group16, 512, dh->priv.dh.p);
	dh_compute(dh);
	return dh;
}

static void ecdh_compute(SSH_DH *dh)
{
	/* FIXME: check return value and throw error */
	RAND_bytes(dh->priv.privkey, 32);

	dh->priv.privkey[0] &= 248;
	dh->priv.privkey[31] &= 127;
	dh->priv.privkey[31] |= 64;

	dh->e_len = 36;
	dh->mpint_e = (unsigned char*)malloc(36);
	dh->mpint_e[0] = dh->mpint_e[1] = dh->mpint_e[2] = 0;
	dh->mpint_e[3] = 32;
	crypto_scalarmult_base(dh->mpint_e + 4, dh->priv.privkey);
}

static SSH_DH *
ssh_dh_curve25519_sha256(void)
{
	SSH_DH *dh = (SSH_DH*)malloc(sizeof(SSH_DH));
	dh->name = DH_CURVE25519_SHA256;
	dh->digest = (evp_md_t) {
		.mdctx = ssh_md_ctx_new(),
		.md = EVP_sha256(),
		.hashlen = SHA256_DIGEST_LENGTH
	};
	ecdh_compute(dh);
	return dh;
}

void
ssh_dh_hash(SSH_DH *dh, const unsigned char *in, unsigned char *out, size_t n)
{
	EVP_DigestInit_ex(dh->digest.mdctx, dh->digest.md, NULL);
	EVP_DigestUpdate(dh->digest.mdctx, in, n);
	EVP_DigestFinal_ex(dh->digest.mdctx, out, NULL);
}

static int ecdh_compute_secret(SSH_DH *dh, const unsigned char *f_bin, int f_len)
{
	unsigned char s[32];

	if (f_len != 32)
		return -1;
	crypto_scalarmult(s, dh->priv.privkey, f_bin);
	BIGNUM *bn_k = BN_new();
	BN_bin2bn(s, 32, bn_k);
	dh->secret_len = BN_bn2mpi(bn_k, NULL);
	dh->secret = (unsigned char*)malloc(dh->secret_len);
	BN_bn2mpi(bn_k, dh->secret);
	BN_clear_free(bn_k);
	return 0;
}

int ssh_dh_compute_secret(SSH_DH *dh, const unsigned char *f_bin, int f_len)
{
	if (dh->name == DH_CURVE25519_SHA256)
		return ecdh_compute_secret(dh, f_bin, f_len);

	BIGNUM *bn_f = BN_new();
	if (bn_f == NULL || BN_bin2bn(f_bin, f_len, bn_f) == NULL)
		return -1;
	BIGNUM *bn_k = BN_new();
	BN_mod_exp(bn_k, bn_f, dh->priv.dh.bn_x, dh->priv.dh.p, dh->priv.dh.ctx);
	dh->secret_len = BN_bn2mpi(bn_k, NULL);
	dh->secret = (unsigned char*)malloc(dh->secret_len);
	BN_bn2mpi(bn_k, dh->secret);
	BN_clear_free(bn_k);
	return 0;
}

/* SSH SHA2: https://tools.ietf.org/html/draft-ietf-curdle-ssh-modp-dh-sha2-09 */

struct
{
	const char *name;
	NEW_DH f;
} all_dh[] = {
	{ DH_CURVE25519_SHA256, ssh_dh_curve25519_sha256 },
	{ DH_G16_SHA512, ssh_dh_group16_sha512 },
	{ DH_G14_SHA256, ssh_dh_group14_sha256 },
	{ DH_G14_SHA1, ssh_dh_group14_sha1 },
	{ DH_G1_SHA1, ssh_dh_group1_sha1 },
	{ NULL, NULL }
};

NEW_DH
search_dh(const char *s)
{
	int i = search_name((name_sp)all_dh, s);
	if (i!=-1)
		return all_dh[i].f;
	else
		return NULL;
}

const char all_dh_list[] =
	"curve25519-sha256@libssh.org,"
	"diffie-hellman-group16-sha512,diffie-hellman-group14-sha256,"
	"diffie-hellman-group14-sha1,diffie-hellman-group1-sha1";

void computeKey(ssh_session *sess, int expected_len, char flag, unsigned char key[])
{
	SSH_DH *dh = sess->dh;

	int len = 0;

	EVP_MD_CTX *mdctx = dh->digest.mdctx;
	const EVP_MD *md = dh->digest.md;
	int hashlen = dh->digest.hashlen;

	while (len < expected_len) {
		EVP_DigestInit_ex(mdctx, md, NULL);
		EVP_DigestUpdate(mdctx, dh->secret, dh->secret_len);
		EVP_DigestUpdate(mdctx, sess->H, hashlen);

		if (len == 0) {
			EVP_DigestUpdate(mdctx, &flag, 1);
			EVP_DigestUpdate(mdctx, sess->session_id, hashlen);
		} else {
			EVP_DigestUpdate(mdctx, key, len);
		}

		EVP_DigestFinal_ex(mdctx, key+len, NULL);
		len += hashlen;
	}
}
