template<>
class ABIDefs<ExecModeMips64> {
public:
    typedef uint64_t Tpointer_t;
    typedef int8_t Tchar;
    typedef uint8_t Tuchar;
    typedef int16_t Tshort;
    typedef uint16_t Tushort;
    typedef int32_t Tint;
    typedef uint32_t Tuint;
    typedef int64_t Tlong;
    typedef uint64_t Tulong;
    typedef int64_t Tllong;
    typedef int64_t Tssize_t;
    typedef int64_t Tptrdiff_t;
    typedef uint64_t Tsize_t;
    static const Tint VSTDIN_FILENO = 0x00000000;
    static const Tint VSTDOUT_FILENO = 0x00000001;
    static const Tint VSTDERR_FILENO = 0x00000002;
    typedef int32_t Tpid_t;
    static const Tint VEPERM = 0x00000001;
    static const Tint VENOENT = 0x00000002;
    static const Tint VESRCH = 0x00000003;
    static const Tint VEINTR = 0x00000004;
    static const Tint VEIO = 0x00000005;
    static const Tint VENXIO = 0x00000006;
    static const Tint VE2BIG = 0x00000007;
    static const Tint VENOEXEC = 0x00000008;
    static const Tint VEBADF = 0x00000009;
    static const Tint VECHILD = 0x0000000a;
    static const Tint VEAGAIN = 0x0000000b;
    static const Tint VENOMEM = 0x0000000c;
    static const Tint VEACCES = 0x0000000d;
    static const Tint VEFAULT = 0x0000000e;
    static const Tint VENOTBLK = 0x0000000f;
    static const Tint VEBUSY = 0x00000010;
    static const Tint VEEXIST = 0x00000011;
    static const Tint VEXDEV = 0x00000012;
    static const Tint VENODEV = 0x00000013;
    static const Tint VENOTDIR = 0x00000014;
    static const Tint VEISDIR = 0x00000015;
    static const Tint VEINVAL = 0x00000016;
    static const Tint VENFILE = 0x00000017;
    static const Tint VEMFILE = 0x00000018;
    static const Tint VENOTTY = 0x00000019;
    static const Tint VETXTBSY = 0x0000001a;
    static const Tint VEFBIG = 0x0000001b;
    static const Tint VENOSPC = 0x0000001c;
    static const Tint VESPIPE = 0x0000001d;
    static const Tint VEROFS = 0x0000001e;
    static const Tint VEMLINK = 0x0000001f;
    static const Tint VEPIPE = 0x00000020;
    static const Tint VEDOM = 0x00000021;
    static const Tint VERANGE = 0x00000022;
    static const Tint VENOSYS = 0x00000059;
    static const Tint VELOOP = 0x0000005a;
    static const Tint VEAFNOSUPPORT = 0x0000007c;
    static const Tint VSIGHUP = 0x00000001;
    static const Tint VSIGINT = 0x00000002;
    static const Tint VSIGQUIT = 0x00000003;
    static const Tint VSIGILL = 0x00000004;
    static const Tint VSIGTRAP = 0x00000005;
    static const Tint VSIGABRT = 0x00000006;
    static const Tint VSIGFPE = 0x00000008;
    static const Tint VSIGKILL = 0x00000009;
    static const Tint VSIGBUS = 0x0000000a;
    static const Tint VSIGSEGV = 0x0000000b;
    static const Tint VSIGPIPE = 0x0000000d;
    static const Tint VSIGALRM = 0x0000000e;
    static const Tint VSIGTERM = 0x0000000f;
    static const Tint VSIGUSR1 = 0x00000010;
    static const Tint VSIGUSR2 = 0x00000011;
    static const Tint VSIGCHLD = 0x00000012;
    static const Tint VSIGSTOP = 0x00000017;
    static const Tint VSIGTSTP = 0x00000018;
    static const Tint VSIGCONT = 0x00000019;
    static const Tint VCSIGNAL = 0x000000ff;
    static const Tint VCLONE_VM = 0x00000100;
    static const Tint VCLONE_FS = 0x00000200;
    static const Tint VCLONE_FILES = 0x00000400;
    static const Tint VCLONE_SIGHAND = 0x00000800;
    static const Tint VCLONE_VFORK = 0x00004000;
    static const Tint VCLONE_PARENT = 0x00008000;
    static const Tint VCLONE_THREAD = 0x00010000;
    static const Tint VCLONE_NEWNS = 0x00020000;
    static const Tint VCLONE_SYSVSEM = 0x00040000;
    static const Tint VCLONE_SETTLS = 0x00080000;
    static const Tint VCLONE_PARENT_SETTID = 0x00100000;
    static const Tint VCLONE_CHILD_CLEARTID = 0x00200000;
    static const Tint VCLONE_DETACHED = 0x00400000;
    static const Tint VCLONE_UNTRACED = 0x00800000;
    static const Tint VCLONE_CHILD_SETTID = 0x01000000;
    static const Tint VCLONE_STOPPED = 0x02000000;
    static const Tint VWNOHANG = 0x00000001;
    static const Tint VWUNTRACED = 0x00000002;
    static const Tint VFUTEX_PRIVATE_FLAG = 0x00000080;
    static const Tint VFUTEX_CLOCK_REALTIME = 0x00000100;
    static const Tint VFUTEX_CMD_MASK = ~Tint(0x00000180);
    static const Tint VFUTEX_WAIT = 0x00000000;
    static const Tint VFUTEX_WAKE = 0x00000001;
    static const Tint VFUTEX_FD = 0x00000002;
    static const Tint VFUTEX_REQUEUE = 0x00000003;
    static const Tint VFUTEX_CMP_REQUEUE = 0x00000004;
    static const Tint VFUTEX_WAKE_OP = 0x00000005;
    static const Tint VFUTEX_LOCK_PI = 0x00000006;
    static const Tint VFUTEX_UNLOCK_PI = 0x00000007;
    static const Tint VFUTEX_TRYLOCK_PI = 0x00000008;
    static const Tint VFUTEX_WAIT_BITSET = 0x00000009;
    static const Tint VFUTEX_WAKE_BITSET = 0x0000000a;
    static const Tuint VFUTEX_BITSET_MATCH_ANY = ~Tuint(0x00000000);
    static const Tint VFUTEX_OP_OPARG_SHIFT = 0x00000008;
    static const Tint VFUTEX_OP_SET = 0x00000000;
    static const Tint VFUTEX_OP_ADD = 0x00000001;
    static const Tint VFUTEX_OP_OR = 0x00000002;
    static const Tint VFUTEX_OP_ANDN = 0x00000003;
    static const Tint VFUTEX_OP_XOR = 0x00000004;
    static const Tint VFUTEX_OP_CMP_EQ = 0x00000000;
    static const Tint VFUTEX_OP_CMP_NE = 0x00000001;
    static const Tint VFUTEX_OP_CMP_LT = 0x00000002;
    static const Tint VFUTEX_OP_CMP_LE = 0x00000003;
    static const Tint VFUTEX_OP_CMP_GT = 0x00000004;
    static const Tint VFUTEX_OP_CMP_GE = 0x00000005;

