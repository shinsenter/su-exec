/* set user and group id and exec */

#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

/* minimal error reporting: avoids pulling stdio/err into the binary */
static void put(const char *s) {
	size_t n = strlen(s);
	while (n > 0) {
		ssize_t w = write(2, s, n);
		if (w <= 0) {
			break;
		}
		s += w;
		n -= (size_t)w;
	}
}

static void die(const char *what, const char *arg, int use_errno) {
	int e = errno;
	put("su-exec: ");
	put(what);
	if (arg) {
		put(" '");
		put(arg);
		put("'");
	}
	if (use_errno) {
		put(": ");
		put(strerror(e));
	}
	put("\n");
	_exit(1);
}

/* parse a purely numeric id; return 0 if `s` is not a valid number */
static int parse_id(const char *s, unsigned long *out) {
	char *end;

	if (s[0] < '0' || s[0] > '9') {
		return 0;
	}

	errno = 0;
	*out = strtoul(s, &end, 10);
	if (*end != '\0') {
		return 0;
	}
	if (errno == ERANGE || *out > (unsigned long)UINT_MAX) {
		die("invalid user-spec, id out of range:", s, 0);
	}

	return 1;
}

int main(int argc, char *argv[]) {
	/* check arguments, print usage */
	if (argc < 3) {
		put("Usage:\n"
		    "  su-exec user:group command [args]\n"
		    "  su-exec user command [args]\n"
		    "  su-exec :group command [args]\n"
		    "\n");
		_exit(1);
	}

	/* create a new session and become the session leader.
	 * This detaches the command from the controlling terminal and
	 * prevents TIOCSTI keystroke injection into the parent session.
	 * It fails harmlessly when already a session leader. */
	(void)setsid();

	/* get user and group part from user-spec */
	char *user = argv[1];
	char *group = strchr(user, ':');
	if (group) {
		*group++ = '\0';
	}

	uid_t uid = getuid();
	gid_t gid = getgid();
	unsigned long id;

	/* get passwd entry; an empty user keeps the current uid */
	struct passwd *pw = NULL;
	if (user[0] != '\0') {
		if (parse_id(user, &id)) {
			uid = (uid_t)id;
		} else {
			pw = getpwnam(user);
			if (pw == NULL) {
				die("invalid user-spec, user not found:", user, 0);
			}
		}
	}
	if (pw == NULL) {
		pw = getpwuid(uid);
	}
	if (pw != NULL) {
		uid = pw->pw_uid;
		gid = pw->pw_gid;
	}

	/* set variable HOME (and USER when known) in the environment */
	if (setenv("HOME", pw != NULL ? pw->pw_dir : "/", 1) == -1) {
		die("setenv", NULL, 1);
	}
	if (pw != NULL && setenv("USER", pw->pw_name, 1) == -1) {
		die("setenv", NULL, 1);
	}

	/* get group entry */
	if (group && group[0] != '\0') {
		/* group was specified, ignore grouplist for setgroups later */
		pw = NULL;

		if (parse_id(group, &id)) {
			gid = (gid_t)id;
		} else {
			struct group *gr = getgrnam(group);
			if (gr == NULL) {
				die("invalid user-spec, group not found:", group, 0);
			}
			gid = gr->gr_gid;
		}
	}

	/* setgroups */
	if (pw == NULL) {
		/* group was specified, or the uid has no passwd entry */
		if (setgroups(1, &gid) < 0) {
			die("setgroups", NULL, 1);
		}
	} else {
		/* group was not specified, use the supplementary group list */
		int ngroups = 0;
		gid_t *glist = NULL;

		while (1) {
			int r = getgrouplist(pw->pw_name, gid, glist, &ngroups);

			if (r >= 0) {
				if (setgroups(ngroups, glist) < 0) {
					die("setgroups", NULL, 1);
				}
				break;
			}

			/* getgrouplist stored the required size in ngroups */
			glist = realloc(glist, (size_t)ngroups * sizeof(gid_t));
			if (glist == NULL) {
				die("realloc", NULL, 1);
			}
		}

		free(glist);
	}

	if (setgid(gid) < 0) {
		die("setgid", NULL, 1);
	}

	if (setuid(uid) < 0) {
		die("setuid", NULL, 1);
	}

	execvp(argv[2], argv + 2);
	die("execvp", argv[2], 1);
}
