#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gmp.h>

#include "rsa.h"

static int usage(FILE *fp)
{
	return fprintf(fp,
"Usage:\n"
"  rsa encrypt <keyfile> <message>\n"
"  rsa decrypt <keyfile> <ciphertext>\n"
"  rsa genkey <numbits>\n"
	);
}

/* Encode the string s into an integer and store it in x. We're assuming that s
 * does not have any leading \x00 bytes (otherwise we would have to encode how
 * many leading zeros there are). */
static void encode(mpz_t x, const char *s)
{
	mpz_import(x, strlen(s), 1, 1, 0, 0, s);
}

/* Decode the integer x into a NUL-terminated string and return the string. The
 * returned string is allocated using malloc and it is the caller's
 * responsibility to free it. If len is not NULL, store the length of the string
 * (not including the NUL terminator) in *len. */
static char *decode(const mpz_t x, size_t *len)
{
	void (*gmp_freefunc)(void *, size_t);
	size_t count;
	char *s, *buf;

	buf = mpz_export(NULL, &count, 1, 1, 0, 0, x);

	s = malloc(count + 1);
	if (s == NULL)
		abort();
	memmove(s, buf, count);
	s[count] = '\0';
	if (len != NULL)
		*len = count;

	/* Ask GMP for the appropriate free function to use. */
	mp_get_memory_functions(NULL, NULL, &gmp_freefunc);
	gmp_freefunc(buf, count);

	return s;
}

/* The "encrypt" subcommand.
 *
 * The return value is the exit code of the program as a whole: nonzero if there
 * was an error; zero otherwise. */
static int encrypt_mode(const char *key_filename, const char *message)
{
	//Init RSA key
	struct rsa_key enc_key;
	rsa_key_init(&enc_key);
	int ret = rsa_key_load_public(key_filename, &enc_key);
	if(ret != 0) {
		printf("Error: could not load key from %s\n", key_filename);
		return 1;
	}

	//Encode message m in an mpz var
	mpz_t m;
	mpz_init(m);
	encode(m, message);

	//Make sure message isn't too big
	if(mpz_cmp(m, (&enc_key)->n) > 0) {
		printf("Error: m is too big\n");
		return 1;
	}

	//Init ciphertext var
	mpz_t c;
	mpz_init(c);

	//Encrypt and output
	rsa_encrypt(c, m, &enc_key);
	gmp_printf("%Zd", c);

	//Clean up
	rsa_key_clear(&enc_key);
	mpz_clear(c);
	mpz_clear(m);
	return 0;
}

/* The "decrypt" subcommand. c_str should be the string representation of an
 * integer ciphertext.
 *
 * The return value is the exit code of the program as a whole: nonzero if there
 * was an error; zero otherwise. */
static int decrypt_mode(const char *key_filename, const char *c_str)
{
	//Init RSA key
	struct rsa_key dec_key;
	rsa_key_init(&dec_key);
	int ret = rsa_key_load_private(key_filename, &dec_key);
	if(ret != 0) {
		printf("Error: could not load key from %s\n", key_filename);
		return 1;
	}

	//Init mpz vars for cipher text and plain text
	mpz_t c, m;
	mpz_init(c);
	mpz_init(m);

	//Set char input to mpz variable
	mpz_set_str(c, c_str, 10);

	//Decrypt and decode integer to characters, then output
	rsa_decrypt(m, c, &dec_key);
	size_t out_len = 0;
	const char* output = decode(m, &out_len);
	printf("%s", output);

	//Cleanup
	rsa_key_clear(&dec_key);
	mpz_clear(c);
	mpz_clear(m);

	return 1;
}

/* The "genkey" subcommand. numbits_str should be the string representation of
 * an integer number of bits (e.g. "1024").
 *
 * The return value is the exit code of the program as a whole: nonzero if there
 * was an error; zero otherwise. */
static int genkey_mode(const char *numbits_str)
{
	//Init RSA key
	struct rsa_key genkey;
	rsa_key_init(&genkey);

	//Convert numbits to ing
	char *ptr;
	unsigned int numbits_int = strtoul(numbits_str, &ptr, 10);

	//Gen key and output
	rsa_genkey(&genkey, numbits_int);
	rsa_key_write(stdout, &genkey);

	return 0;
}

int main(int argc, char *argv[]) {
	const char *command;

	if (argc < 2) {
		usage(stderr);
		return 1;
	}
	command = argv[1];

	if (strcmp(command, "-h") == 0 || strcmp(command, "--help") == 0 || strcmp(command, "help") == 0) {
		usage(stdout);
		return 0;
	} else if (strcmp(command, "encrypt") == 0) {
		const char *key_filename, *message;

		if (argc != 4) {
			fprintf(stderr, "encrypt needs a key filename and a message\n");
			return 1;
		}
		key_filename = argv[2];
		message = argv[3];

		return encrypt_mode(key_filename, message);
	} else if (strcmp(command, "decrypt") == 0) {
		const char *key_filename, *c_str;

		if (argc != 4) {
			fprintf(stderr, "decrypt needs a key filename and a ciphertext\n");
			return 1;
		}
		key_filename = argv[2];
		c_str = argv[3];

		return decrypt_mode(key_filename, c_str);
	} else if (strcmp(command, "genkey") == 0) {
		const char *numbits_str;

		if (argc != 3) {
			fprintf(stderr, "genkey needs a number of bits\n");
			return 1;
		}
		numbits_str = argv[2];

		return genkey_mode(numbits_str);
	}

	usage(stderr); 
	return 1;
}