    class Trobust_list {
    public:
        static const size_t Size_All = 8;
        typedef Tpointer_t Type_next;
        static const size_t Offs_next = 0;
    };

    class Trobust_list_head {
    public:
        static const size_t Size_All = 24;
        typedef Trobust_list Type_list;
        static const size_t Offs_list = 0;
        typedef Tlong Type_futex_offset;
        static const size_t Offs_futex_offset = 8;
        typedef Tpointer_t Type_list_op_pending;
        static const size_t Offs_list_op_pending = 16;
    };

    static const Tint VSA_NOCLDSTOP = 0x00000001;
    static const Tint VSA_NOCLDWAIT = 0x00010000;
    static const Tuint VSA_RESETHAND = ~Tuint(0x7fffffff);
    static const Tint VSA_ONSTACK = 0x08000000;
    static const Tint VSA_SIGINFO = 0x00000008;
    static const Tint VSA_RESTART = 0x10000000;
    static const Tint VSA_NODEFER = 0x40000000;
    static const Tint VSA_INTERRUPT = 0x20000000;
    static const Tpointer_t VSIG_DFL = 0x0000000000000000;
    static const Tpointer_t VSIG_IGN = 0x0000000000000001;
    static const Tint VSIG_BLOCK = 0x00000001;
    static const Tint VSIG_UNBLOCK = 0x00000002;
    static const Tint VSIG_SETMASK = 0x00000003;
    typedef int64_t Tclock_t;

    class Ttms {
    public:
        static const size_t Size_All = 32;
        typedef Tclock_t Type_tms_utime;
        static const size_t Offs_tms_utime = 0;
        typedef Tclock_t Type_tms_stime;
        static const size_t Offs_tms_stime = 8;
        typedef Tclock_t Type_tms_cutime;
        static const size_t Offs_tms_cutime = 16;
        typedef Tclock_t Type_tms_cstime;
        static const size_t Offs_tms_cstime = 24;
    };

    typedef int64_t Ttime_t;
    typedef int64_t Tsuseconds_t;

    class Ttimeval {
    public:
        static const size_t Size_All = 16;
        typedef Ttime_t Type_tv_sec;
        static const size_t Offs_tv_sec = 0;
        typedef Tsuseconds_t Type_tv_usec;
        static const size_t Offs_tv_usec = 8;
    };

    class Ttimezone {
    public:
        static const size_t Size_All = 8;
        typedef Tint Type_tz_minuteswest;
        static const size_t Offs_tz_minuteswest = 0;
        typedef Tint Type_tz_dsttime;
        static const size_t Offs_tz_dsttime = 4;
    };

    typedef int32_t Tclockid_t;

    class Ttimespec {
    public:
        static const size_t Size_All = 16;
        typedef Ttime_t Type_tv_sec;
        static const size_t Offs_tv_sec = 0;
        typedef Tlong Type_tv_nsec;
        static const size_t Offs_tv_nsec = 8;
    };

