#!/bin/sh
# Functional tests for su-exec. Run as root inside a container where
# su-exec is installed as /usr/sbin/su-exec. POSIX sh and busybox compatible.

SU=/usr/sbin/su-exec
pass=0
fail=0

ok()  { pass=$((pass + 1)); echo "  ok   $1"; }
bad() { fail=$((fail + 1)); echo "  FAIL $1 (got: $2)"; }

# expect NAME EXPECTED_STDOUT cmd...
expect() {
	name=$1 want=$2
	shift 2
	got=$("$@" 2>/dev/null)
	if [ "$got" = "$want" ]; then ok "$name"; else bad "$name" "$got"; fi
}

# expect_fail NAME EXPECTED_EXIT cmd...  (stdout and stderr discarded)
expect_fail() {
	name=$1 want=$2
	shift 2
	"$@" >/dev/null 2>&1
	rc=$?
	if [ "$rc" -eq "$want" ]; then ok "$name"; else bad "$name" "exit $rc"; fi
}

# sorted, de-duplicated list of group ids
gids() { tr ' ' '\n' | sort -un | tr '\n' ' '; }

NUID=$(id -u nobody)
NGID=$(id -g nobody)
NHOME=$(awk -F: '$1=="nobody"{print $6}' /etc/passwd)

echo "user-spec"
expect "name"                      "$NUID" $SU nobody id -u
expect "numeric uid"               "$NUID" $SU "$NUID" id -u
expect "name keeps primary gid"    "$NGID" $SU nobody id -g
expect "user:numeric gid"          "0"     $SU nobody:0 id -g
expect "numeric:numeric"           "7777"  $SU 7777:7777 id -g
expect "uid without passwd entry"  "12345" $SU 12345 id -u
expect "user: (empty group)"       "$NGID" $SU nobody: id -g
expect ":group keeps uid"          "0"     $SU ":$NGID" id -u
expect ":group sets gid"           "$NGID" $SU ":$NGID" id -g
expect "uid 0 is allowed"          "0"     $SU 0 id -u

echo "groups"
expect "group given: only that gid" "0" sh -c "$SU nobody:0 id -G | tr ' ' '\n' | sort -un | tr '\n' ' ' | sed 's/ *\$//'"
want=$(id -G nobody | gids)
got=$($SU nobody id -G | gids)
if [ "$got" = "$want" ]; then ok "no group: supplementary list matches"; else bad "no group: supplementary list matches" "$got vs $want"; fi

echo "environment"
expect "HOME from passwd"          "$NHOME" $SU nobody sh -c 'echo $HOME'
expect "USER from passwd"          "nobody" $SU nobody sh -c 'echo $USER'
expect "HOME is / without entry"   "/"      $SU 12345 sh -c 'echo $HOME'

echo "exec behaviour"
expect_fail "exit code is passed through"  42 $SU nobody sh -c 'exit 42'
expect "arguments are not re-split" 'a b "c" $d' $SU nobody sh -c 'printf %s "$1"' _ 'a b "c" $d'
got=$(echo hello | $SU nobody cat 2>/dev/null)
if [ "$got" = "hello" ]; then ok "stdin is passed through"; else bad "stdin is passed through" "$got"; fi
if [ -r /proc/self/stat ]; then
	expect "setsid: command is session leader" "yes" \
		$SU nobody sh -c 'set -- $(cat /proc/$$/stat | sed "s/^.*) //"); [ "$4" = "$$" ] && echo yes || echo no'
fi

echo "errors"
expect_fail "no arguments"                1 $SU
expect_fail "user only, no command"       1 $SU nobody
expect_fail "unknown user"                1 $SU no_such_user_xyz id
expect_fail "unknown group"               1 $SU nobody:no_such_group_xyz id
expect_fail "unknown command"             1 $SU nobody no_such_cmd_xyz
expect_fail "uid out of range"            1 $SU 99999999999 id
expect_fail "gid out of range"            1 $SU nobody:99999999999 id

echo "privilege boundary"
expect_fail "non-root cannot switch to root" 1 $SU nobody $SU root id
expect_fail "non-root cannot use uid 0"      1 $SU nobody $SU 0 id
expect "binary is not setuid" "no" sh -c "[ -u $SU ] && echo yes || echo no"

echo
echo "passed=$pass failed=$fail"
[ "$fail" -eq 0 ]
