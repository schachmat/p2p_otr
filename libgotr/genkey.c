#include <sys/stat.h>

#include "gotr.h"
#include "util.h"
#include "crypto.h"

int main(int argc, char *argv[])
{
	FILE *fp;
	size_t size = sizeof(struct gotr_dhe_skey);
	mode_t oldmask;
	struct gotr_dhe_skey key;

	gcry_error_t err = 0;
	if (!gcry_check_version(GOTR_GCRYPT_VERSION)) {
		gotr_eprintf("libgcrypt version mismatch");
		return 1;
	}

	gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
	gotr_rand_poll();

	if (argc != 2 || !argv[1]) {
		gotr_eprintf("usage: gotr_genkey FILENAME");
		return 1;
	}

	gotr_ecdhe_key_create(&key);

	oldmask = umask(077);
	if ((fp = fopen(argv[1], "wb"))) {
		if (fwrite(&key, 1, size, fp) != size)
			gotr_eprintf("could not write private key to file %s", argv[1]);
		fclose(fp);
	} else {
		gotr_eprintf("could not open file %s for writing:", argv[1]);
	}
	umask(oldmask);

	return 0;
}