    typedef uint32_t Tuid_t;
    typedef uint32_t Tgid_t;
    static const Tuint VMAP_SHARED = 0x00000001;
    static const Tuint VMAP_PRIVATE = 0x00000002;
    static const Tuint VMAP_FIXED = 0x00000010;
    static const Tuint VMAP_ANONYMOUS = 0x00000800;
    static const Tint VPROT_NONE = 0x00000000;
    static const Tint VPROT_READ = 0x00000001;
    static const Tint VPROT_WRITE = 0x00000002;
    static const Tint VPROT_EXEC = 0x00000004;
    static const Tuint VMREMAP_MAYMOVE = 0x00000001;
    static const Tuint VMREMAP_FIXED = 0x00000002;
    static const Tint VO_ACCMODE = 0x00000003;
    static const Tint VO_RDONLY = 0x00000000;
    static const Tint VO_WRONLY = 0x00000001;
    static const Tint VO_RDWR = 0x00000002;
    static const Tint VO_APPEND = 0x00000008;
    static const Tint VO_SYNC = 0x00000010;
    static const Tint VO_NONBLOCK = 0x00000080;
    static const Tint VO_CREAT = 0x00000100;
    static const Tint VO_TRUNC = 0x00000200;
    static const Tint VO_EXCL = 0x00000400;
    static const Tint VO_NOCTTY = 0x00000800;
    static const Tint VO_ASYNC = 0x00001000;
    static const Tint VO_LARGEFILE = 0x00002000;
    static const Tint VO_DIRECT = 0x00008000;
    static const Tint VO_DIRECTORY = 0x00010000;
    static const Tint VO_NOFOLLOW = 0x00020000;
    typedef uint Tmode_t;
    static const Tuint VS_IRUSR = 0x00000100;
    static const Tuint VS_IWUSR = 0x00000080;
    static const Tuint VS_IXUSR = 0x00000040;
    static const Tuint VS_IRGRP = 0x00000020;
    static const Tuint VS_IWGRP = 0x00000010;
    static const Tuint VS_IXGRP = 0x00000008;
    static const Tuint VS_IROTH = 0x00000004;
    static const Tuint VS_IWOTH = 0x00000002;
    static const Tuint VS_IXOTH = 0x00000001;
    static const Tint VF_DUPFD = 0x00000000;
    static const Tint VF_GETFD = 0x00000001;
    static const Tint VF_SETFD = 0x00000002;
    static const Tint VF_GETFL = 0x00000003;
    static const Tint VF_SETFL = 0x00000004;
    static const Tint VFD_CLOEXEC = 0x00000001;

    class Tiovec {
    public:
        static const size_t Size_All = 16;
        typedef Tpointer_t Type_iov_base;
        static const size_t Offs_iov_base = 0;
        typedef Tsize_t Type_iov_len;
        static const size_t Offs_iov_len = 8;
    };

    typedef int64_t Toff_t;
    typedef int64_t Tloff_t;
    static const Tint VSEEK_SET = 0x00000000;
    static const Tint VSEEK_CUR = 0x00000001;
    static const Tint VSEEK_END = 0x00000002;
    typedef uint64_t Tino_t;
    typedef uint64_t Tino64_t;
    typedef int64_t Toff64_t;

    class Tdirent {
    public:
        static const size_t Size_All = 280;
        typedef Tulong Type_d_ino;
        static const size_t Offs_d_ino = 0;
        typedef Tulong Type_d_off;
        static const size_t Offs_d_off = 8;
        typedef Tushort Type_d_reclen;
        static const size_t Offs_d_reclen = 16;
        typedef Tchar Type_d_name;
        static const size_t Offs_d_name = 18;
        static const size_t Step_d_name = 1;
        static const size_t Size_d_name = 256;
    };

    class Tdirent64 {
    public:
        static const size_t Size_All = 280;
        typedef Tino64_t Type_d_ino;
        static const size_t Offs_d_ino = 0;
        typedef Toff64_t Type_d_off;
        static const size_t Offs_d_off = 8;
        typedef Tushort Type_d_reclen;
        static const size_t Offs_d_reclen = 16;
        typedef Tuchar Type_d_type;
        static const size_t Offs_d_type = 18;
        typedef Tchar Type_d_name;
        static const size_t Offs_d_name = 19;
        static const size_t Step_d_name = 1;
        static const size_t Size_d_name = 256;
    };

    static const Tint VTCGETS = 0x0000540d;
    static const Tint VTCSETS = 0x0000540e;
    static const Tint VTCSETSW = 0x0000540f;
    static const Tint VTCSETSF = 0x00005410;
    static const Tint VTCGETA = 0x00005401;
    static const Tint VTCSETA = 0x00005402;
    static const Tint VTCSETAW = 0x00005403;
    static const Tint VTCSETAF = 0x00005404;
    static const Tlong VTIOCGWINSZ = 0x0000000040087468;

    class Twinsize {
    public:
        static const size_t Size_All = 8;
        typedef Tushort Type_ws_row;
        static const size_t Offs_ws_row = 0;
        typedef Tushort Type_ws_col;
        static const size_t Offs_ws_col = 2;
        typedef Tushort Type_ws_xpixel;
        static const size_t Offs_ws_xpixel = 4;
        typedef Tushort Type_ws_ypixel;
        static const size_t Offs_ws_ypixel = 6;
    };

    class Tpollfd {
    public:
        static const size_t Size_All = 8;
        typedef Tint Type_fd;
        static const size_t Offs_fd = 0;
        typedef Tshort Type_events;
        static const size_t Offs_events = 4;
        typedef Tshort Type_revents;
        static const size_t Offs_revents = 6;
    };

    static const Tint VPOLLIN = 0x00000001;
    static const Tint VPOLLPRI = 0x00000002;
    static const Tint VPOLLOUT = 0x00000004;
    static const Tint VPOLLERR = 0x00000008;
    static const Tint VPOLLHUP = 0x00000010;
    static const Tint VPOLLNVAL = 0x00000020;
    typedef uint32_t Tst_dev_t;
    typedef uint64_t Tst_ino_t;
    typedef uint32_t Tst_nlink_t;
    typedef int64_t Tst_size_t;
    typedef uint32_t Tst_blksize_t;
    typedef uint64_t Tst_blocks_t;

