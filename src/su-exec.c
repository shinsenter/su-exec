/* set user and group id and exec */

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	/* check arguments, print usage */
	if (argc < 3) {
		errx(
			1,
			"Usage:\n"
			"  su-exec user:group command [args]\n"
			"  su-exec user command [args]\n"
			"  su-exec :group command [args]\n"
			"\n"
		);
	}

	/* create a new session and become the session leader */
	setsid();

	/* get user and group part from user-spec */
	char *user = argv[1];
	char *group = strchr(user, ':');
	if (group) {
		*group++ = '\0';
	}

	if (user[0] == '\0' || (group && group[0] == '\0')) {
		errx(1, "invalid user-spec");
	}

	/* get passwd entry */
	struct passwd *pw = NULL;
	if (user[0] != '\0') {
		pw = getpwnam(user);
		if (pw == NULL) {
			errx(1, "invalid user-spec, user '%s' not found", user);
		}
	}

	uid_t uid = pw->pw_uid;
	gid_t gid = pw->pw_gid;

	/* set variable HOME and USER in the environment */
	char home[1024];
	snprintf(home, sizeof(home), "%s", pw->pw_dir);
	if (setenv("HOME", home, 1) == -1) {
		err(1, "setenv");
	}

	char username[1024];
	snprintf(username, sizeof(username), "%s", pw->pw_name);
	if (setenv("USER", username, 1) == -1) {
		err(1, "setenv");
	}

	/* get group entry */
	if (group && group[0] != '\0') {
		/* group was specified, ignore grouplist for setgroups later */
		pw = NULL;

		struct group *gr = getgrnam(group);
		if (gr == NULL) {
			errx(1, "invalid user-spec, group '%s' not found", group);
		}

		gid_t ngid = gr->gr_gid;
		gid = ngid;

		/* get group entry */
		if (user[0] != '\0') {
			pw = getpwnam(user);
			if (pw == NULL) {
				errx(1, "invalid user-spec, user '%s' not found", user);
			}
		}

		uid_t nuid = pw->pw_uid;
		if (nuid != uid) {
			errx(1, "invalid user-spec, user '%s' and group '%s' do not match", user, group);
		}
	}

	/* setgroups */
	if (pw == NULL) {
		/* group was specified */
		if (setgroups(1, &gid) < 0)
			err(1, "setgroups(%i)", gid);
	} else {
		/* group was not specified */
		int ngroups = 0;
		gid_t *glist = NULL;

		while (1) {
			int r = getgrouplist(pw->pw_name, gid, glist, &ngroups);

			if (r >= 0) {
				if (setgroups(ngroups, glist) < 0)
					err(1, "setgroups");
				break;
			}

			/* allocate more memory for the group list */
			glist = realloc(glist, (ngroups + 1) * sizeof(gid_t));
			if (glist == NULL) {
				err(1, "realloc");
			}
		}

		free(glist);
	}

	if (setgid(gid) < 0) {
		err(1, "setgid");
	}

	if (setuid(uid) < 0) {
		err(1, "setuid");
	}

	execvp(argv[2], argv + 2);
	err(1, "execvp");

	exit(0);
}
