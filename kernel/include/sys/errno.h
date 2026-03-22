#ifndef _SYS_ERRNO_H
#define _SYS_ERRNO_H

// yoinked from linux abi (mlibc)
#define EPERM 1
#define ENOENT 2
#define ESRCH 3
#define EINTR 4
#define EIO 5
#define ENXIO 6
#define E2BIG 7
#define ENOEXEC 8
#define EBADF 9
#define ECHILD 10
#define EAGAIN 11
#define ENOMEM 12
#define EACCES 13
#define EFAULT 14
#define ENOTBLK 15
#define EBUSY 16
#define EEXIST 17
#define EXDEV 18
#define ENODEV 19
#define ENOTDIR 20
#define EISDIR 21
#define EINVAL 22
#define ENFILE 23
#define EMFILE 24
#define ENOTTY 25
#define ETXTBSY 26
#define EFBIG 27
#define ENOSPC 28
#define ESPIPE 29
#define EROFS 30
#define EMLINK 31
#define EPIPE 32
#define EDOM 33
#define ERANGE 34
#define EDEADLK 35
#define ENAMETOOLONG 36
#define ENOLCK 37
#define ENOSYS 38
#define ENOTEMPTY 39
#define ELOOP 40
#define EWOULDBLOCK EAGAIN
#define ENOMSG 42
#define EIDRM 43
#define ECHRNG 44
#define EL2NSYNC 45
#define EL3HLT 46
#define EL3RST 47
#define ELNRNG 48
#define EUNATCH 49
#define ENOCSI 50
#define EL2HLT 51
#define EBADE 52
#define EBADR 53
#define EXFULL 54
#define ENOANO 55
#define EBADRQC 56
#define EBADSLT 57
#define EDEADLOCK EDEADLK
#define EBFONT 59
#define ENOSTR 60
#define ENODATA 61
#define ETIME 62
#define ENOSR 63
#define ENONET 64
#define ENOPKG 65
#define EREMOTE 66
#define ENOLINK 67
#define EADV 68
#define ESRMNT 69
#define ECOMM 70
#define EPROTO 71
#define EMULTIHOP 72
#define EDOTDOT 73
#define EBADMSG 74
#define EOVERFLOW 75
#define ENOTUNIQ 76
#define EBADFD 77
#define EREMCHG 78
#define ELIBACC 79
#define ELIBBAD 80
#define ELIBSCN 81
#define ELIBMAX 82
#define ELIBEXEC 83
#define EILSEQ 84
#define ERESTART 85
#define ESTRPIPE 86
#define EUSERS 87
#define ENOTSOCK 88
#define EDESTADDRREQ 89
#define EMSGSIZE 90
#define EPROTOTYPE 91
#define ENOPROTOOPT 92
#define EPROTONOSUPPORT 93
#define ESOCKTNOSUPPORT 94
#define EOPNOTSUPP 95
#define ENOTSUP EOPNOTSUPP
#define EPFNOSUPPORT 96
#define EAFNOSUPPORT 97
#define EADDRINUSE 98
#define EADDRNOTAVAIL 99
#define ENETDOWN 100
#define ENETUNREACH 101
#define ENETRESET 102
#define ECONNABORTED 103
#define ECONNRESET 104
#define ENOBUFS 105
#define EISCONN 106
#define ENOTCONN 107
#define ESHUTDOWN 108
#define ETOOMANYREFS 109
#define ETIMEDOUT 110
#define ECONNREFUSED 111
#define EHOSTDOWN 112
#define EHOSTUNREACH 113
#define EALREADY 114
#define EINPROGRESS 115
#define ESTALE 116
#define EUCLEAN 117
#define ENOTNAM 118
#define ENAVAIL 119
#define EISNAM 120
#define EREMOTEIO 121
#define EDQUOT 122
#define ENOMEDIUM 123
#define EMEDIUMTYPE 124
#define ECANCELED 125
#define ENOKEY 126
#define EKEYEXPIRED 127
#define EKEYREVOKED 128
#define EKEYREJECTED 129
#define EOWNERDEAD 130
#define ENOTRECOVERABLE 131
#define ERFKILL 132
#define EHWPOISON 133