    class Tstat {
    public:
        static const size_t Size_All = 104;
        typedef Tst_dev_t Type_st_dev;
        static const size_t Offs_st_dev = 0;
        typedef Tst_ino_t Type_st_ino;
        static const size_t Offs_st_ino = 16;
        typedef Tmode_t Type_st_mode;
        static const size_t Offs_st_mode = 24;
        typedef Tst_nlink_t Type_st_nlink;
        static const size_t Offs_st_nlink = 28;
        typedef Tuid_t Type_st_uid;
        static const size_t Offs_st_uid = 32;
        typedef Tgid_t Type_st_gid;
        static const size_t Offs_st_gid = 36;
        typedef Tst_dev_t Type_st_rdev;
        static const size_t Offs_st_rdev = 40;
        typedef Tst_size_t Type_st_size;
        static const size_t Offs_st_size = 56;
        typedef Tuint Type_st_atime_sec;
        static const size_t Offs_st_atime_sec = 64;
        typedef Tuint Type_st_mtime_sec;
        static const size_t Offs_st_mtime_sec = 72;
        typedef Tuint Type_st_ctime_sec;
        static const size_t Offs_st_ctime_sec = 80;
        typedef Tst_blksize_t Type_st_blksize;
        static const size_t Offs_st_blksize = 88;
        typedef Tst_blocks_t Type_st_blocks;
        static const size_t Offs_st_blocks = 96;
    };

    typedef uint64_t Tnlink_t;
    typedef int64_t Tblksize_t;
    typedef int64_t Tblkcnt64_t;
    typedef uint64_t Tst_dev64_t;

    class Tstat64 {
    public:
        static const size_t Size_All = 216;
        typedef Tst_dev64_t Type_st_dev;
        static const size_t Offs_st_dev = 0;
        typedef Tino64_t Type_st_ino;
        static const size_t Offs_st_ino = 24;
        typedef Tmode_t Type_st_mode;
        static const size_t Offs_st_mode = 32;
        typedef Tnlink_t Type_st_nlink;
        static const size_t Offs_st_nlink = 40;
        typedef Tuid_t Type_st_uid;
        static const size_t Offs_st_uid = 48;
        typedef Tgid_t Type_st_gid;
        static const size_t Offs_st_gid = 52;
        typedef Tst_dev64_t Type_st_rdev;
        static const size_t Offs_st_rdev = 56;
        typedef Toff64_t Type_st_size;
        static const size_t Offs_st_size = 80;
        typedef Ttime_t Type_st_atime;
        static const size_t Offs_st_atime = 88;
        typedef Ttime_t Type_st_mtime;
        static const size_t Offs_st_mtime = 104;
        typedef Ttime_t Type_st_ctime;
        static const size_t Offs_st_ctime = 120;
        typedef Tblksize_t Type_st_blksize;
        static const size_t Offs_st_blksize = 136;
        typedef Tblkcnt64_t Type_st_blocks;
        static const size_t Offs_st_blocks = 152;
    };

    static const Tint VF_OK = 0x00000000;
    static const Tint VR_OK = 0x00000004;
    static const Tint VW_OK = 0x00000002;
    static const Tint VX_OK = 0x00000001;
    typedef uint64_t Trlim_t;

    class Trlimit {
    public:
        static const size_t Size_All = 16;
        typedef Trlim_t Type_rlim_cur;
        static const size_t Offs_rlim_cur = 0;
        typedef Trlim_t Type_rlim_max;
        static const size_t Offs_rlim_max = 8;
    };

    static const Tint VRLIMIT_AS = 0x00000006;
    static const Tint VRLIMIT_CORE = 0x00000004;
    static const Tint VRLIMIT_CPU = 0x00000000;
    static const Tint VRLIMIT_DATA = 0x00000002;
    static const Tint VRLIMIT_FSIZE = 0x00000001;
    static const Tint VRLIMIT_LOCKS = 0x0000000a;
    static const Tint VRLIMIT_MEMLOCK = 0x00000009;
    static const Tint VRLIMIT_NOFILE = 0x00000005;
    static const Tint VRLIMIT_NPROC = 0x00000008;
    static const Tint VRLIMIT_RSS = 0x00000007;
    static const Tint VRLIMIT_STACK = 0x00000003;
    static const Trlim_t VRLIM_INFINITY = ~Trlim_t(0x0000000000000000);

    class Trusage {
    public:
        static const size_t Size_All = 144;
        typedef Ttimeval Type_ru_utime;
        static const size_t Offs_ru_utime = 0;
        typedef Ttimeval Type_ru_stime;
        static const size_t Offs_ru_stime = 16;
        typedef Tlong Type_ru_maxrss;
        static const size_t Offs_ru_maxrss = 32;
        typedef Tlong Type_ru_ixrss;
        static const size_t Offs_ru_ixrss = 40;
        typedef Tlong Type_ru_idrss;
        static const size_t Offs_ru_idrss = 48;
        typedef Tlong Type_ru_isrss;
        static const size_t Offs_ru_isrss = 56;
        typedef Tlong Type_ru_minflt;
        static const size_t Offs_ru_minflt = 64;
        typedef Tlong Type_ru_majflt;
        static const size_t Offs_ru_majflt = 72;
        typedef Tlong Type_ru_nswap;
        static const size_t Offs_ru_nswap = 80;
        typedef Tlong Type_ru_inblock;
        static const size_t Offs_ru_inblock = 88;
        typedef Tlong Type_ru_oublock;
        static const size_t Offs_ru_oublock = 96;
        typedef Tlong Type_ru_msgsnd;
        static const size_t Offs_ru_msgsnd = 104;
        typedef Tlong Type_ru_msgrcv;
        static const size_t Offs_ru_msgrcv = 112;
        typedef Tlong Type_ru_nsignals;
        static const size_t Offs_ru_nsignals = 120;
        typedef Tlong Type_ru_nvcsw;
        static const size_t Offs_ru_nvcsw = 128;
        typedef Tlong Type_ru_nivcsw;
        static const size_t Offs_ru_nivcsw = 136;
    };

