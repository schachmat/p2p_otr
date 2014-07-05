#include <stdio.h>

#include "util.h"
#include "crypto.h"
#include "gotr.h"
#include "messaging.h"
#include "b64.h"
#include "gka.h"
#include "key.h"

struct gotr_user;

struct gotr_chatroom {
	gotr_cb_send_all send_all;       ///< callback to send a message to every participant in this room
	gotr_cb_send_user send_usr;       ///< callback to send a message to a specific user
	gotr_cb_receive_usr receive_usr; ///< callback to notify the client about a decrypted message he has to print
	struct gotr_roomdata data;
};

static int test();
static struct gotr_user *gotr_new_user(struct gotr_chatroom *room, void *user_closure);

int gotr_init()
{
	gcry_error_t err = 0;
	if (!gcry_check_version(GOTR_GCRYPT_VERSION)) {
		gotr_eprintf("libgcrypt version mismatch");
		return 0;
	}

	if ((err = gcry_control(GCRYCTL_DISABLE_SECMEM, 0)))
		gotr_eprintf("failed to set libgcrypt option DISABLE_SECMEM: %s",
				gcry_strerror(err));

	gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);

	gotr_rand_poll();

	return gotr_gka_init();
}

struct gotr_chatroom *gotr_join(gotr_cb_send_all send_all, gotr_cb_send_user send_usr, gotr_cb_receive_usr receive_usr, const void *room_closure, const char *privkey_filename)
{
	struct gotr_chatroom *room;

	test();
	room = malloc(sizeof(struct gotr_chatroom));
	room->data.closure = room_closure;
	room->send_all = send_all;
	room->send_usr = send_usr;
	room->receive_usr = receive_usr;

	load_privkey(privkey_filename, &room->data.my_privkey);
	gotr_eddsa_key_get_public(&room->data.my_privkey, &room->data.my_pubkey);
	return room;
}

int gotr_send(struct gotr_chatroom *room, char *plain_msg)
{
	char *b64_msg;
	int ret = 0;

	if(!(b64_msg = gotr_b64_enc((unsigned char *)plain_msg, strlen(plain_msg)))) {
		gotr_eprintf("unable to base64 encode message");
		return 0;
	}

	if(!(ret = room->send_all((void *)room->data.closure, b64_msg)))
		gotr_eprintf("unable to broadcast message");

	free(b64_msg);
	return ret;
}

int gotr_receive(struct gotr_chatroom *room, char *b64_msg)
{
	size_t len = 0;
	char *packed_msg = NULL;

	if (!room || !b64_msg) {
		gotr_eprintf("called gotr_receive with NULL argument");
		return 0;
	}

	if ((gotr_b64_dec(b64_msg, (unsigned char **)&packed_msg, &len))) {
		gotr_eprintf("could not decode message: %s", b64_msg);
		return 0;
	}
	packed_msg[len-1] = '\0';

	gotr_parse_msg(&room->data, packed_msg);

	free(packed_msg);
	return 1;
}

int gotr_receive_user(struct gotr_chatroom *room, struct gotr_user *user, char *b64_msg)
{
	size_t len = 0;
	char *packed_msg = NULL;

	if (!room || !user || !b64_msg) {
		gotr_eprintf("called gotr_receive_user with NULL argument");
		return 0;
	}

	if ((gotr_b64_dec(b64_msg, (unsigned char **)&packed_msg, &len))) {
		gotr_eprintf("could not decode message: %s", b64_msg);
		return 0;
	}
	packed_msg[len-1] = '\0';

	/// @todo unpack (and handle)

	free(packed_msg);
	return 1;
}

/**
 * @brief BLABLA
 * @todo docu
 */
void gotr_user_joined(struct gotr_chatroom *room, void *user_closure) {
	unsigned char *packed_msg;
	char *b64_msg;
	struct gotr_user *user;

	if(!room) {
		gotr_eprintf("passed room was NULL");
		return;
	}

	if(!(user = gotr_new_user(room, user_closure))) {
		gotr_eprintf("could not create new user");
		return;
	}

	if(!(packed_msg = gotr_pack_est_pair_channel(&room->data, user))) {
		gotr_eprintf("could not pack est_pair_channel message");
		return;
	}

	if((b64_msg = gotr_b64_enc(packed_msg, sizeof(struct est_pair_channel)))) {
		room->send_usr((void *)room->data.closure, user->closure, b64_msg);
		free(b64_msg);
	} else {
		gotr_eprintf("could not b64 encode est_pair_channel message");
	}

	free(packed_msg);
}