#define ERRNO_NAME(err)                           \
	((err) == EPERM ? "EPERM" :                  \
	 (err) == ENOENT ? "ENOENT" :                \
	 (err) == ESRCH ? "ESRCH" :                  \
	 (err) == EINTR ? "EINTR" :                  \
	 (err) == EIO ? "EIO" :                      \
	 (err) == ENXIO ? "ENXIO" :                  \
	 (err) == E2BIG ? "E2BIG" :                  \
	 (err) == ENOEXEC ? "ENOEXEC" :              \
	 (err) == EBADF ? "EBADF" :                  \
	 (err) == ECHILD ? "ECHILD" :                \
	 (err) == EAGAIN ? "EAGAIN" :                \
	 (err) == ENOMEM ? "ENOMEM" :                \
	 (err) == EACCES ? "EACCES" :                \
	 (err) == EFAULT ? "EFAULT" :                \
	 (err) == ENOTBLK ? "ENOTBLK" :              \
	 (err) == EBUSY ? "EBUSY" :                  \
	 (err) == EEXIST ? "EEXIST" :                \
	 (err) == EXDEV ? "EXDEV" :                  \
	 (err) == ENODEV ? "ENODEV" :                \
	 (err) == ENOTDIR ? "ENOTDIR" :              \
	 (err) == EISDIR ? "EISDIR" :                \
	 (err) == EINVAL ? "EINVAL" :                \
	 (err) == ENFILE ? "ENFILE" :                \
	 (err) == EMFILE ? "EMFILE" :                \
	 (err) == ENOTTY ? "ENOTTY" :                \
	 (err) == ETXTBSY ? "ETXTBSY" :              \
	 (err) == EFBIG ? "EFBIG" :                  \
	 (err) == ENOSPC ? "ENOSPC" :                \
	 (err) == ESPIPE ? "ESPIPE" :                \
	 (err) == EROFS ? "EROFS" :                  \
	 (err) == EMLINK ? "EMLINK" :                \
	 (err) == EPIPE ? "EPIPE" :                  \
	 (err) == EDOM ? "EDOM" :                    \
	 (err) == ERANGE ? "ERANGE" :                \
	 (err) == EDEADLK ? "EDEADLK" :              \
	 (err) == ENAMETOOLONG ? "ENAMETOOLONG" :    \
	 (err) == ENOLCK ? "ENOLCK" :                \
	 (err) == ENOSYS ? "ENOSYS" :                \
	 (err) == ENOTEMPTY ? "ENOTEMPTY" :          \
	 (err) == ELOOP ? "ELOOP" :                  \
	 (err) == ENOMSG ? "ENOMSG" :                \
	 (err) == EIDRM ? "EIDRM" :                  \
	 (err) == ECHRNG ? "ECHRNG" :                \
	 (err) == EL2NSYNC ? "EL2NSYNC" :            \
	 (err) == EL3HLT ? "EL3HLT" :                \
	 (err) == EL3RST ? "EL3RST" :                \
	 (err) == ELNRNG ? "ELNRNG" :                \
	 (err) == EUNATCH ? "EUNATCH" :              \
	 (err) == ENOCSI ? "ENOCSI" :                \
	 (err) == EL2HLT ? "EL2HLT" :                \
	 (err) == EBADE ? "EBADE" :                  \
	 (err) == EBADR ? "EBADR" :                  \
	 (err) == EXFULL ? "EXFULL" :                \
	 (err) == ENOANO ? "ENOANO" :                \
	 (err) == EBADRQC ? "EBADRQC" :              \
	 (err) == EBADSLT ? "EBADSLT" :              \
	 (err) == EBFONT ? "EBFONT" :                \
	 (err) == ENOSTR ? "ENOSTR" :                \
	 (err) == ENODATA ? "ENODATA" :              \
	 (err) == ETIME ? "ETIME" :                  \
	 (err) == ENOSR ? "ENOSR" :                  \
	 (err) == ENONET ? "ENONET" :                \
	 (err) == ENOPKG ? "ENOPKG" :                \
	 (err) == EREMOTE ? "EREMOTE" :              \
	 (err) == ENOLINK ? "ENOLINK" :              \
	 (err) == EADV ? "EADV" :                    \
	 (err) == ESRMNT ? "ESRMNT" :                \
	 (err) == ECOMM ? "ECOMM" :                  \
	 (err) == EPROTO ? "EPROTO" :                \
	 (err) == EMULTIHOP ? "EMULTIHOP" :          \
	 (err) == EDOTDOT ? "EDOTDOT" :              \
	 (err) == EBADMSG ? "EBADMSG" :              \
	 (err) == EOVERFLOW ? "EOVERFLOW" :          \
	 (err) == ENOTUNIQ ? "ENOTUNIQ" :            \
	 (err) == EBADFD ? "EBADFD" :                \
	 (err) == EREMCHG ? "EREMCHG" :              \
	 (err) == ELIBACC ? "ELIBACC" :              \
	 (err) == ELIBBAD ? "ELIBBAD" :              \
	 (err) == ELIBSCN ? "ELIBSCN" :              \
	 (err) == ELIBMAX ? "ELIBMAX" :              \
	 (err) == ELIBEXEC ? "ELIBEXEC" :            \
	 (err) == EILSEQ ? "EILSEQ" :                \
	 (err) == ERESTART ? "ERESTART" :            \
	 (err) == ESTRPIPE ? "ESTRPIPE" :            \
	 (err) == EUSERS ? "EUSERS" :                \
	 (err) == ENOTSOCK ? "ENOTSOCK" :            \
	 (err) == EDESTADDRREQ ? "EDESTADDRREQ" :    \
	 (err) == EMSGSIZE ? "EMSGSIZE" :            \
	 (err) == EPROTOTYPE ? "EPROTOTYPE" :        \
	 (err) == ENOPROTOOPT ? "ENOPROTOOPT" :      \
	 (err) == EPROTONOSUPPORT ? "EPROTONOSUPPORT" : \
	 (err) == ESOCKTNOSUPPORT ? "ESOCKTNOSUPPORT" : \
	 (err) == EOPNOTSUPP ? "EOPNOTSUPP" :        \
	 (err) == EPFNOSUPPORT ? "EPFNOSUPPORT" :    \
	 (err) == EAFNOSUPPORT ? "EAFNOSUPPORT" :    \
	 (err) == EADDRINUSE ? "EADDRINUSE" :        \
	 (err) == EADDRNOTAVAIL ? "EADDRNOTAVAIL" :  \
	 (err) == ENETDOWN ? "ENETDOWN" :            \
	 (err) == ENETUNREACH ? "ENETUNREACH" :      \
	 (err) == ENETRESET ? "ENETRESET" :          \
	 (err) == ECONNABORTED ? "ECONNABORTED" :    \
	 (err) == ECONNRESET ? "ECONNRESET" :        \
	 (err) == ENOBUFS ? "ENOBUFS" :              \
	 (err) == EISCONN ? "EISCONN" :              \
	 (err) == ENOTCONN ? "ENOTCONN" :            \
	 (err) == ESHUTDOWN ? "ESHUTDOWN" :          \
	 (err) == ETOOMANYREFS ? "ETOOMANYREFS" :    \
	 (err) == ETIMEDOUT ? "ETIMEDOUT" :          \
	 (err) == ECONNREFUSED ? "ECONNREFUSED" :    \
	 (err) == EHOSTDOWN ? "EHOSTDOWN" :          \
	 (err) == EHOSTUNREACH ? "EHOSTUNREACH" :    \
	 (err) == EALREADY ? "EALREADY" :            \
	 (err) == EINPROGRESS ? "EINPROGRESS" :      \
	 (err) == ESTALE ? "ESTALE" :                \
	 (err) == EUCLEAN ? "EUCLEAN" :              \
	 (err) == ENOTNAM ? "ENOTNAM" :              \
	 (err) == ENAVAIL ? "ENAVAIL" :              \
	 (err) == EISNAM ? "EISNAM" :                \
	 (err) == EREMOTEIO ? "EREMOTEIO" :          \
	 (err) == EDQUOT ? "EDQUOT" :                \
	 (err) == ENOMEDIUM ? "ENOMEDIUM" :          \
	 (err) == EMEDIUMTYPE ? "EMEDIUMTYPE" :      \
	 (err) == ECANCELED ? "ECANCELED" :          \
	 (err) == ENOKEY ? "ENOKEY" :                \
	 (err) == EKEYEXPIRED ? "EKEYEXPIRED" :      \
	 (err) == EKEYREVOKED ? "EKEYREVOKED" :      \
	 (err) == EKEYREJECTED ? "EKEYREJECTED" :    \
	 (err) == EOWNERDEAD ? "EOWNERDEAD" :        \
	 (err) == ENOTRECOVERABLE ? "ENOTRECOVERABLE" : \
	 (err) == ERFKILL ? "ERFKILL" :              \
	 (err) == EHWPOISON ? "EHWPOISON" :          \
	 "EUNKNOWN")

#endif // _SYS_ERRNO_H