    static const Tint VRUSAGE_SELF = 0x00000000;
    static const Tint VRUSAGE_CHILDREN = ~Tint(0x00000000);

    class Tutsname {
    public:
        static const size_t Size_All = 390;
        typedef Tchar Type_sysname;
        static const size_t Offs_sysname = 0;
        static const size_t Step_sysname = 1;
        static const size_t Size_sysname = 65;
        typedef Tchar Type_nodename;
        static const size_t Offs_nodename = 65;
        static const size_t Step_nodename = 1;
        static const size_t Size_nodename = 65;
        typedef Tchar Type_release;
        static const size_t Offs_release = 130;
        static const size_t Step_release = 1;
        static const size_t Size_release = 65;
        typedef Tchar Type_version;
        static const size_t Offs_version = 195;
        static const size_t Step_version = 1;
        static const size_t Size_version = 65;
        typedef Tchar Type_machine;
        static const size_t Offs_machine = 260;
        static const size_t Step_machine = 1;
        static const size_t Size_machine = 65;
    };

    class T__sysctl_args {
    public:
        static const size_t Size_All = 80;
        typedef Tpointer_t Type_name;
        static const size_t Offs_name = 0;
        typedef Tint Type_nlen;
        static const size_t Offs_nlen = 8;
        typedef Tpointer_t Type_oldval;
        static const size_t Offs_oldval = 16;
        typedef Tpointer_t Type_oldlenp;
        static const size_t Offs_oldlenp = 24;
        typedef Tpointer_t Type_newval;
        static const size_t Offs_newval = 32;
        typedef Tsize_t Type_newlen;
        static const size_t Offs_newlen = 40;
    };

    static const Tint VCTL_KERN = 0x00000001;
    static const Tint VKERN_VERSION = 0x00000004;

    class Tsigset_t {
    public:
        static const size_t Size_All = 16;
        typedef Tulong Type_sig;
        static const size_t Offs_sig = 0;
        static const size_t Step_sig = 8;
        static const size_t Size_sig = 2;
    };

    class Tsigaction {
    public:
        static const size_t Size_All = 32;
        typedef Tuint Type_sa_flags;
        static const size_t Offs_sa_flags = 0;
        typedef Tpointer_t Type_sa_handler;
        static const size_t Offs_sa_handler = 8;
        typedef Tsigset_t Type_sa_mask;
        static const size_t Offs_sa_mask = 16;
    };