struct gotr_user *gotr_new_user(struct gotr_chatroom *room, void *user_closure)
{
	struct gotr_user *user;

	if (!room || !(user = malloc(sizeof(struct gotr_user))))
		return NULL;

	user->closure = user_closure;
	user->state = GOTR_STATE_UNKNOWN;
	user->next = room->data.users;
	return room->data.users = user;
}

void gotr_leave(struct gotr_chatroom *room)
{
	struct gotr_user *user;

	if (!room)
		return;

	while (room->data.users) {
		user = room->data.users;
		room->data.users = user->next;
		free(user);
	}

	gotr_eddsa_key_clear(&room->data.my_privkey);

	free(room);
}

/**
 * for testing purposes only.
 */
static int test()
{
	struct gotr_user u[2];
	gotr_gen_BD_keypair(&u[0].r[0], &u[0].z[0]);
	gotr_gen_BD_keypair(&u[0].r[1], &u[0].z[1]);
	u[1].y[0] = u[0].z[0];
	u[1].y[1] = u[0].z[1];
	gotr_gen_BD_keypair(&u[1].r[0], &u[1].z[0]);
	gotr_gen_BD_keypair(&u[1].r[1], &u[1].z[1]);
	u[0].y[0] = u[1].z[0];
	u[0].y[1] = u[1].z[1];
	if (!gotr_gen_BD_X_value(&u[0].R[0], u[0].y[1], u[0].z[1], u[0].r[0]))
		gotr_eprintf("X0 failed");
	if (!gotr_gen_BD_X_value(&u[0].R[1], u[0].z[0], u[0].y[0], u[0].r[1]))
		gotr_eprintf("X1 failed");
	if (!gotr_gen_BD_X_value(&u[1].R[0], u[1].y[1], u[1].z[1], u[1].r[0]))
		gotr_eprintf("X2 failed");
	if (!gotr_gen_BD_X_value(&u[1].R[1], u[1].z[0], u[1].y[0], u[1].r[1]))
		gotr_eprintf("X3 failed");
	u[1].V[0] = u[0].R[0];
	u[1].V[1] = u[0].R[1];
	u[0].V[0] = u[1].R[0];
	u[0].V[1] = u[1].R[1];
	if (!gotr_gen_BD_flake_key(&u[0].flake_key, u[0].y[0], u[0].r[1], u[0].R[0], u[0].R[1], u[0].V[1]))
		gotr_eprintf("f0 failed");
	if (!gotr_gen_BD_flake_key(&u[1].flake_key, u[1].y[0], u[1].r[1], u[1].R[0], u[1].R[1], u[1].V[1]))
		gotr_eprintf("f1 failed");
	gcry_mpi_dump(u[0].flake_key);
	gotr_eprintf("");
	gcry_mpi_dump(u[1].flake_key);
	gotr_eprintf("");

	u[0].state = u[1].state = GOTR_STATE_FLAKE_VALIDATED;
	u[0].next = u[1].next = NULL;
	if (!gotr_gen_BD_circle_key(u[0].flake_key, &u[0]))
		gotr_eprintf("c0 failed");
	if (gcry_mpi_cmp(u[0].flake_key, u[1].flake_key))
		gotr_eprintf("flake != c0");
	gcry_mpi_dump(u[0].flake_key);
	gotr_eprintf("");
	if (!gotr_gen_BD_circle_key(u[1].flake_key, &u[1]))
		gotr_eprintf("c1 failed");
	if (gcry_mpi_cmp(u[1].flake_key, u[0].flake_key))
		gotr_eprintf("flake != c1");
	gcry_mpi_dump(u[1].flake_key);
	gotr_eprintf("");
	gotr_eprintf("circle keys match");

	return 0 == gcry_mpi_cmp(u[0].flake_key, u[1].flake_key);
}