    // curl --silent https://raw.githubusercontent.com/hrw/syscalls-table/master/tables/syscalls-mips64 | awk '/\s[0-9]/ { print "\tstatic const Tint V__NR_" $1, "=", $2 ";" }'
    static const Tint V__NR__newselect = 5022;
    static const Tint V__NR__sysctl = 5152;
    static const Tint V__NR_accept = 5042;
    static const Tint V__NR_accept4 = 5293;
    static const Tint V__NR_access = 5020;
    static const Tint V__NR_acct = 5158;
    static const Tint V__NR_add_key = 5239;
    static const Tint V__NR_adjtimex = 5154;
    static const Tint V__NR_alarm = 5037;
    static const Tint V__NR_bind = 5048;
    static const Tint V__NR_bpf = 5315;
    static const Tint V__NR_brk = 5012;
    static const Tint V__NR_cachectl = 5198;
    static const Tint V__NR_cacheflush = 5197;
    static const Tint V__NR_capget = 5123;
    static const Tint V__NR_capset = 5124;
    static const Tint V__NR_chdir = 5078;
    static const Tint V__NR_chmod = 5088;
    static const Tint V__NR_chown = 5090;
    static const Tint V__NR_chroot = 5156;
    static const Tint V__NR_clock_adjtime = 5300;
    static const Tint V__NR_clock_getres = 5223;
    static const Tint V__NR_clock_gettime = 5222;
    static const Tint V__NR_clock_nanosleep = 5224;
    static const Tint V__NR_clock_settime = 5221;
    static const Tint V__NR_clone = 5055;
    static const Tint V__NR_clone3 = 5435;
    static const Tint V__NR_close = 5003;
    static const Tint V__NR_close_range = 5436;
    static const Tint V__NR_connect = 5041;
    static const Tint V__NR_copy_file_range = 5320;
    static const Tint V__NR_creat = 5083;
    static const Tint V__NR_create_module = 5167;
    static const Tint V__NR_delete_module = 5169;
    static const Tint V__NR_dup = 5031;
    static const Tint V__NR_dup2 = 5032;
    static const Tint V__NR_dup3 = 5286;
    static const Tint V__NR_epoll_create = 5207;
    static const Tint V__NR_epoll_create1 = 5285;
    static const Tint V__NR_epoll_ctl = 5208;
    static const Tint V__NR_epoll_pwait = 5272;
    static const Tint V__NR_epoll_pwait2 = 5441;
    static const Tint V__NR_epoll_wait = 5209;
    static const Tint V__NR_eventfd = 5278;
    static const Tint V__NR_eventfd2 = 5284;
    static const Tint V__NR_execve = 5057;
    static const Tint V__NR_execveat = 5316;
    static const Tint V__NR_exit = 5058;
    static const Tint V__NR_exit_group = 5205;
    static const Tint V__NR_faccessat = 5259;
    static const Tint V__NR_faccessat2 = 5439;
    static const Tint V__NR_fadvise64 = 5215;
    static const Tint V__NR_fallocate = 5279;
    static const Tint V__NR_fanotify_init = 5295;
    static const Tint V__NR_fanotify_mark = 5296;
    static const Tint V__NR_fchdir = 5079;
    static const Tint V__NR_fchmod = 5089;
    static const Tint V__NR_fchmodat = 5258;
    static const Tint V__NR_fchown = 5091;
    static const Tint V__NR_fchownat = 5250;
    static const Tint V__NR_fcntl = 5070;
    static const Tint V__NR_fdatasync = 5073;
    static const Tint V__NR_fgetxattr = 5185;
    static const Tint V__NR_finit_module = 5307;
    static const Tint V__NR_flistxattr = 5188;
    static const Tint V__NR_flock = 5071;
    static const Tint V__NR_fork = 5056;
    static const Tint V__NR_fremovexattr = 5191;
    static const Tint V__NR_fsconfig = 5431;
    static const Tint V__NR_fsetxattr = 5182;
    static const Tint V__NR_fsmount = 5432;
    static const Tint V__NR_fsopen = 5430;
    static const Tint V__NR_fspick = 5433;
    static const Tint V__NR_fstat = 5005;
    static const Tint V__NR_fstatfs = 5135;
    static const Tint V__NR_fsync = 5072;
    static const Tint V__NR_ftruncate = 5075;
    static const Tint V__NR_futex = 5194;
    static const Tint V__NR_futimesat = 5251;
    static const Tint V__NR_get_kernel_syms = 5170;
    static const Tint V__NR_get_mempolicy = 5228;
    static const Tint V__NR_get_robust_list = 5269;
    static const Tint V__NR_getcpu = 5271;
    static const Tint V__NR_getcwd = 5077;
    static const Tint V__NR_getdents = 5076;
    static const Tint V__NR_getdents64 = 5308;
    static const Tint V__NR_getegid = 5106;
    static const Tint V__NR_geteuid = 5105;
    static const Tint V__NR_getgid = 5102;
    static const Tint V__NR_getgroups = 5113;
    static const Tint V__NR_getitimer = 5035;
    static const Tint V__NR_getpeername = 5051;
    static const Tint V__NR_getpgid = 5119;
    static const Tint V__NR_getpgrp = 5109;
    static const Tint V__NR_getpid = 5038;
    static const Tint V__NR_getpmsg = 5174;
    static const Tint V__NR_getppid = 5108;
    static const Tint V__NR_getpriority = 5137;
    static const Tint V__NR_getrandom = 5313;
    static const Tint V__NR_getresgid = 5118;
    static const Tint V__NR_getresuid = 5116;
    static const Tint V__NR_getrlimit = 5095;
    static const Tint V__NR_getrusage = 5096;
    static const Tint V__NR_getsid = 5122;
    static const Tint V__NR_getsockname = 5050;
    static const Tint V__NR_getsockopt = 5054;
    static const Tint V__NR_gettid = 5178;
    static const Tint V__NR_gettimeofday = 5094;
    static const Tint V__NR_getuid = 5100;
    static const Tint V__NR_getxattr = 5183;
    static const Tint V__NR_init_module = 5168;
    static const Tint V__NR_inotify_add_watch = 5244;
    static const Tint V__NR_inotify_init = 5243;
    static const Tint V__NR_inotify_init1 = 5288;
    static const Tint V__NR_inotify_rm_watch = 5245;
    static const Tint V__NR_io_cancel = 5204;
    static const Tint V__NR_io_destroy = 5201;
    static const Tint V__NR_io_getevents = 5202;
    static const Tint V__NR_io_pgetevents = 5328;
    static const Tint V__NR_io_setup = 5200;
    static const Tint V__NR_io_submit = 5203;
    static const Tint V__NR_io_uring_enter = 5426;
    static const Tint V__NR_io_uring_register = 5427;
    static const Tint V__NR_io_uring_setup = 5425;
    static const Tint V__NR_ioctl = 5015;
    static const Tint V__NR_ioprio_get = 5274;
    static const Tint V__NR_ioprio_set = 5273;
    static const Tint V__NR_kcmp = 5306;
    static const Tint V__NR_kexec_load = 5270;
    static const Tint V__NR_keyctl = 5241;
    static const Tint V__NR_kill = 5060;
    static const Tint V__NR_landlock_add_rule = 5445;
    static const Tint V__NR_landlock_create_ruleset = 5444;
    static const Tint V__NR_landlock_restrict_self = 5446;
    static const Tint V__NR_lchown = 5092;
    static const Tint V__NR_lgetxattr = 5184;
    static const Tint V__NR_link = 5084;
    static const Tint V__NR_linkat = 5255;
    static const Tint V__NR_listen = 5049;
    static const Tint V__NR_listxattr = 5186;
    static const Tint V__NR_llistxattr = 5187;
    static const Tint V__NR_lookup_dcookie = 5206;
    static const Tint V__NR_lremovexattr = 5190;
    static const Tint V__NR_lseek = 5008;
    static const Tint V__NR_lsetxattr = 5181;
    static const Tint V__NR_lstat = 5006;
    static const Tint V__NR_madvise = 5027;
    static const Tint V__NR_mbind = 5227;
    static const Tint V__NR_membarrier = 5318;
    static const Tint V__NR_memfd_create = 5314;
    static const Tint V__NR_migrate_pages = 5246;
    static const Tint V__NR_mincore = 5026;
    static const Tint V__NR_mkdir = 5081;
    static const Tint V__NR_mkdirat = 5248;
    static const Tint V__NR_mknod = 5131;
    static const Tint V__NR_mknodat = 5249;
    static const Tint V__NR_mlock = 5146;
    static const Tint V__NR_mlock2 = 5319;
    static const Tint V__NR_mlockall = 5148;
    static const Tint V__NR_mmap = 5009;
    static const Tint V__NR_mount = 5160;
    static const Tint V__NR_mount_setattr = 5442;
    static const Tint V__NR_move_mount = 5429;
    static const Tint V__NR_move_pages = 5267;
    static const Tint V__NR_mprotect = 5010;
    static const Tint V__NR_mq_getsetattr = 5235;
    static const Tint V__NR_mq_notify = 5234;
    static const Tint V__NR_mq_open = 5230;
    static const Tint V__NR_mq_timedreceive = 5233;
    static const Tint V__NR_mq_timedsend = 5232;
    static const Tint V__NR_mq_unlink = 5231;
    static const Tint V__NR_mremap = 5024;
    static const Tint V__NR_msgctl = 5069;
    static const Tint V__NR_msgget = 5066;
    static const Tint V__NR_msgrcv = 5068;
    static const Tint V__NR_msgsnd = 5067;
    static const Tint V__NR_msync = 5025;
    static const Tint V__NR_munlock = 5147;
    static const Tint V__NR_munlockall = 5149;
    static const Tint V__NR_munmap = 5011;
    static const Tint V__NR_name_to_handle_at = 5298;
    static const Tint V__NR_nanosleep = 5034;
    static const Tint V__NR_newfstatat = 5252;
    static const Tint V__NR_nfsservctl = 5173;
    static const Tint V__NR_open = 5002;
    static const Tint V__NR_open_by_handle_at = 5299;
    static const Tint V__NR_open_tree = 5428;
    static const Tint V__NR_openat = 5247;
    static const Tint V__NR_openat2 = 5437;
    static const Tint V__NR_pause = 5033;
    static const Tint V__NR_perf_event_open = 5292;
    static const Tint V__NR_personality = 5132;
    static const Tint V__NR_pidfd_getfd = 5438;
    static const Tint V__NR_pidfd_open = 5434;
    static const Tint V__NR_pidfd_send_signal = 5424;
    static const Tint V__NR_pipe = 5021;
    static const Tint V__NR_pipe2 = 5287;
    static const Tint V__NR_pivot_root = 5151;
    static const Tint V__NR_pkey_alloc = 5324;
    static const Tint V__NR_pkey_free = 5325;
    static const Tint V__NR_pkey_mprotect = 5323;
    static const Tint V__NR_poll = 5007;
    static const Tint V__NR_ppoll = 5261;
    static const Tint V__NR_prctl = 5153;
    static const Tint V__NR_pread64 = 5016;
    static const Tint V__NR_preadv = 5289;
    static const Tint V__NR_preadv2 = 5321;
    static const Tint V__NR_prlimit64 = 5297;
    static const Tint V__NR_process_madvise = 5440;
    static const Tint V__NR_process_vm_readv = 5304;
    static const Tint V__NR_process_vm_writev = 5305;
    static const Tint V__NR_pselect6 = 5260;
    static const Tint V__NR_ptrace = 5099;
    static const Tint V__NR_pwrite64 = 5017;
    static const Tint V__NR_pwritev = 5290;
    static const Tint V__NR_pwritev2 = 5322;
    static const Tint V__NR_query_module = 5171;
    static const Tint V__NR_quotactl = 5172;
    static const Tint V__NR_quotactl_fd = 5443;
    static const Tint V__NR_read = 5000;
    static const Tint V__NR_readahead = 5179;
    static const Tint V__NR_readlink = 5087;
    static const Tint V__NR_readlinkat = 5257;
    static const Tint V__NR_readv = 5018;
    static const Tint V__NR_reboot = 5164;
    static const Tint V__NR_recvfrom = 5044;
    static const Tint V__NR_recvmmsg = 5294;
    static const Tint V__NR_recvmsg = 5046;
    static const Tint V__NR_remap_file_pages = 5210;
    static const Tint V__NR_removexattr = 5189;
    static const Tint V__NR_rename = 5080;
    static const Tint V__NR_renameat = 5254;
    static const Tint V__NR_renameat2 = 5311;
    static const Tint V__NR_request_key = 5240;
    static const Tint V__NR_restart_syscall = 5213;
    static const Tint V__NR_rmdir = 5082;
    static const Tint V__NR_rseq = 5327;
    static const Tint V__NR_rt_sigaction = 5013;
    static const Tint V__NR_rt_sigpending = 5125;
    static const Tint V__NR_rt_sigprocmask = 5014;
    static const Tint V__NR_rt_sigqueueinfo = 5127;
    static const Tint V__NR_rt_sigreturn = 5211;
    static const Tint V__NR_rt_sigsuspend = 5128;
    static const Tint V__NR_rt_sigtimedwait = 5126;
    static const Tint V__NR_rt_tgsigqueueinfo = 5291;
    static const Tint V__NR_sched_get_priority_max = 5143;
    static const Tint V__NR_sched_get_priority_min = 5144;
    static const Tint V__NR_sched_getaffinity = 5196;
    static const Tint V__NR_sched_getattr = 5310;
    static const Tint V__NR_sched_getparam = 5140;
    static const Tint V__NR_sched_getscheduler = 5142;
    static const Tint V__NR_sched_rr_get_interval = 5145;
    static const Tint V__NR_sched_setaffinity = 5195;
    static const Tint V__NR_sched_setattr = 5309;
    static const Tint V__NR_sched_setparam = 5139;
    static const Tint V__NR_sched_setscheduler = 5141;
    static const Tint V__NR_sched_yield = 5023;
    static const Tint V__NR_seccomp = 5312;
    static const Tint V__NR_semctl = 5064;
    static const Tint V__NR_semget = 5062;
    static const Tint V__NR_semop = 5063;
    static const Tint V__NR_semtimedop = 5214;
    static const Tint V__NR_sendfile = 5039;
    static const Tint V__NR_sendmmsg = 5302;
    static const Tint V__NR_sendmsg = 5045;
    static const Tint V__NR_sendto = 5043;
    static const Tint V__NR_set_mempolicy = 5229;
    static const Tint V__NR_set_robust_list = 5268;
    static const Tint V__NR_set_thread_area = 5242;
    static const Tint V__NR_set_tid_address = 5212;
    static const Tint V__NR_setdomainname = 5166;
    static const Tint V__NR_setfsgid = 5121;
    static const Tint V__NR_setfsuid = 5120;
    static const Tint V__NR_setgid = 5104;
    static const Tint V__NR_setgroups = 5114;
    static const Tint V__NR_sethostname = 5165;
    static const Tint V__NR_setitimer = 5036;
    static const Tint V__NR_setns = 5303;
    static const Tint V__NR_setpgid = 5107;
    static const Tint V__NR_setpriority = 5138;
    static const Tint V__NR_setregid = 5112;
    static const Tint V__NR_setresgid = 5117;
    static const Tint V__NR_setresuid = 5115;
    static const Tint V__NR_setreuid = 5111;
    static const Tint V__NR_setrlimit = 5155;
    static const Tint V__NR_setsid = 5110;
    static const Tint V__NR_setsockopt = 5053;
    static const Tint V__NR_settimeofday = 5159;
    static const Tint V__NR_setuid = 5103;
    static const Tint V__NR_setxattr = 5180;
    static const Tint V__NR_shmat = 5029;
    static const Tint V__NR_shmctl = 5030;
    static const Tint V__NR_shmdt = 5065;
    static const Tint V__NR_shmget = 5028;
    static const Tint V__NR_shutdown = 5047;
    static const Tint V__NR_sigaltstack = 5129;
    static const Tint V__NR_signalfd = 5276;
    static const Tint V__NR_signalfd4 = 5283;
    static const Tint V__NR_socket = 5040;
    static const Tint V__NR_socketpair = 5052;
    static const Tint V__NR_splice = 5263;
    static const Tint V__NR_stat = 5004;
    static const Tint V__NR_statfs = 5134;
    static const Tint V__NR_statx = 5326;
    static const Tint V__NR_swapoff = 5163;
    static const Tint V__NR_swapon = 5162;
    static const Tint V__NR_symlink = 5086;
    static const Tint V__NR_symlinkat = 5256;
    static const Tint V__NR_sync = 5157;
    static const Tint V__NR_sync_file_range = 5264;
    static const Tint V__NR_syncfs = 5301;
    static const Tint V__NR_sysfs = 5136;
    static const Tint V__NR_sysinfo = 5097;
    static const Tint V__NR_syslog = 5101;
    static const Tint V__NR_sysmips = 5199;
    static const Tint V__NR_tee = 5265;
    static const Tint V__NR_tgkill = 5225;
    static const Tint V__NR_timer_create = 5216;
    static const Tint V__NR_timer_delete = 5220;
    static const Tint V__NR_timer_getoverrun = 5219;
    static const Tint V__NR_timer_gettime = 5218;
    static const Tint V__NR_timer_settime = 5217;
    static const Tint V__NR_timerfd = 5277;
    static const Tint V__NR_timerfd_create = 5280;
    static const Tint V__NR_timerfd_gettime = 5281;
    static const Tint V__NR_timerfd_settime = 5282;
    static const Tint V__NR_times = 5098;
    static const Tint V__NR_tkill = 5192;
    static const Tint V__NR_truncate = 5074;
    static const Tint V__NR_umask = 5093;
    static const Tint V__NR_umount2 = 5161;
    static const Tint V__NR_uname = 5061;
    static const Tint V__NR_unlink = 5085;
    static const Tint V__NR_unlinkat = 5253;
    static const Tint V__NR_unshare = 5262;
    static const Tint V__NR_userfaultfd = 5317;
    static const Tint V__NR_ustat = 5133;
    static const Tint V__NR_utime = 5130;
    static const Tint V__NR_utimensat = 5275;
    static const Tint V__NR_utimes = 5226;
    static const Tint V__NR_vhangup = 5150;
    static const Tint V__NR_vmsplice = 5266;
    static const Tint V__NR_wait4 = 5059;
    static const Tint V__NR_waitid = 5237;
    static const Tint V__NR_write = 5001;
    static const Tint V__NR_writev = 5019;
};
